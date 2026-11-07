// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kirimasim/front/front.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"
#include <algorithm>
namespace {
namespace f = kirimasim::front;
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-ff-expression-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) return;
        directory = pattern;
        const auto path = directory / "design.sv";
        std::ofstream stream(path);
        stream << text;
        stream.close();
        if (stream) file = std::filesystem::relative(std::filesystem::canonical(path)).string();
    }
    ~Source() {
        if (!directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
    }
};
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() const {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string result;
        std::array<char, 512> buffer{};
        while (const auto count = std::fread(buffer.data(), 1, buffer.size(), file))
            result.append(buffer.data(), count);
        return result;
    }
};
f::CompileResult compile(std::string_view text, bool conditions = false) {
    Source source(text);
    EXPECT_FALSE(source.file.empty());
    if (source.file.empty()) return {};
    f::CompileOptions options;
    options.condX = conditions;
    return f::compile(std::vector<std::string>{source.file}, options);
}
unsigned slot(const s::Design& d, std::string_view suffix) {
    for (unsigned i = 0; i < d.slots.size(); ++i)
        if (d.slots[i].name.ends_with(suffix)) return i;
    ADD_FAILURE() << suffix;
    return 0;
}

std::uint32_t ff(const s::Design& design) {
    for (std::uint32_t i = 0; i < design.processes.size(); ++i) {
        const auto& process = design.processes[i];
        if (process.kind == s::ProcessKind::Ff ||
            std::any_of(process.triggers.begin(), process.triggers.end(),
                        [](const auto& trigger) { return trigger.watch != s::kNone; })) {
            EXPECT_EQ(process.kind, s::ProcessKind::Ff);
            return i;
        }
    }
    ADD_FAILURE() << "missing ff process";
    return 0;
}
bool rejects(const s::Design& design, std::string_view message) {
    const auto errors = s::verify(design);
    return std::any_of(errors.begin(), errors.end(), [&](const auto& error) {
        return error.message.find(message) != std::string::npos;
    });
}
f::CompileResult mixed() {
    return compile(R"(
module top;
bit a=0,b=0,reset=1,enable=1;int hits=0;
always_ff @(posedge (a^b) iff enable or negedge reset) hits<=hits+1;
initial begin #1;a=1;#1;reset=0;#1;$display("%0d",hits);end
endmodule
)");
}
TEST(FfExpression, ProducerRetainsFfKindAndMixedTriggerOwnership) {
    auto compiled = mixed();
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto id = ff(compiled.design());
    const auto& process = compiled.design().processes[id];
    ASSERT_EQ(process.triggers.size(), 2U);
    const auto& watch = process.triggers[0];
    EXPECT_EQ(watch.range.slot, s::kNone);
    ASSERT_NE(watch.watch, s::kNone);
    ASSERT_NE(watch.guard, s::kNone);
    EXPECT_EQ(compiled.design().subroutine(watch.watch).kind, s::SubroutineKind::Watch);
    EXPECT_EQ(compiled.design().subroutine(watch.watch).frameOf.kind, s::Owner::Kind::Process);
    EXPECT_EQ(compiled.design().subroutine(watch.watch).frameOf.id, id);
    EXPECT_EQ(compiled.design().subroutine(watch.guard).kind, s::SubroutineKind::Guard);
    EXPECT_EQ(process.triggers[1].watch, s::kNone);
    EXPECT_NE(process.triggers[1].range.slot, s::kNone);
    EXPECT_FALSE(process.armBeforeStart);
    EXPECT_NE(s::toText(compiled.design()).find("posedge(watch @"), std::string::npos);
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design(), context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "2\n");
}
TEST(FfExpression, TriggerReferenceKindOwnerAndRangeAreVerified) {
    const auto compiled = mixed();
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto id = ff(compiled.design());
    const auto& valid = compiled.design();
    auto broken = valid;
    broken.processes[id].triggers[0].watch = s::SubroutineId{9999};
    EXPECT_TRUE(rejects(broken, "is not a watch"));
    broken = valid;
    broken.processes[id].triggers[0].watch = broken.processes[id].triggers[0].guard;
    EXPECT_TRUE(rejects(broken, "is not a watch"));
    broken = valid;
    const auto watcher = broken.processes[id].triggers[0].watch;
    broken.subroutine(watcher).frameOf = s::Owner::of(s::ProcessId{0});
    ASSERT_NE(id, 0U);
    EXPECT_TRUE(rejects(broken, "trigger watch @"));
    EXPECT_TRUE(rejects(broken, "reads another frame"));
    broken = valid;
    broken.processes[id].triggers[0].range.slot = s::SlotId{0};
    EXPECT_TRUE(rejects(broken, "also names a slot range"));
    broken = valid;
    broken.processes[id].triggers[0].range.width = 1;
    EXPECT_TRUE(rejects(broken, "also names a slot range"));
    broken = valid;
    broken.processes[id].kind = s::ProcessKind::General;
    EXPECT_TRUE(rejects(broken, "expression trigger needs ff or ff-plain"));
    broken = valid;
    broken.processes[id].kind = s::ProcessKind::FfPlain;
    EXPECT_TRUE(s::verify(broken).empty());
}
TEST(FfExpression, WatchesDoNotWeakenSingleWriterRestrictions) {
    auto compiled = mixed();
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto design = compiled.design();
    const auto id = ff(design);
    const auto output = s::SlotId{slot(design, "hits")};
    const auto value = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
    const auto type = design.slot(output).type;
    design.constants.push_back(
        {type,
         {1},
         s::isFourState(type) ? std::vector<std::uint64_t>{0} : std::vector<std::uint64_t>{}});
    s::Process writer;
    writer.name = "other_writer";
    writer.instance = design.processes[id].instance;
    writer.loc = design.processes[id].loc;
    writer.writes = {{output}};
    writer.blocks.resize(1);
    writer.blocks[0].instrs = {{s::Op::Store,
                                s::voidType(),
                                s::kNone,
                                {s::Operand::slot(output), s::Operand::constant(value)}},
                               {s::Op::Ret, s::voidType(), s::kNone, {}}};
    for (auto& instruction : writer.blocks[0].instrs)
        instruction.loc = writer.loc;
    design.processes.push_back(std::move(writer));
    EXPECT_TRUE(rejects(design, "also writes"));
    design.processes[id].kind = s::ProcessKind::FfPlain;
    const auto errors = s::verify(design);
    std::string diagnostic;
    for (const auto& error : errors)
        diagnostic += error.message + "\n";
    EXPECT_TRUE(errors.empty()) << diagnostic;
}
TEST(FfExpression, BodyCannotAddAnotherEventOrDelay) {
    auto compiled = mixed();
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto design = compiled.design();
    const auto id = ff(design);
    const auto delay = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
    design.constants.push_back({s::u2(64), {1}, {0}});
    auto& process = design.processes[id];
    process.blocks.back().instrs.back() = {
        s::Op::WaitDelay,
        s::voidType(),
        s::kNone,
        {s::Operand::constant(delay), s::Operand::block(s::kEntryBlock)},
        process.loc};
    EXPECT_TRUE(rejects(design, "wait"));
}
TEST(FfExpression, TriggerCannotReadAnInactiveAutomaticScope) {
    auto compiled = mixed();
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto design = compiled.design();
    const auto id = ff(design);
    auto& process = design.processes[id];
    s::FrameVar local;
    local.name = "late";
    local.type = s::u4(1);
    local.home = s::FrameHome::Scoped;
    local.activationScope = s::ActivationScopeId{0};
    process.frame.push_back(local);
    process.activation.scopes.push_back({s::kNone, {s::FrameVarId{0}}});
    auto& watcher = design.subroutine(process.triggers[0].watch);
    watcher.numTemps = 1;
    watcher.returnType = s::u4(1);
    watcher.blocks.resize(1);
    watcher.blocks[0].instrs = {
        {s::Op::Fload, s::u4(1), s::TempId{0}, {s::Operand::frame(s::FrameVarId{0})}, watcher.loc},
        {s::Op::Ret, s::voidType(), s::kNone, {s::Operand::temp(s::TempId{0})}, watcher.loc}};
    EXPECT_TRUE(rejects(design, "waiter-frame helper accesses an inactive automatic scope"));
}
TEST(FfExpression, InstrumentedFixedSelectorsStillHaveFfRestrictions) {
    auto compiled = compile(R"(
module top;
logic[1:0] c=0;int hits=0;
always_ff @(posedge c[(1'bx ? 0 : 0)])hits<=hits+1;
initial begin #1;c=1;#1;$display("%0d",hits);end
endmodule
)",
                            true);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto& process = compiled.design().processes[ff(compiled.design())];
    ASSERT_EQ(process.triggers.size(), 1U);
    EXPECT_NE(process.triggers[0].watch, s::kNone);
    EXPECT_FALSE(process.armBeforeStart);
}
TEST(FfExpression, ArmingErrorRunsFinalWithoutStartingInitials) {
    auto compiled = compile(R"(
module top;
logic choice=1'bx,a=0,b=1;int hits=0;
always_ff @(posedge (choice?a:b))hits<=hits+1;
initial $display("unexpected initial");
final $display("final");
endmodule
)",
                            true);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output, errors;
    ASSERT_NE(output.file, nullptr);
    ASSERT_NE(errors.file, nullptr);
    k::Context context({output.file, errors.file});
    kirimasim::vm::RunOptions options;
    options.condX = kirimasim::vm::CondXMode::Error;
    const auto result = kirimasim::vm::run(compiled.design(), context, options);
    EXPECT_EQ(result.reason, k::EndReason::Error) << result.error;
    EXPECT_EQ(output.text(), "final\n");
    EXPECT_NE(errors.text().find("error:"), std::string::npos);
}
TEST(FfExpression, FunctionPrivateStateDoesNotSubscribeToItsOwnWrites) {
    auto compiled = compile(R"(
module top;
bit a=0,b=0;int hits=0;
function bit mix();static int calls=0;calls++;return a^b;endfunction
always_ff @(posedge mix())hits<=hits+1;
initial begin
 #1;a=1;#1;b=1;#1;b=0;#1;
 $display("%0d %0d",hits,mix.calls);
end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design(), context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "2 6\n");
}
TEST(FfExpression, EventAndIffFunctionsCannotWriteGlobalState) {
    for (const bool iff : {false, true}) {
        const std::string event = iff ? "posedge (a^b) iff write_state()" : "posedge write_state()";
        auto compiled = compile("module top;bit a,b;int state=0,hits=0;"
                                "function bit write_state();state++;return a^b;endfunction "
                                "always_ff @(" +
                                event + ")hits<=hits+1;endmodule");
        EXPECT_FALSE(compiled.success);
        EXPECT_TRUE(compiled.unsupported);
        EXPECT_TRUE(compiled.verifyErrors.empty());
        EXPECT_NE(compiled.diagnostics.find(iff ? "iff condition that writes"
                                                : "event expression that writes"),
                  std::string::npos);
    }
}
TEST(FfExpression, VMRejectsInvalidTriggerMetadataBeforeValues) {
    const auto compiled = mixed();
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto id = ff(compiled.design());
    for (unsigned fault = 0; fault < 5; ++fault) {
        auto design = compiled.design();
        auto& trigger = design.processes[id].triggers[0];
        if (fault == 0) trigger.watch = s::SubroutineId{9999};
        if (fault == 1) trigger.watch = trigger.guard;
        if (fault == 2) trigger.range.slot = s::SlotId{0};
        if (fault == 3) trigger.watch = s::kNone;
        if (fault == 4) design.subroutine(trigger.watch).frameOf = s::Owner::of(s::ProcessId{0});
        k::Context context;
        const auto result = kirimasim::vm::run(design, context);
        EXPECT_EQ(result.reason, k::EndReason::Internal) << fault;
        EXPECT_NE(result.error.find("invalid expression trigger SIR"), std::string::npos) << fault;
        EXPECT_EQ(context.store().size(), 0U) << fault;
    }
}
}  // namespace
