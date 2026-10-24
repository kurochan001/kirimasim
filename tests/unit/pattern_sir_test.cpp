// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original SIR inputs independently exercise presentation metadata, verification
// and the execution adapter. They do not use front's aggregate lowering.
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/pattern.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <memory_resource>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace kirimasim::sir;
using Kind = PatternLayout::Kind;
namespace k = kirimasim::kernel;

struct Program {
    Design d;
    Program() {
        d.sources = {"pattern.sv"};
        d.instances = {{"top", "top", kNone}};
        Process p;
        p.name = "top.initial";
        p.loc = {0, 1, 1};
        p.blocks.resize(1);
        d.processes.push_back(std::move(p));
    }
    PatternId layout(PatternLayout row) {
        const PatternId id{static_cast<std::uint32_t>(d.patterns.size())};
        d.patterns.push_back(std::move(row));
        return id;
    }
    PatternId leaf(Type t, Kind kind = Kind::Integral) {
        PatternLayout row;
        row.kind = kind;
        row.value = t;
        return layout(std::move(row));
    }
    Operand constant(Type t, std::vector<std::uint64_t> a, std::vector<std::uint64_t> b = {},
                     std::string bytes = {}) {
        const ConstId id{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back({t, std::move(a), std::move(b), std::move(bytes)});
        return Operand::constant(id);
    }
    Operand emit(Op op, Type type, std::vector<Operand> args) {
        auto& p = d.processes[0];
        const TempId id = type == voidType() ? TempId{kNone} : TempId{p.numTemps++};
        p.blocks[0].instrs.push_back({op, type, id, std::move(args), {0, 1, 1}});
        return Operand::temp(id);
    }
    Operand format(PatternId id, std::vector<Operand> values, bool positional = false) {
        std::vector<Operand> args{Operand::pattern(id),
                                  Operand::str(positional ? "positional" : "named")};
        args.insert(args.end(), values.begin(), values.end());
        return emit(Op::FormatPattern, {TypeKind::String, 0}, std::move(args));
    }
    void print(const Operand& value) {
        emit(Op::Syscall, voidType(), {Operand::str("$display"), Operand::str("%s"), value});
    }
    void end() { emit(Op::Ret, voidType(), {}); }
};

struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string text;
        std::array<char, 512> data{};
        while (const auto n = std::fread(data.data(), 1, data.size(), file))
            text.append(data.data(), n);
        return text;
    }
};
void expectOutput(Program& p, const std::string& expected) {
    const auto errors = verify(p.d);
    ASSERT_TRUE(errors.empty()) << (errors.empty()
                                        ? ""
                                        : errors.front().where + ": " + errors.front().message);
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx(k::OutputDestinations{output.file, output.file});
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error << output.text();
    EXPECT_EQ(output.text(), expected);
}

TEST(PatternSir, ScalarEnumStringAndRealPayloads) {
    Program p;
    const auto integer = p.leaf(s4(65));
    const auto value = p.constant(s4(65), {~std::uint64_t{2}, 1}, {0, 0});
    p.print(p.format(integer, {value}));
    PatternLayout en;
    en.kind = Kind::Enumeration;
    en.value = s4(65);
    en.enumerators.push_back({"VALUE", value.asConst(), false});
    const auto enumeration = p.layout(en);
    p.print(p.format(enumeration, {value}));
    p.print(p.format(enumeration, {p.constant(s4(65), {7, 0}, {0, 0})}));
    const auto string = p.leaf({TypeKind::String, 0}, Kind::String);
    p.print(p.format(string, {p.constant({TypeKind::String, 0}, {}, {}, "a\"b\nc")}));
    const auto real = p.leaf({TypeKind::Real, 0}, Kind::Real);
    p.print(
        p.format(real, {p.constant({TypeKind::Real, 0}, {std::bit_cast<std::uint64_t>(1.25)})}));
    p.end();
    expectOutput(p, "-3\nVALUE\n7\n\"a\\\"b\\nc\"\n1.25\n");
}

TEST(PatternSir, FixedShapesNamedPositionalAndFirstUnionMember) {
    Program p;
    const auto integer = p.leaf(u2(32));
    PatternLayout arr;
    arr.kind = Kind::Array;
    arr.element = integer;
    arr.count = 2;
    const auto array = p.layout(arr);
    PatternLayout record;
    record.kind = Kind::Record;
    record.members = {{"a", array}, {"int", integer, true}};
    const auto structure = p.layout(record);
    const std::vector<Operand> values{p.constant(u2(32), {7}), p.constant(u2(32), {9}),
                                      p.constant(u2(32), {11})};
    p.print(p.format(structure, values));
    p.print(p.format(structure, values, true));
    PatternLayout un;
    un.kind = Kind::Union;
    un.members = {{"first", array}, {"ignored", integer}};
    p.print(p.format(p.layout(un), {values[0], values[1]}));
    PatternLayout tagged;
    tagged.kind = Kind::TaggedUnion;
    tagged.element = array;
    tagged.tag = "tag";
    p.print(p.format(p.layout(tagged), {values[0], values[1]}));
    p.end();
    expectOutput(p, "'{a:'{7, 9}, \\int :11}\n'{'{7, 9}, 11}\n'{first:'{7, 9}}\ntag:'{7, 9}\n");
}

TEST(PatternSir, PackedMembersUseTheirOffsetsWidthsStatesAndSigns) {
    Program p;
    const auto high = p.leaf(s4(32));
    const auto low = p.leaf(u2(33));
    PatternLayout record;
    record.kind = Kind::PackedRecord;
    record.value = u4(65);
    record.members = {{"high", high, false, 33}, {"low", low, false, 0}};
    const auto structure = p.layout(record);
    // high = -3, low = 7; the low field's X/Z bits convert to two-state 0.
    const auto value = p.constant(u4(65), {0xfffffffa00000007ULL, 1}, {2, 0});
    p.print(p.format(structure, {value}));
    p.print(p.format(structure, {value}, true));
    PatternLayout un = record;
    un.kind = Kind::PackedUnion;
    un.members = {{"first", low, false, 0}, {"other", high, false, 33}};
    p.print(p.format(p.layout(un), {value}));
    p.end();
    expectOutput(p, "'{high:-3, low:5}\n'{-3, 5}\n'{first:5}\n");
}

TEST(PatternSir, NestedContainersEmptyAndPackedElementViews) {
    Program p;
    const Type elem = u4(65);
    TypeEntry q;
    q.kind = TypeEntryKind::Queue;
    q.name = "q";
    q.value = elem;
    p.d.types.push_back(q);
    const auto queue = queueType(TypeId{0});
    TypeEntry outer;
    outer.kind = TypeEntryKind::DynArray;
    outer.name = "outer";
    outer.value = queue;
    p.d.types.push_back(outer);
    const auto dynamic = dynarrayType(TypeId{1});
    const auto packedLow = p.leaf(u4(32));
    const auto packedHigh = p.leaf(u4(33));
    PatternLayout record;
    record.kind = Kind::PackedRecord;
    record.value = elem;
    record.members = {{"low", packedLow}, {"high", packedHigh, false, 32}};
    const auto structure = p.layout(record);
    PatternLayout qlayout;
    qlayout.kind = Kind::Container;
    qlayout.value = queue;
    qlayout.element = structure;
    const auto queueLayout = p.layout(qlayout);
    PatternLayout dlayout;
    dlayout.kind = Kind::Container;
    dlayout.value = dynamic;
    dlayout.element = queueLayout;
    const auto nested = p.layout(dlayout);
    const auto first = p.emit(Op::ContainerPack, queue,
                              {p.constant(elem, {7, 0}, {0, 0}), p.constant(elem, {9, 0}, {0, 0})});
    const auto empty = p.emit(Op::ContainerPack, queue, {});
    const auto all = p.emit(Op::ContainerPack, dynamic, {first, empty});
    p.print(p.format(nested, {all}));
    p.print(p.format(nested, {all}, true));
    p.end();
    const auto checks = validatePatterns(p.d);
    ASSERT_TRUE(checks.errors.empty());
    EXPECT_TRUE(checks.shapes[nested.index()].heap);
    expectOutput(p, "'{'{'{low:7, high:0}, '{low:9, high:0}}, '{}}\n'{'{'{7, 0}, '{9, 0}}, '{}}\n");
}

TEST(PatternSir, DeepLayoutAndExecutionAreIterative) {
    Program p;
    auto layout = p.leaf(u2(1));
    constexpr std::size_t depth = 4000;
    for (std::size_t i = 0; i < depth; ++i) {
        PatternLayout array;
        array.kind = Kind::Array;
        array.count = 1;
        array.element = layout;
        layout = p.layout(array);
    }
    p.print(p.format(layout, {p.constant(u2(1), {1})}));
    p.end();
    std::string expected;
    for (std::size_t i = 0; i < depth; ++i)
        expected += "'{";
    expected += '1';
    expected += std::string(depth, '}');
    expected += '\n';
    expectOutput(p, expected);
}

TEST(PatternSir, MetadataTextIsExplicitAndAbsentForOldDesigns) {
    Program p;
    p.end();
    EXPECT_EQ(toText(p.d).find("pattern !p"), std::string::npos);
    const auto integer = p.leaf(u4(65));
    PatternLayout arr;
    arr.kind = Kind::Array;
    arr.count = 2;
    arr.element = integer;
    p.layout(arr);
    const auto text = toText(p.d);
    EXPECT_NE(text.find("pattern !p0 integral i4<65>"), std::string::npos);
    EXPECT_NE(text.find("pattern !p1 array !p0 count=2"), std::string::npos);
}

TEST(PatternSir, InvalidMetadataCyclesOffsetsAndOverflowAreRejected) {
    for (unsigned mode = 0; mode < 9; ++mode) {
        Program p;
        const auto integer = p.leaf(u2(8));
        PatternLayout row;
        row.kind = Kind::Array;
        row.element = integer;
        row.count = 2;
        const auto array = p.layout(row);
        switch (mode) {
            default: FAIL() << "unknown validation case"; break;
            case 0: p.d.patterns[1].element = array; break;
            case 1: p.d.patterns[1].element = PatternId{9}; break;
            case 2: p.d.patterns[0].value = voidType(); break;
            case 3: p.d.patterns[1].count = std::numeric_limits<std::uint64_t>::max(); break;
            case 4: p.d.patterns[1].kind = static_cast<Kind>(255); break;
            case 5: p.d.patterns[0].members = {{"unexpected", integer}}; break;
            case 6:
                p.d.patterns[1] = {};
                p.d.patterns[1].kind = Kind::PackedRecord;
                p.d.patterns[1].value = u2(8);
                p.d.patterns[1].members = {{"field", integer, false, 7}};
                break;
            case 7:
                p.d.patterns[0].kind = Kind::Enumeration;
                p.d.patterns[0].enumerators = {{"bad", ConstId{4}}};
                break;
            case 8:
                p.d.patterns[1].kind = Kind::Union;
                p.d.patterns[1].count = 0;
                p.d.patterns[1].element = kNone;
                break;
        }
        p.end();
        EXPECT_FALSE(verify(p.d).empty()) << mode;
    }
}

TEST(PatternSir, OperandTypesCountsModesAndLayoutReferencesAreChecked) {
    for (unsigned mode = 0; mode < 6; ++mode) {
        Program p;
        const auto layout = p.leaf(u2(8));
        p.format(layout, {p.constant(u2(8), {7})});
        p.end();
        auto& instruction = p.d.processes[0].blocks[0].instrs[0];
        switch (mode) {
            default: FAIL() << "unknown validation case"; break;
            case 0: instruction.operands.pop_back(); break;
            case 1: instruction.operands.push_back(instruction.operands.back()); break;
            case 2: instruction.operands[1] = Operand::str("other"); break;
            case 3: instruction.operands[0] = Operand::pattern(PatternId{9}); break;
            case 4: instruction.type = u2(32); break;
            case 5: instruction.operands.back() = p.constant(u2(32), {7}); break;
        }
        EXPECT_FALSE(verify(p.d).empty()) << mode;
    }
}

TEST(PatternSir, HeapReadsWakeGuardsAndMonitorsWithoutHandleReplacement) {
    for (const bool monitor : {false, true}) {
        for (const bool wrap : {false, true}) {
            SCOPED_TRACE(monitor);
            SCOPED_TRACE(wrap);
            Program p;
            TypeEntry q;
            q.kind = TypeEntryKind::Queue;
            q.name = "q";
            q.value = u2(32);
            p.d.types.push_back(q);
            const auto queue = queueType(TypeId{0});
            p.d.slots.push_back({"top.q", queue});
            const auto home = Operand::slot(SlotId{0});
            p.d.processes[0].writes.push_back({SlotId{0}});
            const auto zero = p.constant(u2(32), {0});
            const auto nine = p.constant(u2(32), {9});
            const auto index = p.constant(s2(64), {0});
            const auto delay = p.constant(u2(64), {2});
            p.emit(Op::Store, voidType(), {home, p.emit(Op::ContainerPack, queue, {zero})});
            const auto leaf = p.leaf(u2(32));
            PatternLayout layout;
            layout.kind = Kind::Container;
            layout.value = queue;
            layout.element = leaf;
            auto root = p.layout(layout);
            if (wrap) {
                PatternLayout record;
                record.kind = Kind::Record;
                record.members = {{"q", root}};
                root = p.layout(record);
            }
            const std::string before = wrap ? "'{q:'{0}}" : "'{0}";
            const std::string after = wrap ? "'{q:'{9}}" : "'{9}";
            const auto desired = p.constant({TypeKind::String, 0}, {}, {}, after);
            Subroutine guard;
            guard.name = "top.guard";
            guard.kind = SubroutineKind::Guard;
            guard.returnType = u2(1);
            guard.frameOf = {Owner::Kind::Process, 0};
            guard.loc = {0, 1, 1};
            guard.numTemps = 3;
            guard.blocks.resize(1);
            guard.blocks[0].instrs = {
                {Op::Load, queue, TempId{0}, {home}, {0, 1, 1}},
                {Op::FormatPattern,
                 {TypeKind::String, 0},
                 TempId{1},
                 {Operand::pattern(root), Operand::str("named"), Operand::temp(TempId{0})},
                 {0, 1, 1}},
                {Op::Eq, u2(1), TempId{2}, {Operand::temp(TempId{1}), desired}, {0, 1, 1}},
                {Op::Ret, voidType(), kNone, {Operand::temp(TempId{2})}, {0, 1, 1}}};
            if (monitor) {
                Process report;
                report.kind = ProcessKind::Postponed;
                report.name = "top.report";
                report.loc = {0, 1, 1};
                report.numTemps = 2;
                report.blocks.resize(1);
                report.blocks[0].instrs = {
                    guard.blocks[0].instrs[0],
                    guard.blocks[0].instrs[1],
                    {Op::Syscall,
                     voidType(),
                     kNone,
                     {Operand::str("$display"), Operand::str("%s"), Operand::temp(TempId{1})},
                     {0, 1, 1}},
                    {Op::Ret, voidType(), kNone, {}, {0, 1, 1}}};
                p.d.processes.push_back(std::move(report));
                p.emit(Op::Monitor, voidType(), {Operand::process(ProcessId{1})});
                p.end();
            } else {
                p.d.subroutines.push_back(std::move(guard));
                p.emit(Op::WaitCond, voidType(),
                       {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{1})});
                p.d.processes[0].blocks.resize(2);
                p.d.processes[0].blocks[1].instrs = {
                    {Op::Syscall,
                     voidType(),
                     kNone,
                     {Operand::str("$display"), Operand::str("done")},
                     {0, 1, 1}},
                    {Op::Ret, voidType(), kNone, {}, {0, 1, 1}}};
            }
            Process setter;
            setter.name = "top.setter";
            setter.loc = {0, 1, 1};
            setter.numTemps = 1;
            setter.writes.push_back({SlotId{0}});
            setter.blocks.resize(2);
            setter.blocks[0].instrs = {
                {Op::WaitDelay, voidType(), kNone, {delay, Operand::block(BlockId{1})}, {0, 1, 1}}};
            setter.blocks[1].instrs = {
                {Op::RefMake, refType(queue.detail), TempId{0}, {home}, {0, 1, 1}},
                {Op::ContainerWrite,
                 voidType(),
                 kNone,
                 {Operand::typeRow(queue.detail), Operand::temp(TempId{0}), index, nine},
                 {0, 1, 1}},
                {Op::Ret, voidType(), kNone, {}, {0, 1, 1}}};
            p.d.processes.push_back(std::move(setter));
            std::string expected = "done\n";
            if (monitor) {
                expected = before;
                expected += '\n';
                expected += after;
                expected += '\n';
            }
            expectOutput(p, expected);
        }
    }
}

TEST(PatternSir, ZeroLeafLargeShapesDoNotExpandDuringVerification) {
    Program p;
    PatternLayout empty;
    empty.kind = Kind::Record;
    const auto record = p.layout(empty);
    PatternLayout array;
    array.kind = Kind::Array;
    array.element = record;
    array.count = std::numeric_limits<std::uint64_t>::max() - 1;
    const auto root = p.layout(array);
    const auto checked = validatePatterns(p.d);
    ASSERT_TRUE(checked.errors.empty());
    EXPECT_EQ(checked.shapes[root.index()].leaves, 0U);
    EXPECT_TRUE(patternArguments(p.d, root, checked.shapes, {}));
    p.print(p.format(root, {}));
    p.end();
    ASSERT_TRUE(verify(p.d).empty());
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx(k::OutputDestinations{output.file, output.file});
    const auto result = kirimasim::vm::run(p.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
}

class RejectMemory final : public std::pmr::memory_resource {
    void* do_allocate(std::size_t, std::size_t) override { throw std::bad_alloc{}; }
    void do_deallocate(void*, std::size_t, std::size_t) override {}
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(PatternSir, ScratchAllocationFailureIsAResourceError) {
    Program p;
    const auto layout = p.leaf(u2(32));
    p.print(p.format(layout, {p.constant(u2(32), {7})}));
    p.end();
    ASSERT_TRUE(verify(p.d).empty());
    Capture output;
    ASSERT_NE(output.file, nullptr);
    RejectMemory memory;
    k::Context ctx(k::OutputDestinations{output.file, output.file}, nullptr, &memory);
    const auto result = kirimasim::vm::run(p.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
    EXPECT_NE(output.text().find("pattern formatting allocation limit"), std::string::npos);
}
}  // namespace
