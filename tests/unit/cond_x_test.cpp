// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original SIR inputs check the approved CLI 2.2 / VALUE_SEMANTICS 3.9,
// 4.1 and 4.2 diagnostics independently of source lowering.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
using namespace kirimasim::sir;
namespace k = kirimasim::kernel;
namespace vm = kirimasim::vm;

struct Program {
    Design d;
    std::size_t body = 0;

    Program() {
        d.sources = {"cond-x.sv", "other.sv"};
        d.instances = {{"top", "top", kNone}};
        process(ProcessKind::Initial);
    }

    void process(ProcessKind kind) {
        Process p;
        p.name = "top.p" + std::to_string(d.processes.size());
        p.kind = kind;
        p.loc = {0, 1, 1};
        p.blocks.resize(1);
        body = d.processes.size();
        d.processes.push_back(std::move(p));
    }

    Operand constant(Type type, std::vector<std::uint64_t> a, std::vector<std::uint64_t> b = {}) {
        const ConstId id{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back({type, std::move(a), std::move(b)});
        return Operand::constant(id);
    }

    void emit(Op op, std::vector<Operand> args, SourceLoc loc = {0, 1, 1}) {
        d.processes[body].blocks[0].instrs.push_back(
            {op, voidType(), TempId{kNone}, std::move(args), loc});
    }

    void marker(const std::string& construct, Operand value, SourceLoc loc = {0, 7, 3}) {
        emit(Op::CondX, {Operand::str(construct), std::move(value)}, loc);
    }

    void print(const std::string& text) {
        emit(Op::Syscall, {Operand::str("$display"), Operand::str(text)});
    }

    void printValue(Operand value) {
        emit(Op::Syscall, {Operand::str("$display"), Operand::str("%b"), std::move(value)});
    }

    void end() { emit(Op::Ret, {}); }
};

struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string out;
        std::array<char, 512> bytes{};
        while (const auto n = std::fread(bytes.data(), 1, bytes.size(), file))
            out.append(bytes.data(), n);
        return out;
    }
};

struct Execution {
    k::EndReason reason;
    std::string error;
    std::string output;
    int level = 0;
};

Execution run(const Program& p, vm::CondXMode mode) {
    const auto errors = verify(p.d);
    EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front().message);
    if (!errors.empty()) return {k::EndReason::Internal, "invalid test SIR", {}};
    Capture output;
    EXPECT_NE(output.file, nullptr);
    if (!output.file) return {k::EndReason::Internal, "cannot make capture", {}};
    k::Context context(k::OutputDestinations{output.file, output.file});
    vm::RunOptions options;
    options.condX = mode;
    const auto result = vm::run(p.d, context, options);
    return {result.reason, result.error, output.text(), context.scheduler().level()};
}

std::string warning(const std::string& construct, SourceLoc loc = {0, 7, 3}) {
    return std::string(loc.file == 0 ? "cond-x.sv:" : "other.sv:") + std::to_string(loc.line) +
           ":" + std::to_string(loc.column) + ": warning: " + construct +
           (construct == "case" ? " expression" : " condition") +
           " contains X/Z bits (--cond-x=warn)\n";
}

Instr statement(Op op, std::vector<Operand> operands, SourceLoc loc = {0, 1, 1}) {
    return {op, voidType(), TempId{kNone}, std::move(operands), loc};
}

Program armedWaitProgram() {
    Program p;
    for (const std::string name : {"top.clock", "top.seen"}) {
        Slot slot;
        slot.name = name;
        slot.type = u2(1);
        slot.flags = kObservable;
        p.d.slots.push_back(std::move(slot));
    }
    const auto one = p.constant(u2(1), {1});
    auto& proc = p.d.processes[0];
    proc.kind = ProcessKind::General;
    proc.armBeforeStart = true;
    proc.writes = {{SlotId{1}}};
    proc.blocks.resize(2);
    proc.blocks[0].instrs = {statement(
        Op::WaitEvent, {Operand::event(SlotId{0}, Edge::Pos), Operand::block(BlockId{1})})};
    proc.blocks[1].instrs = {
        statement(Op::Store, {Operand::slot(SlotId{1}), one}),
        statement(Op::Syscall, {Operand::str("$display"), Operand::str("caught")}),
        statement(Op::Ret, {})};
    return p;
}

TEST(CondX, ArmBeforeStartDefaultsOffAndSerializesOnlyWhenEnabled) {
    EXPECT_FALSE(Process{}.armBeforeStart);
    auto p = armedWaitProgram();
    ASSERT_TRUE(verify(p.d).empty());  // The continuation may write after the sole entry wait.
    const auto armedText = toText(p.d);
    EXPECT_NE(armedText.find("process @0 \"top.p0\" kind=general inst=0 arm-before-start"),
              std::string::npos);
    EXPECT_EQ(toText(p.d), armedText);
    p.d.processes[0].armBeforeStart = false;
    ASSERT_TRUE(verify(p.d).empty());
    EXPECT_EQ(toText(p.d).find("arm-before-start"), std::string::npos);
}

TEST(CondX, ArmBeforeStartRejectsNonGeneralOrNestedAndTriggeredProcesses) {
    for (unsigned shape = 0; shape < 6; ++shape) {
        SCOPED_TRACE(shape);
        auto p = armedWaitProgram();
        auto& proc = p.d.processes[0];
        switch (shape) {
            case 0: proc.kind = ProcessKind::Initial; break;
            case 1: proc.kind = ProcessKind::Ff; break;
            case 2: proc.parent = Owner::of(ProcessId{0}); break;
            case 3: proc.parent = Owner::of(SubroutineId{0}); break;
            case 4: proc.triggers = {{{SlotId{0}}, Edge::Pos}}; break;
            default: proc.strengthTriggers = {{SlotId{0}}}; break;
        }
        const auto errors = verify(p.d);
        EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const VerifyError& e) {
            return e.message ==
                   "arm-before-start needs a top-level general process without triggers";
        }));
    }
}

TEST(CondX, ArmBeforeStartRejectsEveryEntryShapeExceptTheWaitAlone) {
    for (unsigned shape = 0; shape < 8; ++shape) {
        SCOPED_TRACE(shape);
        auto p = armedWaitProgram();
        auto& blocks = p.d.processes[0].blocks;
        switch (shape) {
            case 0: blocks.clear(); break;
            case 1: blocks[0].instrs.clear(); break;
            case 2: blocks[0].instrs = {statement(Op::Ret, {})}; break;
            case 3:
                blocks[0].instrs.insert(blocks[0].instrs.begin(),
                                        statement(Op::Store, {Operand::slot(SlotId{1}),
                                                              Operand::constant(ConstId{0})}));
                break;
            case 4:
                blocks[0].instrs.insert(
                    blocks[0].instrs.begin(),
                    statement(Op::Syscall, {Operand::str("$display"), Operand::str("prefix")}));
                break;
            case 5:
                blocks[0].instrs.insert(blocks[0].instrs.begin(),
                                        statement(Op::Br, {Operand::block(BlockId{1})}));
                break;
            case 6: {
                Subroutine fn;
                fn.name = "top.prefix";
                fn.returnType = voidType();
                fn.blocks.resize(1);
                fn.blocks[0].instrs = {statement(Op::Ret, {})};
                p.d.subroutines.push_back(std::move(fn));
                blocks[0].instrs.insert(
                    blocks[0].instrs.begin(),
                    statement(Op::Call, {Operand::subroutine(SubroutineId{0})}));
                break;
            }
            default: blocks[0].instrs.push_back(statement(Op::Ret, {})); break;
        }
        const auto errors = verify(p.d);
        EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const VerifyError& e) {
            return e.message == "arm-before-start entry must contain only wait.event";
        }));
    }
}

TEST(CondX, ArmBeforeStartSamplesAfterStaticInitializationAndCatchesTimeZeroInEitherOrder) {
    for (const bool initialFirst : {false, true})
        for (const bool arm : {false, true})
            for (const auto mode : {vm::CondXMode::Off, vm::CondXMode::Warn}) {
                SCOPED_TRACE(::testing::Message()
                             << initialFirst << '/' << arm << '/' << static_cast<unsigned>(mode));
                auto p = armedWaitProgram();
                p.d.processes[0].armBeforeStart = arm;
                Subroutine watch;
                watch.name = "top.clock_watch";
                watch.kind = SubroutineKind::Watch;
                watch.frameOf = Owner::of(ProcessId{initialFirst ? 1U : 0U});
                watch.returnType = u2(1);
                watch.numTemps = 1;
                watch.blocks.resize(1);
                watch.blocks[0].instrs = {
                    {Op::Load, u2(1), TempId{0}, {Operand::slot(SlotId{0})}, {0, 1, 1}},
                    statement(Op::Ret, {Operand::temp(TempId{0})})};
                p.d.subroutines.push_back(std::move(watch));
                auto item = Operand::subroutine(SubroutineId{0});
                item.edge = Edge::Neg;
                p.d.processes[0].blocks[0].instrs[0].operands[0] = item;
                p.process(ProcessKind::Initial);
                p.d.processes[p.body].writes = {{SlotId{0}}};
                p.emit(Op::Store, {Operand::slot(SlotId{0}), p.constant(u2(1), {0})});
                p.print("initial");
                p.end();
                p.process(ProcessKind::StaticInit);
                p.d.processes[p.body].writes = {{SlotId{0}}};
                p.emit(Op::Store, {Operand::slot(SlotId{0}), p.constant(u2(1), {1})});
                p.end();
                p.process(ProcessKind::Final);
                p.d.processes[p.body].numTemps = 1;
                p.d.processes[p.body].blocks[0].instrs.push_back(
                    {Op::Load, u2(1), TempId{0}, {Operand::slot(SlotId{1})}, {0, 1, 1}});
                p.printValue(Operand::temp(TempId{0}));
                p.end();
                if (initialFirst) std::swap(p.d.processes[0], p.d.processes[1]);
                const auto result = run(p, mode);
                EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                EXPECT_TRUE(result.error.empty());
                const bool caught = arm || !initialFirst;
                EXPECT_EQ(result.output, caught ? "initial\ncaught\n1\n" : "initial\n0\n");
            }
}

TEST(CondX, ArmBeforeStartWatchErrorRunsALaterRegisteredFinalBeforeAnyInitialBody) {
    Program p;
    p.print("unreachable initial");
    p.end();
    p.process(ProcessKind::General);
    auto& proc = p.d.processes[p.body];
    proc.armBeforeStart = true;
    proc.blocks.resize(2);
    proc.blocks[0].instrs = {statement(
        Op::WaitEvent, {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{1})})};
    proc.blocks[1].instrs = {
        statement(Op::Syscall, {Operand::str("$display"), Operand::str("unreachable waiter")}),
        statement(Op::Ret, {})};
    Subroutine watch;
    watch.name = "top.unknown_watch";
    watch.kind = SubroutineKind::Watch;
    watch.frameOf = Owner::of(ProcessId{1});
    watch.returnType = u4(1);
    watch.blocks.resize(1);
    const auto x = p.constant(u4(1), {1}, {1});
    watch.blocks[0].instrs = {statement(Op::CondX, {Operand::str("if"), x}, {0, 5, 3}),
                              statement(Op::Ret, {x})};
    p.d.subroutines.push_back(std::move(watch));
    p.process(ProcessKind::Final);
    p.print("final");
    p.end();
    const auto result = run(p, vm::CondXMode::Error);
    EXPECT_EQ(result.reason, k::EndReason::Error) << result.error;
    EXPECT_EQ(result.level, 1);
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.output, "cond-x.sv:5: error: if condition contains X/Z bits (--cond-x=error) "
                             "[top @ 0ns]\nfinal\n");
}

TEST(CondX, VerifierAcceptsEachConstructAndTextPreservesItsRawOperand) {
    for (const std::string construct : {"if", "?:", "case"}) {
        SCOPED_TRACE(construct);
        Program p;
        p.marker(construct, p.constant(u4(2), {3}, {1}));  // 1x, a true but unknown-bearing value.
        p.end();
        EXPECT_TRUE(verify(p.d).empty());
        const auto text = toText(p.d);
        EXPECT_NE(text.find("cond.x \"" + construct + "\", const i4<2> 2'b1x"), std::string::npos);
        EXPECT_EQ(toText(p.d), text);
    }
    for (const Type type : {u2(1), s2(65), u4(129)}) {
        Program p;
        const auto chunks = (type.width + 63) / 64;
        p.marker("if", p.constant(type, std::vector<std::uint64_t>(chunks),
                                  isFourState(type) ? std::vector<std::uint64_t>(chunks)
                                                    : std::vector<std::uint64_t>{}));
        p.end();
        EXPECT_TRUE(verify(p.d).empty());
    }
}

TEST(CondX, VerifierRejectsMalformedMarkerShapes) {
    for (unsigned shape = 0; shape < 10; ++shape) {
        SCOPED_TRACE(shape);
        Program p;
        p.marker("if", p.constant(u4(1), {1}, {1}));
        p.end();
        auto& in = p.d.processes[0].blocks[0].instrs[0];
        switch (shape) {
            case 0: in.operands.clear(); break;
            case 1: in.operands.pop_back(); break;
            case 2: in.operands.push_back(in.operands.back()); break;
            case 3: in.operands[0] = Operand::str("while"); break;
            case 4: in.operands[0] = Operand::str(""); break;
            case 5: in.operands[0] = in.operands[1]; break;
            case 6: in.operands[1] = p.constant({TypeKind::Real, 0}, {0}); break;
            case 7: in.operands[1] = Operand::str("x"); break;
            case 8:
                in.type = u4(1);
                in.result = TempId{0};
                p.d.processes[0].numTemps = 1;
                break;
            default:
                in.result = TempId{0};
                p.d.processes[0].numTemps = 1;
                break;
        }
        EXPECT_FALSE(verify(p.d).empty());
    }
}

TEST(CondX, VerifierRejectsUnknownConstantBitsOutsideTheDeclaredWidth) {
    for (unsigned width : {1U, 65U, 129U}) {
        SCOPED_TRACE(width);
        Program p;
        std::vector<std::uint64_t> a((width + 63) / 64), b(a.size());
        b.back() = 2;  // The last word has only bit zero in range.
        p.marker("if", p.constant(u4(width), a, b));
        p.end();
        const auto errors = verify(p.d);
        EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const VerifyError& e) {
            return e.message == "bits above the width are not 0";
        }));
    }
}

TEST(CondX, KnownValuesAndDisabledUnknownChecksAreSilent) {
    for (const auto mode : {vm::CondXMode::Off, vm::CondXMode::Warn, vm::CondXMode::Error})
        for (unsigned width : {1U, 2U, 64U, 65U, 129U}) {
            SCOPED_TRACE(::testing::Message() << static_cast<unsigned>(mode) << '/' << width);
            Program p;
            std::vector<std::uint64_t> zero((width + 63) / 64), one = zero;
            one.back() = 1;
            p.marker("if", p.constant(u4(width), zero, zero));
            p.marker("?:", p.constant(u4(width), one, zero));
            p.marker("case", p.constant(u2(width), one));
            if (mode == vm::CondXMode::Off) p.marker("if", p.constant(u4(width), one, one));
            p.print("done");
            p.end();
            const auto result = run(p, mode);
            EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            EXPECT_TRUE(result.error.empty());
            EXPECT_EQ(result.output, "done\n");
        }
}

TEST(CondX, WarnsForXAndZInEveryWordWithoutChangingTheValue) {
    for (unsigned width : {1U, 2U, 32U, 64U, 65U, 129U}) {
        std::vector<unsigned> bits{0, width - 1};
        for (unsigned at = 64; at < width; at += 64)
            bits.push_back(at);
        std::sort(bits.begin(), bits.end());
        bits.erase(std::unique(bits.begin(), bits.end()), bits.end());
        for (const auto bit : bits)
            for (bool z : {false, true})
                for (const std::string construct : {"if", "?:", "case"}) {
                    SCOPED_TRACE(::testing::Message()
                                 << width << '/' << bit << '/' << z << '/' << construct);
                    Program p;
                    std::vector<std::uint64_t> a((width + 63) / 64), b(a.size());
                    b[bit / 64] = std::uint64_t{1} << (bit % 64);
                    if (!z) a = b;
                    if (bit != 0) a[0] |= 1;  // An in-range known one must not hide the X/Z.
                    const auto value = p.constant(u4(width), a, b);
                    p.marker(construct, value);
                    p.printValue(value);
                    p.end();
                    std::string expectedBits(width, '0');
                    expectedBits[width - 1 - bit] = z ? 'z' : 'x';
                    if (bit != 0) expectedBits.back() = '1';
                    const auto result = run(p, vm::CondXMode::Warn);
                    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                    EXPECT_TRUE(result.error.empty());
                    EXPECT_EQ(result.output,
                              warning(construct) + expectedBits +
                                  "\nkirimasim: info: condition X check: 1 evaluations at 1 source "
                                  "positions\n");
                }
    }
}

TEST(CondX, WarningDeduplicationCountsEvaluationsAndCompleteSourcePositions) {
    Program p;
    const auto x = p.constant(u4(2), {3}, {1});
    for (unsigned i = 0; i < 3; ++i)
        p.marker("if", x, {0, 10, 3});
    p.marker("?:", x, {0, 10, 8});
    p.marker("case", x, {0, 20, 2});
    p.marker("case", x, {0, 20, 2});
    p.marker("if", x, {1, 10, 3});
    p.marker("if", p.constant(u4(1), {1}, {0}), {0, 30, 2});
    p.print("done");
    p.end();
    const std::string expected =
        warning("if", {0, 10, 3}) + warning("?:", {0, 10, 8}) + warning("case", {0, 20, 2}) +
        warning("if", {1, 10, 3}) +
        "done\nkirimasim: info: condition X check: 7 evaluations at 4 source positions\n";
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        SCOPED_TRACE(repeat);
        const auto result = run(p, vm::CondXMode::Warn);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(result.output, expected);  // Each Context reports its own locations.
    }
}

TEST(CondX, ErrorStopsTheFirstEvaluationAndRunsFinalAfterInitialOrStaticFailure) {
    for (const auto kind : {ProcessKind::Initial, ProcessKind::StaticInit}) {
        SCOPED_TRACE(static_cast<unsigned>(kind));
        Program p;
        p.d.processes[0].kind = kind;
        p.marker("if", p.constant(u4(2), {3}, {1}), {0, 5, 3});
        p.print("unreachable");
        p.end();
        if (kind == ProcessKind::StaticInit) {
            p.process(ProcessKind::Initial);
            p.print("ordinary initial must not start");
            p.end();
        }
        p.process(ProcessKind::Final);
        p.print("final");
        p.end();
        const auto result = run(p, vm::CondXMode::Error);
        EXPECT_EQ(result.reason, k::EndReason::Error) << result.error;
        EXPECT_EQ(result.level, 1);
        EXPECT_TRUE(result.error.empty());
        EXPECT_EQ(result.output,
                  "cond-x.sv:5: error: if condition contains X/Z bits (--cond-x=error) "
                  "[top @ 0ns]\nfinal\n");
    }
}

TEST(CondX, WarnSummaryIncludesStaticInitializersAndFinalProcedures) {
    Program p;
    p.d.processes[0].kind = ProcessKind::StaticInit;
    const auto x = p.constant(u4(1), {1}, {1});
    p.marker("if", x, {0, 5, 3});
    p.end();
    p.process(ProcessKind::Initial);
    p.marker("?:", x, {0, 9, 7});
    p.end();
    p.process(ProcessKind::Final);
    p.marker("case", x, {0, 15, 2});
    p.end();
    const auto result = run(p, vm::CondXMode::Warn);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(result.output,
              warning("if", {0, 5, 3}) + warning("?:", {0, 9, 7}) + warning("case", {0, 15, 2}) +
                  "kirimasim: info: condition X check: 3 evaluations at 3 source positions\n");
}

TEST(CondX, ErrorInFinalPreservesTheExistingPreFinalTerminationPolicy) {
    Program p;
    p.end();
    p.process(ProcessKind::Final);
    p.marker("case", p.constant(u4(1), {0}, {1}), {0, 15, 2});
    p.print("unreachable final continuation");
    p.end();
    p.process(ProcessKind::Final);
    p.print("unreachable later final");
    p.end();
    const auto result = run(p, vm::CondXMode::Error);
    // CondX does not replace Scheduler::runFinal's policy for a final failure.
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(result.output,
              "cond-x.sv:15: error: case expression contains X/Z bits (--cond-x=error) "
              "[top @ 0ns]\n");
}

}  // namespace
