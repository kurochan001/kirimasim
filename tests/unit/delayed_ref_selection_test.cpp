// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Delayed fixed-array and bounded packed reference NBA checks.
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
#include "kirimasim/valsem/value.h"
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
Execution execute(const kirimasim::sir::Design& design, const vm::RunOptions& options = {}) {
    Capture out, err;
    EXPECT_NE(out.file, nullptr);
    EXPECT_NE(err.file, nullptr);
    if (out.file == nullptr || err.file == nullptr)
        return {{k::EndReason::Internal, "cannot capture output"}, {}, {}};
    k::Context context(k::OutputDestinations{out.file, err.file});
    auto result = vm::run(design, context, options);
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

template <class F> void forSelected(s::Design& design, F&& visit) {
    for (auto& sub : design.subroutines)
        for (auto& block : sub.blocks)
            for (auto& instruction : block.instrs)
                if (instruction.op == s::Op::NbaRefElemDelay ||
                    instruction.op == s::Op::NbaRefBitsDelay)
                    visit(instruction, sub);
}
s::Operand addConstant(s::Design& design, s::Type type, std::vector<std::uint64_t> words,
                       std::vector<std::uint64_t> unknown = {}) {
    const auto id = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
    design.constants.push_back({type, std::move(words), std::move(unknown)});
    return s::Operand::constant(id);
}
constexpr std::string_view elementSource = R"sv(module top;
int values[2];
function automatic void issue(ref static int r[2]);r[1]<=#2 7;endfunction
initial begin issue(values);#3;$display("%0d",values[1]);end
endmodule
)sv";
constexpr std::string_view bitsSource = R"sv(module top;
logic[1:0][7:0] values[2];
function automatic void issue(ref static logic[1:0][7:0] r[2]);
  int position=3;r[1][1][position+:3]<=#2 3'h5;
endfunction
initial begin values='{default:'0};issue(values);#3;$display("%h",values[1]);end
endmodule
)sv";

TEST(DelayedRefSelection, BoundedPackedWritesMatchValsem) {
    namespace v = kirimasim::valsem;
    const auto before =
        v::fromWords({0x12345678, 0xfedcba98, 0x55555555, 0xaaaaaaaa, 1},
                     {0x11111111, 0x22222222, 0x33333333, 0x44444444, 2}, 258, false);
    const auto data = v::fromWords({0x1a5b}, {0x1555}, 13, false);
    std::string source = "module top;logic[1:0][128:0] value=258'b";
    source += v::toBinary(before);
    source += ";function automatic void issue(ref static logic[1:0][128:0] r);";
    source += "logic signed[128:0] position=0;r[1][position+:13]<=#2 13'b";
    source += v::toBinary(data);
    source += ";endfunction initial begin issue(value);#3;$display(\"%b\",value);end endmodule\n";
    const auto compiled = compileDetached(source);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    std::size_t comparisons = 0;
    for (const auto [low, width] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
             {0, 1}, {1, 8}, {63, 65}, {64, 129}, {129, 129}, {257, 1}, {0, 0}}) {
        std::vector<v::Value> positions;
        for (const auto position :
             {INT64_C(-14), INT64_C(-1), INT64_C(0), INT64_C(1), INT64_C(63), INT64_C(64),
              INT64_C(128), INT64_C(129), INT64_C(259), INT64_MIN, INT64_MAX}) {
            std::vector<std::uint64_t> words(3, position < 0 ? UINT64_MAX : 0);
            words[0] = static_cast<std::uint64_t>(position);
            positions.push_back(v::fromWords(words, {}, 129, true));
        }
        positions.push_back(v::filled(v::Logic::X, 129, true));
        positions.push_back(v::filled(v::Logic::Z, 129, true));
        for (const auto& position : positions) {
            auto design = compiled.design;
            s::Constant constant;
            constant.type = s::s4(129);
            v::toWords(position, constant.aWords, constant.bWords);
            const auto id = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
            design.constants.push_back(std::move(constant));
            std::size_t replacements = 0;
            forSelected(design, [&](auto& instruction, auto&) {
                instruction.operands[0].lsb = low;
                instruction.operands[0].width = width;
                instruction.operands[1] = s::Operand::constant(id);
                ++replacements;
            });
            ASSERT_EQ(replacements, 1U);
            ASSERT_TRUE(s::verify(design).empty());
            const auto parent = v::bits(before, low, width == 0 ? before.width() : width);
            const auto changed = v::writeBits(parent, position, data);
            const auto expected = v::writeBits(before, v::fromUint(low, 64, false), changed);
            const auto result = execute(design);
            success(result);
            EXPECT_EQ(result.output, v::toBinary(expected) + "\n")
                << low << "+" << width << " at " << v::toBinary(position);
            EXPECT_EQ(result.time, 3U);
            ++comparisons;
        }
    }
    EXPECT_EQ(comparisons, 91U);
}

TEST(DelayedRefSelection, ReturnedSubarrayRefsPreserveCurrentNeighbours) {
    const auto compiled = compileDetached(R"sv(module top;
typedef logic[1:0][64:0] T;T values[2][3:1];
task automatic inner(ref static T r[-1:1]);
  int index=1,position=-2;logic[12:0] data=13'h1a5b;
  r[index][1][position+:13]<=#2 data;index=-1;position=0;data='0;disable inner;
endtask
task automatic outer(ref static T r[3:1]);inner(r);endtask
initial begin
  automatic logic[129:0] expected='z;automatic logic[12:0] data=13'h1a5b;
  values='{default:'{default:'1}};
  for(int n=2;n<13;n++) expected[65-2+n]=data[n];
  outer(values[1]);wait fork;if($time!=0) $fatal(1,"not a child");
  #1;values[1][1]='z;#2;
  if(values[1][1]!==expected || values[0][1]!=='1 || values[1][3]!=='1)
    $fatal(1,"captured range and current neighbours");
  $display("passed");
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (int i = 0; i < 2; ++i) {
        const auto result = execute(compiled.design);
        success(result);
        EXPECT_EQ(result.output, "passed\n");
        EXPECT_EQ(result.time, 3U);
    }
}

TEST(DelayedRefSelection, ContainerArrayPayloadsRemainRootsAfterReturn) {
    const auto compiled = compileDetached(R"sv(module top;
typedef int Q[$];typedef int D[];Q queues[2];D arrays[2];
task automatic putq(ref static Q r[2]);Q data={7,11};r[1]<=#2 data;data[0]=99;endtask
task automatic putd(ref static D r[2]);D data=new[2];data[0]=13;data[1]=17;r[1]<=#2 data;data[0]=101;endtask
task automatic churn();Q work={31,37};work.push_back(41);endtask
initial begin
  putq(queues);putd(arrays);for(int n=0;n<4000;n++) churn();#3;
  queues[1].push_back(19);
  if(queues[1].size()!=3 || queues[1][0]!=7 || queues[1][1]!=11 || queues[1][2]!=19 ||
     arrays[1].size()!=2 || arrays[1][0]!=13 || arrays[1][1]!=17 || queues[0].size()!=0)
    $fatal(1,"owned payloads");
  $display("passed");
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "passed\n");
}

TEST(DelayedRefSelection, VerifierChecksShapeWindowValueAndDelay) {
    for (const auto source : {elementSource, bitsSource}) {
        const auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        for (unsigned variant = 0; variant < 10; ++variant) {
            auto design = compiled.design;
            const auto wrong = addConstant(design, {s::TypeKind::Real, 0}, {0});
            const auto large = addConstant(design, s::u4(129), {1, 0, 0}, {0, 0, 0});
            forSelected(design, [&](auto& instruction, auto&) {
                auto& ops = instruction.operands;
                if (variant == 0) ops.pop_back();
                if (variant == 1) ops.push_back(ops.back());
                if (variant == 2) ops[0] = ops[1];
                if (variant == 3) ops[1] = wrong;
                if (variant == 4) ops.back() = wrong;
                if (variant == 5) ops[ops.size() - 2] = wrong;
                if (variant == 6) ops[0].lsb = UINT32_MAX;
                if (variant == 7) ops[0].width = UINT32_MAX;
                if (variant == 8) ops[ops.size() - 2] = large;
                if (variant == 9) instruction.type = s::u2(1);
            });
            EXPECT_FALSE(s::verify(design).empty()) << variant;
        }
    }
}

TEST(DelayedRefSelection, AutomaticAndReadonlyActualsAreRejected) {
    for (unsigned variant = 0; variant < 3; ++variant) {
        std::string source = "module top;int values[2];function automatic void inspect(";
        source += variant == 1 ? "const ref" : "ref";
        source += " int r[2]);endfunction initial begin ";
        if (variant == 0) source += "automatic int values[2];";
        source += "inspect(values);#2;end endmodule\n";
        auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        auto& design = compiled.design;
        const auto selected = addConstant(design, s::s2(32), {1});
        const auto data = addConstant(design, s::s2(32), {7});
        const auto delay = addConstant(design, s::u2(64), {1});
        auto& sub = design.subroutines[0];
        sub.blocks[0].instrs.insert(
            sub.blocks[0].instrs.end() - 1,
            {s::Op::NbaRefElemDelay,
             s::voidType(),
             s::kNone,
             {s::Operand::frame(sub.params[0].frameVar), selected, data, delay},
             sub.loc});
        ASSERT_TRUE(s::verify(design).empty());
        const auto result = execute(design);
        if (variant == 2) {
            success(result);
        } else {
            EXPECT_EQ(result.result.reason, k::EndReason::Error);
            EXPECT_NE(result.diagnostics.find("must refer to a writable static variable"),
                      std::string::npos);
        }
        EXPECT_EQ(result.leases, 0U);
    }
}

TEST(DelayedRefSelection, ActualShapeAndDelayOverflowAreChecked) {
    for (const auto source : {elementSource, bitsSource}) {
        const auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        for (unsigned variant = 0; variant < 4; ++variant) {
            auto design = compiled.design;
            const auto delay = addConstant(design, s::u4(129), {0, 1, 0}, {0, 0, 0});
            forSelected(design, [&](auto& instruction, auto& sub) {
                auto& formal = sub.frame[instruction.operands[0].asFrame().index()];
                if (variant == 0) {
                    formal.dims[0].right = 2;
                }
                if (variant == 1)
                    for (auto& slot : design.slots)
                        if (slot.name == "top.values")
                            slot.type = s::isFourState(formal.target) ? s::u2(formal.target.width)
                                                                      : s::u4(formal.target.width);
                if (variant == 2) instruction.operands.back() = delay;
                if (variant == 3) {
                    const auto zero = addConstant(design, s::u4(129), {0, 0, 0}, {0, 1, 0});
                    instruction.operands.back() = zero;
                }
            });
            if (variant < 2) {
                // VM execution checks actual bindings as well as verified formal metadata.
                const auto result = execute(design);
                EXPECT_EQ(result.result.reason, k::EndReason::Error);
                EXPECT_NE(result.diagnostics.find(variant == 0 ? "exceeds its static target"
                                                               : "differs from its declared leaf"),
                          std::string::npos);
            } else {
                ASSERT_TRUE(s::verify(design).empty());
                const auto result = execute(design);
                if (variant == 2) {
                    EXPECT_EQ(result.result.reason, k::EndReason::Error);
                    EXPECT_NE(result.diagnostics.find("past the largest time"), std::string::npos);
                } else {
                    success(result);
                    EXPECT_EQ(result.output, source == elementSource ? "7\n" : "2800\n");
                }
            }
        }
    }
}

TEST(DelayedRefSelection, BoundsWarningsUseTheParentAndDeduplicate) {
    const auto compiled = compileDetached(R"sv(module top;
logic[1:0][7:0] values[2];
task automatic issue(ref static logic[1:0][7:0] r[2],input int position);
  r[1][1][position+:3]<=#2 3'b0;
endtask
initial begin
  values='{default:'1};issue(values,9);issue(values,9);issue(values,7);#3;
  if(values[1]!==16'h7fff || values[0]!==16'hffff) $fatal(1,"parent clipping");
  $display("passed");
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    vm::RunOptions options;
    options.boundsCheck = true;
    const auto result = execute(compiled.design, options);
    EXPECT_EQ(result.result.reason, k::EndReason::Quiescent) << result.diagnostics;
    EXPECT_EQ(result.output, "passed\n");
    const auto first = result.diagnostics.find("(--bounds-check=warn)");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(result.diagnostics.find("(--bounds-check=warn)", first + 1), std::string::npos);
    EXPECT_EQ(result.leases, 0U);
    EXPECT_EQ(result.payloads, 0U);
}

TEST(DelayedRefSelection, PostponedCallbacksCannotScheduleSelectedWrites) {
    for (const auto source : {elementSource, bitsSource}) {
        auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        auto& design = compiled.design;
        s::SlotId target = s::kNone;
        for (std::uint32_t i = 0; i < design.slots.size(); ++i)
            if (design.slots[i].name == "top.values") target = s::SlotId{i};
        ASSERT_NE(target, s::kNone);
        auto callback = design.subroutines[0];
        const auto parameter = callback.params[0].frameVar;
        callback.params.clear();
        callback.name = "top.native_postponed_selected_ref";
        const auto reference = s::TempId{callback.numTemps++};
        callback.blocks[0].instrs.insert(
            callback.blocks[0].instrs.begin(),
            {{s::Op::RefMake, s::refType(), reference, {s::Operand::slot(target)}, callback.loc},
             {s::Op::Fstore,
              s::voidType(),
              s::kNone,
              {s::Operand::frame(parameter), s::Operand::temp(reference)},
              callback.loc}});
        const auto id = s::SubroutineId{static_cast<std::uint32_t>(design.subroutines.size())};
        design.subroutines.push_back(std::move(callback));
        for (auto& process : design.processes)
            for (auto& block : process.blocks)
                for (auto& instruction : block.instrs)
                    if (instruction.op == s::Op::Call) {
                        instruction.op = s::Op::ReportCall;
                        instruction.operands = {s::Operand::str("final"),
                                                s::Operand::subroutine(id)};
                    }
        const auto errors = s::verify(design);
        for (const auto& error : errors)
            ADD_FAILURE() << error.where << ": " << error.message;
        ASSERT_TRUE(errors.empty());
        const auto result = execute(design);
        EXPECT_EQ(result.result.reason, k::EndReason::Error);
        EXPECT_NE(result.diagnostics.find("a write or an event in the Postponed region"),
                  std::string::npos);
    }
}
}  // namespace
