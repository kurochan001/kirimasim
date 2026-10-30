// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Source and immutable-layout checks for captured event-NBA reference selections.
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
            (std::filesystem::temp_directory_path() / "kirimasim-nba-ref-array-XXXXXX").string();
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

TEST(EventNbaRefArray, FormalBoundsBaseOffsetAndPackedSelectorsSurviveReturnedCalls) {
    const auto compiled = compileDetached(R"sv(module top;
logic [128:0] values[2][3:1]; event go;
task automatic inner(ref static logic [128:0] r[-1:1]);
  int index=1, position=62;
  r[index][position+:13]<=@(go) 13'h1a5b;
  index=-1; position=0;
endtask
task automatic outer(ref static logic [128:0] r[3:1]); inner(r); endtask
initial begin
  values='{default:'{default:'0}};
  outer(values[1]); ->go; #1;
  if (values[1][1]!== (129'h1a5b<<62) || values[0][1]!==129'b0 || values[1][3]!==129'b0)
    $fatal(1,"frozen ref array destination");
  $display("passed");
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design.nbaLayouts.size(), 1U);
    const auto& target = std::get<s::NbaRefBitsTarget>(compiled.design.nbaLayouts[0].targets[0]);
    ASSERT_EQ(target.dims.size(), 1U);
    EXPECT_EQ(target.dims[0].left, -1);
    EXPECT_EQ(target.dims[0].right, 1);
    EXPECT_EQ(target.indices.size(), 1U);
    const auto text = s::toText(compiled.design);
    EXPECT_NE(text.find("shape=[-1:1] indices=("), std::string::npos);
    EXPECT_EQ(text, s::toText(compiled.design));
    for (int repetition = 0; repetition < 2; ++repetition) {
        const auto result = execute(compiled.design);
        success(result);
        EXPECT_EQ(result.output, "passed\n");
        EXPECT_EQ(result.time, 1U);
    }
}

TEST(EventNbaRefArray, WholeArrayLeavesShareOneCountdownAndOwnedValues) {
    const auto compiled = compileDetached(R"sv(module top;
int values[3:1]; event go;
task automatic issue(ref static int r[3:1]);
  int source[3:1]='{7,11,13};
  r<=repeat(2) @(go or go) source;
  source='{99,101,103};
endtask
initial begin
  issue(values); wait fork;
  ->go; #1;
  if (values[3]!=0 || values[2]!=0 || values[1]!=0) $fatal(1,"duplicate item countdown");
  ->go; #1;
  $display("%0d:%0d:%0d",values[3],values[2],values[1]);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design.nbaLayouts.size(), 1U);
    EXPECT_EQ(compiled.design.nbaLayouts[0].targets.size(), 3U);
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "7:11:13\n");
    EXPECT_EQ(result.time, 2U);
}

TEST(EventNbaRefArray, ContainerPayloadRootsSurviveIssuerReturnAndCollection) {
    const auto compiled = compileDetached(R"sv(module top;
typedef int queue_t[$]; queue_t values[2]; event go;
task automatic issue(ref static queue_t r[2]);
  queue_t source={7,11};
  r[1]<=@(go) source;
  source[0]=99;
endtask
task automatic churn(); queue_t work={31,37}; work.push_back(41); endtask
initial begin
  issue(values); wait fork;
  values[1].push_back(101);
  for (int n=0;n<4000;n++) churn();
  ->go; #1;
  if (values[0].size()!=0 || values[1].size()!=2) $fatal(1,"typed payload shape");
  $display("%0d:%0d",values[1][0],values[1][1]);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "7:11\n");
}

TEST(EventNbaRefArray, InvalidDestinationsStillOwnCountdownsAndEvaluateNativeGuards) {
    auto compiled = compileDetached(R"sv(module top;
int values[2],calls; event go; bit enabled=1;
task automatic issue(ref static int r[2]);
  logic [64:0] bad='x;
  int outside=3;
  r[bad]<=@(go iff enabled) 7;
  r[outside]<=repeat(2) @(go iff enabled) 11;
endtask
initial begin issue(values); ->go; ->go; #1; $display("%0d:%0d:%0d",calls,values[0],values[1]); end
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
    guard.name = "top.native_count_guard";
    guard.loc = {0, 7, 3};
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
    const auto errors = s::verify(design);
    for (const auto& error : errors)
        ADD_FAILURE() << error.where << ": " << error.message;
    ASSERT_TRUE(errors.empty());
    const auto result = execute(design);
    success(result);
    EXPECT_EQ(result.output, "3:0:0\n");
}

constexpr std::string_view scalarArraySource = R"sv(module top;
int values[2]; event go;
task automatic issue(ref static int r[2]); r[1]<=@(go) 7; endtask
initial begin issue(values); ->go; #1; end
endmodule
)sv";

TEST(EventNbaRefArray, ImmutableSelectionMetadataRejectsMalformedShapesAndIndices) {
    const auto compiled = compileDetached(scalarArraySource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (unsigned variant = 0; variant < 7; ++variant) {
        auto design = compiled.design;
        auto& target = std::get<s::NbaRefTarget>(design.nbaLayouts[0].targets[0]);
        if (variant == 0) target.indices.clear();
        if (variant == 1) target.dims.clear();
        if (variant == 2) target.indices[0] = target.reference;
        if (variant == 3) target.indices[0] = s::NbaValueId{UINT32_MAX};
        if (variant == 4) target.dims[0] = {INT64_MIN, INT64_MAX};
        if (variant == 5) target.dims[0] = {0, INT64_C(4294967296)};
        if (variant == 6) target.leaf = 1;
        EXPECT_FALSE(s::verifyNbaLayouts(design).empty()) << variant;
    }
}

TEST(EventNbaRefArray, ActualStaticArrayMustContainTheDeclaredReferenceShape) {
    auto compiled = compileDetached(scalarArraySource);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto& target = std::get<s::NbaRefTarget>(compiled.design.nbaLayouts[0].targets[0]);
    target.dims[0] = {0, 2};
    ASSERT_TRUE(s::verify(compiled.design).empty());
    const auto result = execute(compiled.design);
    EXPECT_EQ(result.result.reason, k::EndReason::Error);
    EXPECT_NE(result.diagnostics.find("reference array exceeds its static target"),
              std::string::npos);
    EXPECT_TRUE(result.output.empty());
    EXPECT_EQ(result.controls, 0U);
    EXPECT_EQ(result.payloads, 0U);
}
}  // namespace
