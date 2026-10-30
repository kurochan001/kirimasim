// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Delayed scalar-reference NBA capture and validity checks.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/nba.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace front = kirimasim::front;
namespace k = kirimasim::kernel;
namespace vm = kirimasim::vm;

struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-delayed-ref-nba-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) return;
        directory = pattern;
        const auto path = directory / "design.sv";
        std::ofstream stream(path);
        stream << text;
        stream.close();
        if (stream) file = std::filesystem::relative(std::filesystem::canonical(path)).string();
    }
    ~Source() {
        if (directory.empty()) return;
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file != nullptr) std::fclose(file);
    }
    std::string text() const {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string out;
        std::array<char, 512> buffer{};
        while (const auto count = std::fread(buffer.data(), 1, buffer.size(), file))
            out.append(buffer.data(), count);
        return out;
    }
};
front::CompileResult compileDetached(std::string_view text) {
    const Source source(text);
    EXPECT_FALSE(source.file.empty());
    if (source.file.empty()) return {};
    return front::compile(std::vector<std::string>{source.file});
}
struct Execution {
    vm::RunResult result;
    std::string output, diagnostics;
    std::uint64_t time = 0;
    std::size_t events = 0, allocatedEvents = 0, scopes = 0, leases = 0, controls = 0, payloads = 0;
};
Execution execute(const kirimasim::sir::Design& design) {
    Capture out, err;
    EXPECT_NE(out.file, nullptr);
    EXPECT_NE(err.file, nullptr);
    if (out.file == nullptr || err.file == nullptr)
        return {{k::EndReason::Internal, "cannot capture output"}, {}, {}};
    k::Context context(k::OutputDestinations{out.file, err.file});
    auto result = vm::run(design, context);
    context.output().flush();
    const auto scopes = context.activations().stats();
    return {std::move(result),
            out.text(),
            err.text(),
            context.scheduler().time(),
            context.managedEvents().liveObjects(),
            context.managedEvents().allocatedObjects(),
            scopes.live,
            scopes.leases,
            context.scheduler().nbaEventStats().live,
            context.scheduler().nbaStorageStats().live};
}
void success(const Execution& result) {
    EXPECT_EQ(result.result.reason, k::EndReason::Quiescent)
        << result.result.error << result.diagnostics;
    EXPECT_TRUE(result.result.error.empty()) << result.result.error;
    EXPECT_TRUE(result.diagnostics.empty()) << result.diagnostics;
    EXPECT_EQ(result.events, 0U);
    EXPECT_EQ(result.scopes, 0U);
    EXPECT_EQ(result.leases, 0U);
    EXPECT_EQ(result.controls, 0U);
    EXPECT_EQ(result.payloads, 0U);
}

namespace s = kirimasim::sir;

template <class F> void forDelayedRef(s::Design& design, F&& visit) {
    const auto body = [&](auto& value) {
        for (auto& block : value.blocks)
            for (auto& instruction : block.instrs)
                if (instruction.op == s::Op::NbaRefDelay) visit(instruction);
    };
    for (auto& process : design.processes)
        body(process);
    for (auto& subroutine : design.subroutines)
        body(subroutine);
}
s::Operand addConstant(s::Design& design, s::Type type, std::vector<std::uint64_t> a,
                       std::vector<std::uint64_t> b = {}) {
    const auto id = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
    design.constants.push_back({type, std::move(a), std::move(b)});
    return s::Operand::constant(id);
}
constexpr std::string_view scalarSource = R"sv(module top;
int value;
function automatic void issue(ref static int r); r<=#2 7; endfunction
initial begin issue(value); #3; $display("%0d",value); end
endmodule
)sv";

TEST(DelayedRefNba, CapturedWideActualSurvivesReturnedCallsAndSelectorChanges) {
    const auto compiled = compileDetached(R"sv(module top;
logic[128:0] values[2]; int index;
task automatic inner(ref static logic[128:0] r);
  logic[128:0] data='z;
  r<=#2 data; data='0;
endtask
task automatic outer(ref static logic[128:0] r); inner(r); endtask
initial begin
  values='{default:'0}; index=1;
  outer(values[index--]); index=0;
  wait fork;
  if($time!=0) $fatal(1,"not a child");
  #1; if(values[1]!==129'b0) $fatal(1,"not immediate");
  #2;
  if(values[1]!=={129{1'bz}} || values[0]!==129'b0) $fatal(1,"snapshot destination");
  $display("passed");
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    EXPECT_STREQ(s::opName(s::Op::NbaRefDelay), "nba.ref.delay");
    EXPECT_NE(s::toText(compiled.design).find("nba.ref.delay "), std::string::npos);
    for (int repetition = 0; repetition < 2; ++repetition) {
        const auto result = execute(compiled.design);
        success(result);
        EXPECT_EQ(result.output, "passed\n");
        EXPECT_EQ(result.time, 3U);
    }
}

TEST(DelayedRefNba, ContainerSnapshotsRemainRootsAcrossReturnAndCollection) {
    const auto compiled = compileDetached(R"sv(module top;
typedef int Q[$]; typedef int D[];
Q queue_value; D array_value;
task automatic issue_queue(ref static Q r);
  Q data={7,11}; r<=#2 data; data[0]=99;
endtask
task automatic issue_array(ref static D r);
  D data=new[2]; data[0]=13;data[1]=17;r<=#2 data;data[0]=101;
endtask
task automatic churn(); Q work={31,37};work.push_back(41);endtask
initial begin
  issue_queue(queue_value); issue_array(array_value);
  for(int n=0;n<4000;n++) churn();
  #3;
  queue_value.push_back(19);
  if(queue_value.size()!=3 || queue_value[2]!=19 || array_value.size()!=2) $fatal(1,"owned values");
  $display("%0d:%0d:%0d:%0d",queue_value[0],queue_value[1],array_value[0],array_value[1]);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "7:11:13:17\n");
}

TEST(DelayedRefNba, FrameHeapAndReadonlyActualsAreRejectedAtCapture) {
    for (unsigned variant = 0; variant < 4; ++variant) {
        const std::string formal = variant == 2 ? "const ref" : "ref";
        const std::string storage = variant == 1 ? "int values[$]={0};" : "int value;";
        const std::string actual = variant == 1 ? "values[0]" : "value";
        const std::string automatic = variant == 0 ? "automatic int value;" : "";
        std::string source = "module top; ";
        source += storage;
        source += " function automatic void inspect(";
        source += formal;
        source += " int r); endfunction initial begin ";
        source += automatic;
        source += " inspect(";
        source += actual;
        source += "); #2; end endmodule\n";
        auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        auto& design = compiled.design;
        auto& sub = design.subroutines[0];
        const auto parameter = sub.params[0].frameVar;
        const auto reference = s::TempId{sub.numTemps++};
        const auto data = addConstant(design, s::s2(32), {7});
        const auto delay = addConstant(design, s::u2(64), {1});
        auto& instructions = sub.blocks[0].instrs;
        const auto at = instructions.end() - 1;
        instructions.insert(
            at, {{s::Op::Fload, s::refType(), reference, {s::Operand::frame(parameter)}, sub.loc},
                 {s::Op::NbaRefDelay,
                  s::voidType(),
                  s::kNone,
                  {s::Operand::temp(reference), data, delay},
                  sub.loc}});
        ASSERT_TRUE(s::verify(design).empty());
        const auto result = execute(design);
        if (variant == 3) {
            success(result);
        } else {
            EXPECT_EQ(result.result.reason, k::EndReason::Error) << variant;
            EXPECT_NE(result.diagnostics.find("must refer to a writable static variable"),
                      std::string::npos)
                << variant;
        }
        EXPECT_EQ(result.leases, 0U);
    }
}

TEST(DelayedRefNba, VerifierChecksArityValuesDelayAndConstantLeafBounds) {
    const auto compiled = compileDetached(scalarSource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (unsigned variant = 0; variant < 9; ++variant) {
        auto design = compiled.design;
        const auto negative = addConstant(design, s::s2(64), {UINT64_MAX});
        const auto tooLarge = addConstant(design, s::u2(64), {UINT64_C(4294967296)});
        const auto event = addConstant(design, {s::TypeKind::Event, 0}, {0});
        const auto stringId = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
        design.constants.push_back({{s::TypeKind::String, 0}, {}, {}, "not a delay"});
        const auto string = s::Operand::constant(stringId);
        forDelayedRef(design, [&](auto& instruction) {
            auto& ops = instruction.operands;
            if (variant == 0) ops.pop_back();
            if (variant == 1) {
                ops.push_back(ops.back());
                ops.push_back(ops.back());
            }
            if (variant == 2) ops[0] = ops[1];
            if (variant == 3) ops[1] = event;
            if (variant == 4) ops.back() = string;
            if (variant == 5) ops.insert(ops.begin() + 1, negative);
            if (variant == 6) ops.insert(ops.begin() + 1, tooLarge);
            if (variant == 7) ops.insert(ops.begin() + 1, ops[0]);
            if (variant == 8) instruction.type = s::u2(1);
        });
        EXPECT_FALSE(s::verify(design).empty()) << variant;
    }
}

TEST(DelayedRefNba, UnknownDelaysAreZeroAndUnrepresentableTimesAreDesignErrors) {
    const auto compiled = compileDetached(scalarSource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (unsigned variant = 0; variant < 3; ++variant) {
        auto design = compiled.design;
        const auto delay = addConstant(design, s::u4(129),
                                       variant == 2 ? std::vector<std::uint64_t>{0, 1, 0}
                                                    : std::vector<std::uint64_t>{0, 0, 0},
                                       variant == 1 ? std::vector<std::uint64_t>{0, 1, 0}
                                                    : std::vector<std::uint64_t>{0, 0, 0});
        forDelayedRef(design, [&](auto& instruction) { instruction.operands.back() = delay; });
        ASSERT_TRUE(s::verify(design).empty());
        const auto result = execute(design);
        if (variant != 2) {
            success(result);
            EXPECT_EQ(result.output, "7\n");
        } else {
            EXPECT_EQ(result.result.reason, k::EndReason::Error);
            EXPECT_NE(result.diagnostics.find("past the largest time"), std::string::npos);
        }
    }
}

TEST(DelayedRefNba, PostponedCallbacksCannotScheduleAReferenceUpdate) {
    auto compiled = compileDetached(scalarSource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto& design = compiled.design;
    s::SlotId target = s::kNone;
    for (std::uint32_t i = 0; i < design.slots.size(); ++i)
        if (design.slots[i].name == "top.value") target = s::SlotId{i};
    ASSERT_NE(target, s::kNone);
    const auto data = addConstant(design, s::s2(32), {7});
    const auto delay = addConstant(design, s::u2(64), {1});
    s::Subroutine callback;
    callback.name = "top.native_postponed_ref";
    callback.loc = {0, 3, 1};
    callback.automatic = true;
    callback.numTemps = 1;
    callback.blocks.resize(1);
    callback.blocks[0].instrs = {
        {s::Op::RefMake, s::refType(), s::TempId{0}, {s::Operand::slot(target)}, callback.loc},
        {s::Op::NbaRefDelay,
         s::voidType(),
         s::kNone,
         {s::Operand::temp(s::TempId{0}), data, delay},
         callback.loc},
        {s::Op::Ret, s::voidType(), s::kNone, {}, callback.loc}};
    const auto id = s::SubroutineId{static_cast<std::uint32_t>(design.subroutines.size())};
    design.subroutines.push_back(std::move(callback));
    for (auto& process : compiled.design.processes)
        for (auto& block : process.blocks)
            for (auto& instruction : block.instrs)
                if (instruction.op == s::Op::Call) {
                    instruction.op = s::Op::ReportCall;
                    instruction.operands = {s::Operand::str("final"), s::Operand::subroutine(id)};
                }
    const auto errors = s::verify(compiled.design);
    for (const auto& error : errors)
        ADD_FAILURE() << error.where << ": " << error.message;
    ASSERT_TRUE(errors.empty());
    const auto result = execute(compiled.design);
    EXPECT_EQ(result.result.reason, k::EndReason::Error);
    EXPECT_NE(result.diagnostics.find("a write or an event in the Postponed region"),
              std::string::npos);
}

TEST(DelayedRefNba, StaticDestinationTypeAndLeafAreValidatedBeforeScheduling) {
    const auto compiled = compileDetached(scalarSource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (unsigned variant = 0; variant < 2; ++variant) {
        auto design = compiled.design;
        const auto wrong = addConstant(design, s::u4(32), {7}, {0});
        const auto leaf = addConstant(design, s::u2(32), {UINT32_MAX});
        forDelayedRef(design, [&](auto& instruction) {
            if (variant == 0)
                instruction.operands[1] = wrong;
            else
                instruction.operands.insert(instruction.operands.begin() + 1, leaf);
        });
        ASSERT_TRUE(s::verify(design).empty());
        const auto result = execute(design);
        EXPECT_EQ(result.result.reason, k::EndReason::Error);
        EXPECT_NE(result.diagnostics.find(variant == 0 ? "differs from its value type"
                                                       : "leaf is out of range"),
                  std::string::npos);
    }
}
}  // namespace
