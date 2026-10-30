// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Bounded packed-parent captures reuse the shared value semantics.
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
            (std::filesystem::temp_directory_path() / "kirimasim-nba-ref-window-XXXXXX").string();
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
namespace v = kirimasim::valsem;

TEST(EventNbaRefWindow, VmClipsCapturedWindowsAgainstTheValsemReference) {
    const auto before =
        v::fromWords({UINT64_C(0x123456789abcdef0), UINT64_C(0xfedcba9876543210),
                      UINT64_C(0x5555555555555555), UINT64_C(0xaaaaaaaaaaaaaaaa), 1},
                     {UINT64_C(0x1111111111111111), UINT64_C(0x2222222222222222),
                      UINT64_C(0x3333333333333333), UINT64_C(0x4444444444444444), 2},
                     258, false);
    const auto data = v::fromWords({0x1a5b}, {0x1555}, 13, false);
    const auto source =
        "module top; logic [1:0][128:0] value=258'b" + v::toBinary(before) +
        "; event go; task automatic issue(ref static logic [1:0][128:0] r); "
        "logic signed [128:0] position=0; r[1][position+:13]<=@(go) 13'b" +
        v::toBinary(data) +
        "; endtask initial begin issue(value); ->go; #1; $display(\"%b\",value); end endmodule\n";
    const auto compiled = compileDetached(source);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design.nbaLayouts.size(), 1U);
    const auto original = std::get<s::NbaRefBitsTarget>(compiled.design.nbaLayouts[0].targets[0]);
    EXPECT_EQ(original.windowLsb, 129U);
    EXPECT_EQ(original.windowWidth, 129U);
    EXPECT_NE(s::toText(compiled.design).find("parent=129+129"), std::string::npos);
    const auto type = compiled.design.nbaLayouts[0].values[original.position.index()];
    ASSERT_TRUE(s::isFourState(type));
    ASSERT_GE(type.width, 64U);
    std::size_t comparisons = 0;
    for (const auto [low, width] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
             {0, 1}, {1, 8}, {63, 65}, {64, 129}, {129, 129}, {257, 1}, {0, 0}}) {
        std::vector<v::Value> positions;
        for (const auto position :
             {INT64_C(-14), INT64_C(-1), INT64_C(0), INT64_C(1), INT64_C(63), INT64_C(64),
              INT64_C(128), INT64_C(129), INT64_C(259), INT64_MIN, INT64_MAX}) {
            std::vector<std::uint64_t> words((type.width + 63) / 64, position < 0 ? UINT64_MAX : 0);
            words[0] = static_cast<std::uint64_t>(position);
            positions.push_back(v::fromWords(words, {}, type.width, s::isSigned(type)));
        }
        positions.push_back(v::filled(v::Logic::X, type.width, s::isSigned(type)));
        positions.push_back(v::filled(v::Logic::Z, type.width, s::isSigned(type)));
        for (const auto& position : positions) {
            auto design = compiled.design;
            auto& target = std::get<s::NbaRefBitsTarget>(design.nbaLayouts[0].targets[0]);
            target.windowLsb = low;
            target.windowWidth = width;
            s::Constant constant;
            constant.type = type;
            v::toWords(position, constant.aWords, constant.bWords);
            const auto id = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
            design.constants.push_back(std::move(constant));
            std::size_t replacements = 0;
            const auto replace = [&](auto& body) {
                for (auto& block : body.blocks)
                    for (auto& instruction : block.instrs)
                        if (instruction.op == s::Op::NbaEvent) {
                            instruction.operands[2 + target.position.index()] =
                                s::Operand::constant(id);
                            ++replacements;
                        }
            };
            for (auto& process : design.processes)
                replace(process);
            for (auto& subroutine : design.subroutines)
                replace(subroutine);
            ASSERT_GT(replacements, 0U);
            ASSERT_TRUE(s::verify(design).empty());
            const auto parent = v::bits(before, low, width == 0 ? before.width() : width);
            const auto changed = v::writeBits(parent, position, data);
            const auto expected = v::writeBits(before, v::fromUint(low, 64, false), changed);
            const auto result = execute(design);
            success(result);
            EXPECT_EQ(result.output, v::toBinary(expected) + "\n")
                << low << "+" << width << " at " << v::toBinary(position);
            ++comparisons;
        }
    }
    EXPECT_EQ(comparisons, 91U);
}

TEST(EventNbaRefWindow, ReturnedArrayRefsRetainMasksAndPreserveCurrentNeighbourBits) {
    const auto compiled = compileDetached(R"sv(module top;
typedef logic [1:0][64:0] T;
T values[2][3:1]; event go;
task automatic inner(ref static T r[-1:1]);
  int index=1,position=-2;
  logic [12:0] data=13'h1a5b;
  r[index][1][position+:13]<=repeat(2) @(go or go) data;
  index=-1; position=0; data='0;
endtask
task automatic outer(ref static T r[3:1]); inner(r); endtask
initial begin
  automatic logic [129:0] expected='z;
  automatic logic [12:0] data=13'h1a5b;
  values='{default:'{default:'1}};
  for (int n=2;n<13;n++) expected[65-2+n]=data[n];
  outer(values[1]); wait fork;
  values[1][1]='z;
  ->go; #1;
  if (values[1][1]!=={130{1'bz}}) $fatal(1,"one countdown");
  ->go; #1;
  if (values[1][1]!==expected || values[0][1]!=={130{1'b1}} || values[1][3]!=={130{1'b1}})
    $fatal(1,"captured mask and current neighbours");
  $display("passed");
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto& target = std::get<s::NbaRefBitsTarget>(compiled.design.nbaLayouts[0].targets[0]);
    EXPECT_EQ(target.windowLsb, 65U);
    EXPECT_EQ(target.windowWidth, 65U);
    ASSERT_EQ(target.dims.size(), 1U);
    EXPECT_EQ(target.dims[0].left, -1);
    for (int repetition = 0; repetition < 2; ++repetition) {
        const auto result = execute(compiled.design);
        success(result);
        EXPECT_EQ(result.output, "passed\n");
        EXPECT_EQ(result.time, 2U);
    }
}

TEST(EventNbaRefWindow, EmptyWindowsStillRegisterAndEvaluateNativeGuards) {
    auto compiled = compileDetached(R"sv(module top;
typedef logic [1:0][7:0] T;
T value='1; event go; int calls; bit enabled=1;
task automatic issue(ref static T r);
  int above=8,below=-2;
  r[0][above]<=@(go iff enabled) 1'b0;
  r[1][below]<=repeat(2) @(go iff enabled) 1'b0;
endtask
initial begin issue(value); ->go; ->go; #1; $display("%0d:%h",calls,value); end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto& design = compiled.design;
    s::SlotId calls = s::kNone;
    for (std::uint32_t i = 0; i < design.slots.size(); ++i)
        if (design.slots[i].name == "top.calls") calls = s::SlotId{i};
    ASSERT_NE(calls, s::kNone);
    const auto one = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
    design.constants.push_back({s::s2(32), {1}, {}});
    s::Subroutine guard;
    guard.name = "top.native_window_guard";
    guard.loc = {0, 6, 3};
    guard.automatic = true;
    guard.returnType = s::u2(1);
    guard.numTemps = 2;
    guard.blocks.resize(1);
    guard.blocks[0].instrs = {
        {s::Op::Load, s::s2(32), s::TempId{0}, {s::Operand::slot(calls)}, {}},
        {s::Op::Add,
         s::s2(32),
         s::TempId{1},
         {s::Operand::temp(s::TempId{0}), s::Operand::constant(one)},
         {}},
        {s::Op::Store,
         s::voidType(),
         s::kNone,
         {s::Operand::slot(calls), s::Operand::temp(s::TempId{1})},
         {}},
    };
    const auto truth = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
    design.constants.push_back({s::u2(1), {1}, {}});
    guard.blocks[0].instrs.push_back(
        {s::Op::Ret, s::voidType(), s::kNone, {s::Operand::constant(truth)}, {}});
    for (auto& instruction : guard.blocks[0].instrs)
        instruction.loc = guard.loc;
    const auto id = s::SubroutineId{static_cast<std::uint32_t>(design.subroutines.size())};
    design.subroutines.push_back(std::move(guard));
    for (auto& layout : design.nbaLayouts)
        std::get<s::NbaNamedEvent>(layout.events[0]).guard = id;
    for (auto& process : design.processes)
        process.writes.push_back({calls});
    ASSERT_TRUE(s::verify(design).empty());
    const auto result = execute(design);
    success(result);
    EXPECT_EQ(result.output, "3:ffff\n");
}

constexpr std::string_view windowSource = R"sv(module top;
logic [1:0][7:0] value='1; event go;
task automatic issue(ref static logic [1:0][7:0] r);
  int position=2;
  r[1][position]<=@(go) 1'b0;
endtask
initial begin issue(value); ->go; #1; end
endmodule
)sv";

TEST(EventNbaRefWindow, ParentMetadataRejectsNoncanonicalOutsideAndOverflowingRanges) {
    const auto compiled = compileDetached(windowSource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (const auto [low, width] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
             {1, 0}, {16, 1}, {15, 2}, {UINT32_MAX, 2}, {2, UINT32_MAX}}) {
        auto design = compiled.design;
        auto& target = std::get<s::NbaRefBitsTarget>(design.nbaLayouts[0].targets[0]);
        target.windowLsb = low;
        target.windowWidth = width;
        EXPECT_FALSE(s::verifyNbaLayouts(design).empty()) << low << "+" << width;
    }
    auto legacy = compiled.design;
    auto& target = std::get<s::NbaRefBitsTarget>(legacy.nbaLayouts[0].targets[0]);
    target.windowLsb = target.windowWidth = 0;
    EXPECT_TRUE(s::verifyNbaLayouts(legacy).empty());
    const auto text = s::toText(legacy);
    const auto start = text.find("write.ref.bits ");
    ASSERT_NE(start, std::string::npos);
    const auto line = text.substr(start, text.find('\n', start) - start);
    EXPECT_EQ(line.find(" parent="), std::string::npos);
}

TEST(EventNbaRefWindow, RuntimePositionParentsKeepTheirExistingSourceBoundary) {
    const auto compiled = compileDetached(R"sv(module top;
logic [1:0][7:0] value; event go;
task automatic issue(ref static logic [1:0][7:0] r);
  int parent=1,position=2;
  r[parent][position]<=@(go) 1'b0;
endtask
initial issue(value);
endmodule
)sv");
    EXPECT_FALSE(compiled.success);
    EXPECT_TRUE(compiled.unsupported);
    EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    EXPECT_NE(compiled.diagnostics.find("a runtime select of this parent packed range"),
              std::string::npos);
}

TEST(EventNbaRefWindow, BoundsWarningUsesTheParentAndIsIssuedOncePerPlace) {
    const auto compiled = compileDetached(R"sv(module top;
typedef logic [1:0][7:0] T;
T value='1; event go;
task automatic issue(ref static T r,input int position); r[0][position]<=@(go) 1'b0; endtask
initial begin
  issue(value,8); ->go; #1;
  issue(value,9); ->go; #1;
  $display("%h",value);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    vm::RunOptions options;
    options.boundsCheck = true;
    const auto result = execute(compiled.design, options);
    EXPECT_EQ(result.result.reason, k::EndReason::Quiescent);
    EXPECT_TRUE(result.result.error.empty()) << result.result.error;
    EXPECT_EQ(result.output, "ffff\n");
    const auto first = result.diagnostics.find("(--bounds-check=warn)");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(result.diagnostics.find("(--bounds-check=warn)", first + 1), std::string::npos);
    EXPECT_EQ(result.controls, 0U);
    EXPECT_EQ(result.payloads, 0U);
}
}  // namespace
