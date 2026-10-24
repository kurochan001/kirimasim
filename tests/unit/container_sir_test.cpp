// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Typed dynamic-container SIR (7.5, 7.10; SIR_SPEC 4.2.1/5.8).
// The tests build SIR independently of front, so malformed descriptors and
// operands are checked even before a language producer can make them.
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"

namespace {
using namespace kirimasim::sir;

Instr make(Op op, Type type, TempId result, std::vector<Operand> operands) {
    return {op, type, result, std::move(operands), {0, 1, 1}};
}
Instr none(Op op, std::vector<Operand> operands = {}) {
    return make(op, voidType(), kNone, std::move(operands));
}
std::string errorsOf(const Design& d) {
    std::string text;
    for (const auto& e : verify(d))
        text += e.where + ": " + e.message + "\n";
    return text;
}

Design playground(Type element = s4(32), std::uint64_t capacity = 3) {
    Design d;
    d.sources = {"container.sv"};
    d.instances = {{"top", "top", kNone}};
    const Type a = dynarrayType(TypeId{0}), q = queueType(TypeId{1}), nested = queueType(TypeId{2});
    TypeEntry array;
    array.kind = TypeEntryKind::DynArray;
    array.name = "array";
    array.value = element;
    TypeEntry queue;
    queue.kind = TypeEntryKind::Queue;
    queue.name = "queue";
    queue.value = element;
    queue.maxElements = capacity;
    TypeEntry outer;
    outer.kind = TypeEntryKind::Queue;
    outer.name = "nested";
    outer.value = q;
    outer.maxElements = 2;
    d.types = {array, queue, outer};
    d.slots = {{"top.a", a}, {"top.q", q}, {"top.nested", nested}};
    d.constants = {{s4(64), {3}, {0}}, {a, {0}, {}}, {q, {0}, {}}};
    Constant data;
    data.type = element;
    if (element.kind == TypeKind::String)
        data.bytes = "payload";
    else {
        const std::size_t words = isIntegral(element) ? (element.width + 63) / 64 : 1;
        data.aWords.assign(words, 0);
        if (isFourState(element)) data.bWords.assign(words, 0);
    }
    d.constants.push_back(data);
    const Operand size = Operand::constant(ConstId{0}), empty = Operand::constant(ConstId{1}),
                  item = Operand::constant(ConstId{3});
    Subroutine sub;
    sub.name = "top.peek";
    sub.automatic = true;
    sub.returnType = q;
    sub.params = {{"receiver", Direction::ConstRef, q, kNone, FrameVarId{0}}};
    FrameVar formal;
    formal.name = "receiver";
    formal.type = refType(q.detail);
    formal.target = q;
    sub.frame = {formal};
    sub.numTemps = 2;
    sub.loc = {0, 1, 1};
    sub.blocks.resize(1);
    sub.blocks[0].instrs = {
        make(Op::Fload, refType(q.detail), TempId{0}, {Operand::frame(FrameVarId{0})}),
        make(Op::LoadRef, q, TempId{1}, {Operand::temp(TempId{0})}),
        none(Op::Ret, {Operand::temp(TempId{1})})};
    d.subroutines.push_back(std::move(sub));
    Process p;
    p.name = "top.initial";
    p.loc = {0, 1, 1};
    p.writes = {{SlotId{0}}, {SlotId{1}}, {SlotId{2}}};
    p.frame = {{"a", a}, {"q", q}, formal};
    p.blocks.resize(2);
    auto& ins = p.blocks[0].instrs;
    const auto emit = [&](Op op, Type type, std::vector<Operand> operands) {
        const TempId r{p.numTemps++};
        ins.push_back(make(op, type, r, std::move(operands)));
        return Operand::temp(r);
    };
    const auto emitVoid = [&](Op op, std::vector<Operand> operands) {
        ins.push_back(none(op, std::move(operands)));
    };
    const Operand av = emit(Op::ContainerNew, a, {size, empty});
    const Operand qv = emit(Op::ContainerCopy, q, {av});
    emit(Op::ContainerSnapshot, q, {qv});
    emit(Op::ContainerPack, q, {item, item});
    emit(Op::ContainerPack, q, {});
    emitVoid(Op::Store, {Operand::slot(SlotId{0}), av});
    emitVoid(Op::Fstore, {Operand::frame(FrameVarId{1}), qv});
    emit(Op::Fload, q, {Operand::frame(FrameVarId{1})});
    emit(Op::ContainerSize, u2(64), {qv});
    emit(Op::ContainerRead, element, {qv, size});
    const Operand ar = emit(Op::RefMake, refType(a.detail), {Operand::slot(SlotId{0})});
    const Operand qr = emit(Op::RefMake, refType(q.detail), {Operand::frame(FrameVarId{1})});
    emitVoid(Op::Fstore, {Operand::frame(FrameVarId{2}), qr});
    emit(Op::RefMake, refType(q.detail), {Operand::frame(FrameVarId{2})});
    emitVoid(Op::ContainerAssign, {Operand::typeRow(a.detail), ar, qv});
    emitVoid(Op::ContainerAssign, {Operand::typeRow(q.detail), qr, av});
    emitVoid(Op::ContainerWrite, {Operand::typeRow(a.detail), ar, size, item});
    const Operand cell = emit(Op::ContainerCell, refType(), {Operand::typeRow(q.detail), qr, size});
    emit(Op::LoadRef, element, {cell});
    emitVoid(Op::StoreRef, {cell, item});
    emit(Op::LoadRef, a, {ar});
    emitVoid(Op::StoreRef, {ar, av});
    emitVoid(Op::QueueInsert, {Operand::typeRow(q.detail), qr, size, item});
    emitVoid(Op::QueuePushFront, {Operand::typeRow(q.detail), qr, item});
    emitVoid(Op::QueuePushBack, {Operand::typeRow(q.detail), qr, item});
    emit(Op::QueuePopFront, element, {Operand::typeRow(q.detail), qr});
    emit(Op::QueuePopBack, element, {Operand::typeRow(q.detail), qr});
    emitVoid(Op::QueueErase, {Operand::typeRow(q.detail), qr, size});
    emitVoid(Op::ContainerClear, {Operand::typeRow(a.detail), ar});
    emitVoid(Op::ContainerClear, {Operand::typeRow(q.detail), qr});
    const Operand nr = emit(Op::RefMake, refType(nested.detail), {Operand::slot(SlotId{2})});
    emitVoid(Op::QueuePushBack, {Operand::typeRow(nested.detail), nr, qv});
    const Operand nestedCell =
        emit(Op::ContainerCell, refType(q.detail), {Operand::typeRow(nested.detail), nr, size});
    emitVoid(Op::QueuePushBack, {Operand::typeRow(q.detail), nestedCell, item});
    emit(Op::Call, q, {Operand::subroutine(SubroutineId{0}), qr});
    const TempId param{p.numTemps++};
    emitVoid(Op::Br, {Operand::block(BlockId{1}, 1), av});
    p.blocks[1].params = {{param, a}};
    p.blocks[1].instrs = {none(Op::Store, {Operand::slot(SlotId{0}), Operand::temp(param)}),
                          none(Op::Ret)};
    d.processes.push_back(std::move(p));
    return d;
}

Instr& instruction(Design& d, Op op) {
    auto& instructions = d.processes[0].blocks[0].instrs;
    const auto found = std::find_if(instructions.begin(), instructions.end(),
                                    [&](const Instr& i) { return i.op == op; });
    EXPECT_NE(found, instructions.end());
    return *found;
}

TEST(ContainerSir, ScalarWidthsDomainsAndNestedBindingsVerify) {
    std::vector<Type> types{{TypeKind::Real, 0},
                            {TypeKind::ShortReal, 0},
                            {TypeKind::String, 0},
                            {TypeKind::Chandle, 0}};
    types.reserve(24);
    for (const unsigned width : {1U, 32U, 64U, 65U, 129U})
        for (const Type type : {u2(width), s2(width), u4(width), s4(width)})
            types.push_back(type);
    for (const Type type : types)
        for (const std::uint64_t capacity : {0ULL, 1ULL, 3ULL, (1ULL << 40) + 1}) {
            SCOPED_TRACE(toString(type));
            SCOPED_TRACE(capacity);
            const auto d = playground(type, capacity);
            ASSERT_EQ(errorsOf(d), "");
            EXPECT_EQ(toText(d).find('?'), std::string::npos);
        }
}

TEST(ContainerSir, TextKeepsDescriptorsBoundsAndReceiverReferences) {
    const auto d = playground();
    ASSERT_EQ(errorsOf(d), "");
    const std::string text = toText(d);
    for (const std::string word :
         {"dynarray<!0>", "queue<!1>", "ref<!1>", "capacity=3", "container.new",
          "container.snapshot queue<!1>", "container.assign !0", "container.pack", "container.size",
          "container.read", "container.cell ref !1", "container.write !0", "container.clear !1",
          "queue.insert !1", "queue.push.front !1", "queue.push.back !1",
          "queue.pop.front s4<32> !1", "queue.pop.back s4<32> !1", "queue.erase !1"})
        EXPECT_NE(text.find(word), std::string::npos) << word;
    EXPECT_EQ(text, toText(d));
    EXPECT_EQ(domainOf(dynarrayType(TypeId{0})), Domain::Handle);
    EXPECT_EQ(domainOf(queueType(TypeId{1})), Domain::Handle);
    EXPECT_EQ(toString(refType()), "ref");
    EXPECT_EQ(constantText(d.constants[1]), "empty");
    EXPECT_EQ(constantText(d.constants[2]), "empty");
    EXPECT_EQ(constantText({{TypeKind::Chandle, 0}, {0}, {}}), "null");
    EXPECT_EQ(text.find('?'), std::string::npos);
}

TEST(ContainerSir, ContainerArgumentsTransportValuesOrTypedReferences) {
    for (const Direction dir : {Direction::Input, Direction::Ref, Direction::ConstRef}) {
        SCOPED_TRACE(static_cast<unsigned>(dir));
        auto d = playground();
        auto& sub = d.subroutines[0];
        sub.params[0].dir = dir;
        if (dir == Direction::Input) {
            const Type q = queueType(TypeId{1});
            sub.frame[0].type = q;
            sub.frame[0].target = voidType();
            sub.numTemps = 1;
            sub.blocks[0].instrs = {make(Op::Fload, q, TempId{0}, {Operand::frame(FrameVarId{0})}),
                                    none(Op::Ret, {Operand::temp(TempId{0})})};
            instruction(d, Op::Call).operands[1] =
                Operand::temp(instruction(d, Op::ContainerCopy).result);
        }
        ASSERT_EQ(errorsOf(d), "");
    }
}

TEST(ContainerSir, EquivalentQueueBoundsPassThroughReferenceArguments) {
    for (const bool nested : {false, true}) {
        auto d = playground();
        TypeEntry view = d.types[1];
        view.name = "formal_view";
        view.maxElements = 9;
        d.types.push_back(view);
        Type formal = queueType(TypeId{3});
        Type actual = queueType(TypeId{1});
        if (nested) {
            TypeEntry outer = d.types[2];
            outer.name = "formal_outer";
            outer.value = formal;
            outer.maxElements = 7;
            d.types.push_back(outer);
            formal = queueType(TypeId{4});
            actual = queueType(TypeId{2});
        }
        auto& sub = d.subroutines[0];
        sub.params[0].type = formal;
        sub.returnType = formal;
        sub.frame[0].type = refType(formal.detail);
        sub.frame[0].target = formal;
        sub.blocks[0].instrs[0].type = refType(formal.detail);
        sub.blocks[0].instrs[1].type = formal;
        auto& call = instruction(d, Op::Call);
        call.type = formal;
        for (const auto& i : d.processes[0].blocks[0].instrs)
            if (i.op == Op::RefMake && i.type == refType(actual.detail)) {
                call.operands[1] = Operand::temp(i.result);
                break;
            }
        ASSERT_EQ(errorsOf(d), "");
    }
}

TEST(ContainerSir, NestedQueueEquivalenceIgnoresCapacityAndStaysIterative) {
    auto d = playground();
    Type a = queueType(TypeId{1}), b = a;
    constexpr unsigned depth = 2500;
    for (unsigned n = 0; n < depth; ++n) {
        TypeEntry left;
        left.kind = TypeEntryKind::Queue;
        left.name = "left";
        left.value = a;
        left.maxElements = 2;
        const TypeId l{static_cast<std::uint32_t>(d.types.size())};
        d.types.push_back(std::move(left));
        TypeEntry right;
        right.kind = TypeEntryKind::Queue;
        right.name = "right";
        right.value = b;
        right.maxElements = 7;
        const TypeId r{static_cast<std::uint32_t>(d.types.size())};
        d.types.push_back(std::move(right));
        a = queueType(l);
        b = queueType(r);
    }
    ASSERT_EQ(errorsOf(d), "");
    EXPECT_TRUE(equivalentValueTypes(d, a, b));
    EXPECT_TRUE(equivalentValueTypes(d, refType(a.detail), refType(b.detail)));
    EXPECT_FALSE(equivalentValueTypes(d, a, dynarrayType(TypeId{0})));
    EXPECT_FALSE(equivalentValueTypes(d, a, queueType(TypeId{100000})));
    EXPECT_FALSE(equivalentValueTypes(d, u4(32), s4(32)));
    EXPECT_FALSE(equivalentValueTypes(d, u4(32), u2(32)));
    d.types[a.detail.index()].value = a;
    d.types[b.detail.index()].value = b;
    EXPECT_FALSE(equivalentValueTypes(d, a, b));
    EXPECT_NE(errorsOf(d).find("nested container descriptor must precede its parent"),
              std::string::npos);
}

TEST(ContainerSir, FixedContainerArrayRefArgumentsKeepTheirShape) {
    auto d = playground();
    const Type q = queueType(TypeId{1});
    FrameVar array;
    array.name = "queues";
    array.type = q;
    array.dims = {{0, 2}};
    d.processes[0].frame.push_back(array);
    auto& sub = d.subroutines[0];
    sub.frame[0].type = refType();
    sub.frame[0].dims = array.dims;
    sub.returnType = voidType();
    sub.numTemps = 0;
    sub.blocks[0].instrs = {none(Op::Ret)};
    const TempId r{d.processes[0].numTemps++};
    auto& ins = d.processes[0].blocks[0].instrs;
    const auto pos =
        std::find_if(ins.begin(), ins.end(), [](const Instr& i) { return i.op == Op::Call; });
    ins.insert(pos, make(Op::RefMake, refType(), r, {Operand::frame(FrameVarId{3})}));
    auto& call = instruction(d, Op::Call);
    call.type = voidType();
    call.result = kNone;
    call.operands[1] = Operand::temp(r);
    ASSERT_EQ(errorsOf(d), "");
}

TEST(ContainerSir, NestedValueCopiesAndAssignmentsAcceptEquivalentElementBounds) {
    auto d = playground();
    TypeEntry inner = d.types[1];
    inner.name = "wide_inner";
    inner.maxElements = 7;
    d.types.push_back(inner);
    TypeEntry outer = d.types[2];
    outer.name = "wide_outer";
    outer.value = queueType(TypeId{3});
    outer.maxElements = 5;
    d.types.push_back(outer);
    const Type target = queueType(TypeId{4});
    d.slots.push_back({"top.destination", target});
    d.processes[0].writes.push_back({SlotId{3}});
    auto& p = d.processes[0];
    const Operand source = Operand::temp(instruction(d, Op::ContainerCopy).result);
    auto& ins = p.blocks[0].instrs;
    const Instr last = ins.back();
    ins.pop_back();
    const auto emit = [&](Op op, Type type, std::vector<Operand> args) {
        const TempId id{p.numTemps++};
        ins.push_back(make(op, type, id, std::move(args)));
        return Operand::temp(id);
    };
    const Operand original = emit(Op::ContainerPack, queueType(TypeId{2}), {source});
    emit(Op::ContainerCopy, target, {original});
    emit(Op::ContainerPack, target, {source});
    const Operand binding = emit(Op::RefMake, refType(target.detail), {Operand::slot(SlotId{3})});
    ins.push_back(none(Op::ContainerAssign, {Operand::typeRow(target.detail), binding, original}));
    ins.push_back(none(Op::QueuePushBack, {Operand::typeRow(target.detail), binding, source}));
    ins.push_back(none(Op::ContainerWrite, {Operand::typeRow(target.detail), binding,
                                            Operand::constant(ConstId{0}), source}));
    ins.push_back(last);
    ASSERT_EQ(errorsOf(d), "");
}

TEST(ContainerSir, FixedArraysOfContainersRequireAnElementReceiver) {
    for (const bool frame : {false, true}) {
        SCOPED_TRACE(frame);
        auto d = playground();
        const std::uint32_t home = frame ? 1U : 0U;
        if (frame)
            d.processes[0].frame[home].dims = {{3, 1}};
        else
            d.slots[home].dims = {{3, 1}};
        Instr* receiver = nullptr;
        for (auto& i : d.processes[0].blocks[0].instrs) {
            const bool place =
                !i.operands.empty() && i.operands[0].id == home &&
                i.operands[0].kind == (frame ? Operand::Kind::Frame : Operand::Kind::Slot);
            if (!place) continue;
            if (i.op == Op::RefMake) {
                i.operands.push_back(Operand::constant(ConstId{0}));
                receiver = &i;
            } else if (frame && (i.op == Op::Fstore || i.op == Op::Fload)) {
                i.op = i.op == Op::Fstore ? Op::StoreElem : Op::LoadElem;
                i.operands.insert(i.operands.begin() + 1, Operand::constant(ConstId{0}));
            } else if (!frame && i.op == Op::Store)
                i.operands[0].numElems = 1;
        }
        if (!frame) d.processes[0].blocks[1].instrs[0].operands[0].numElems = 1;
        ASSERT_NE(receiver, nullptr);
        ASSERT_EQ(errorsOf(d), "");
        receiver->operands.pop_back();
        EXPECT_NE(errorsOf(d).find("ref result does not describe its target"), std::string::npos);
    }
}

enum class Broken : std::uint8_t {
    MissingDescriptor,
    WrongKind,
    ContainerWidth,
    ScalarDetail,
    NonContainerRef,
    FrameTarget,
    ConstantDescriptor,
    ConstantNonempty,
    ConstantB,
    ConstantBytes,
    RefResult,
    LoadThroughRef,
    StoreThroughRef,
    CallReceiver,
    NewSize,
    NewInitializer,
    NewResult,
    CopyElement,
    PackElement,
    SizeResult,
    ReadResult,
    ReadIndex,
    ReceiverTypeRow,
    ReceiverRef,
    ReceiverRawValue,
    ReceiverIndex,
    WriteElement,
    QueueOnArray,
    PopResult,
    CellResult,
    NestedCellResult,
    ExtraArgument,
    MissingArgument,
    SelfCycle,
    ForwardCycle,
    NonQueueBound,
    UnsupportedElement,
    BadElementWidth,
    UnrelatedFields,
    NetContainer,
    ReturnDescriptor,
    BlockDescriptor,
    ParameterDescriptor,
    FormalRefDescriptor,
    UntypedContainerFrameRef,
    SnapshotType,
    AssignmentSource,
    SnapshotBoundType,
    AssignmentElementType
};

void breakDesign(Design& d, Broken c) {
    switch (c) {
        case Broken::MissingDescriptor: d.slots[0].type.detail = kNone; break;
        case Broken::WrongKind: d.slots[0].type.detail = TypeId{1}; break;
        case Broken::ContainerWidth: d.slots[0].type.width = 1; break;
        case Broken::ScalarDetail: d.constants[0].type.detail = TypeId{0}; break;
        case Broken::NonContainerRef: {
            TypeEntry row;
            row.kind = TypeEntryKind::Array;
            row.name = "fixed";
            row.dims = {{0, 1}};
            d.types.push_back(std::move(row));
            d.processes[0].frame[2].type.detail = TypeId{3};
            break;
        }
        case Broken::FrameTarget: d.processes[0].frame[2].target = dynarrayType(TypeId{0}); break;
        case Broken::ConstantDescriptor: d.constants[1].type.detail = TypeId{100}; break;
        case Broken::ConstantNonempty: d.constants[1].aWords[0] = 1; break;
        case Broken::ConstantB: d.constants[1].bWords = {0}; break;
        case Broken::ConstantBytes: d.constants[1].bytes = "not a container"; break;
        case Broken::RefResult: instruction(d, Op::RefMake).type = refType(); break;
        case Broken::LoadThroughRef:
            instruction(d, Op::LoadRef).operands[0] =
                Operand::temp(instruction(d, Op::RefMake).result);
            break;
        case Broken::StoreThroughRef:
            instruction(d, Op::StoreRef).operands[0] =
                Operand::temp(instruction(d, Op::RefMake).result);
            break;
        case Broken::CallReceiver:
            instruction(d, Op::Call).operands[1] =
                Operand::temp(instruction(d, Op::RefMake).result);
            break;
        case Broken::NewSize:
            instruction(d, Op::ContainerNew).operands[0] = Operand::constant(ConstId{3});
            break;
        case Broken::NewInitializer:
            instruction(d, Op::ContainerNew).operands[1] = Operand::constant(ConstId{3});
            break;
        case Broken::NewResult: instruction(d, Op::ContainerNew).type = queueType(TypeId{1}); break;
        case Broken::CopyElement: d.types[1].value = u4(8); break;
        case Broken::PackElement:
            instruction(d, Op::ContainerPack).operands[0] = Operand::constant(ConstId{0});
            break;
        case Broken::SizeResult: instruction(d, Op::ContainerSize).type = s4(32); break;
        case Broken::ReadResult: instruction(d, Op::ContainerRead).type = u2(1); break;
        case Broken::ReadIndex:
            instruction(d, Op::ContainerRead).operands[1] = Operand::constant(ConstId{1});
            break;
        case Broken::ReceiverTypeRow:
            instruction(d, Op::QueuePushFront).operands[0] = Operand::constant(ConstId{0});
            break;
        case Broken::ReceiverRef:
            instruction(d, Op::QueuePushFront).operands[1] =
                Operand::temp(instruction(d, Op::RefMake).result);
            break;
        case Broken::ReceiverRawValue:
            instruction(d, Op::QueuePushFront).operands[1] =
                Operand::temp(instruction(d, Op::ContainerCopy).result);
            break;
        case Broken::ReceiverIndex:
            instruction(d, Op::QueueInsert).operands[2] = Operand::constant(ConstId{1});
            break;
        case Broken::WriteElement:
            instruction(d, Op::QueuePushFront).operands[2] = Operand::constant(ConstId{0});
            break;
        case Broken::QueueOnArray: instruction(d, Op::ContainerWrite).op = Op::QueueInsert; break;
        case Broken::PopResult: instruction(d, Op::QueuePopFront).type = u2(8); break;
        case Broken::CellResult: instruction(d, Op::ContainerCell).type = refType(TypeId{1}); break;
        case Broken::NestedCellResult: {
            auto& ins = d.processes[0].blocks[0].instrs;
            for (auto& i : ins)
                if (i.op == Op::ContainerCell && i.operands[0].id == 2) {
                    i.type = refType();
                    break;
                }
            break;
        }
        case Broken::ExtraArgument:
            instruction(d, Op::QueuePopFront).operands.push_back(Operand::constant(ConstId{0}));
            break;
        case Broken::MissingArgument: instruction(d, Op::QueuePushFront).operands.pop_back(); break;
        case Broken::SelfCycle: d.types[2].value = queueType(TypeId{2}); break;
        case Broken::ForwardCycle: d.types[0].value = queueType(TypeId{2}); break;
        case Broken::NonQueueBound: d.types[0].maxElements = 4; break;
        case Broken::UnsupportedElement: d.types[0].value = {TypeKind::Event, 0}; break;
        case Broken::BadElementWidth: d.types[0].value = u4(0); break;
        case Broken::UnrelatedFields: d.types[0].dims = {{0, 1}}; break;
        case Broken::NetContainer: d.slots[0].kind = SlotKind::Wire; break;
        case Broken::ReturnDescriptor: d.subroutines[0].returnType.detail = TypeId{100}; break;
        case Broken::BlockDescriptor: d.processes[0].blocks[1].params[0].type.detail = kNone; break;
        case Broken::ParameterDescriptor:
            d.subroutines[0].params[0].type.detail = TypeId{100};
            break;
        case Broken::FormalRefDescriptor:
            d.subroutines[0].params[0].type = dynarrayType(TypeId{0});
            break;
        case Broken::UntypedContainerFrameRef: d.processes[0].frame[2].type = refType(); break;
        case Broken::SnapshotType:
            instruction(d, Op::ContainerSnapshot).type = dynarrayType(TypeId{0});
            break;
        case Broken::AssignmentSource:
            instruction(d, Op::ContainerAssign).operands[2] = Operand::constant(ConstId{3});
            break;
        case Broken::SnapshotBoundType: {
            auto row = d.types[1];
            row.maxElements = 7;
            d.types.push_back(row);
            instruction(d, Op::ContainerSnapshot).type = queueType(TypeId{3});
            break;
        }
        case Broken::AssignmentElementType: {
            TypeEntry row;
            row.kind = TypeEntryKind::DynArray;
            row.name = "bytes";
            row.value = u4(8);
            d.types.push_back(row);
            const auto id = ConstId{static_cast<std::uint32_t>(d.constants.size())};
            d.constants.push_back({dynarrayType(TypeId{3}), {0}, {}});
            instruction(d, Op::ContainerAssign).operands[2] = Operand::constant(id);
            break;
        }
    }
}

std::string reason(Broken c) {
    switch (c) {
        case Broken::MissingDescriptor:
        case Broken::WrongKind:
        case Broken::ContainerWidth:
        case Broken::ConstantDescriptor:
        case Broken::ReturnDescriptor:
        case Broken::BlockDescriptor:
        case Broken::ParameterDescriptor: return "container type needs its matching descriptor";
        case Broken::ScalarDetail:
        case Broken::NonContainerRef:
            return "only container values and refs may name a container descriptor";
        case Broken::FrameTarget:
        case Broken::UntypedContainerFrameRef:
            return "container ref target differs from its descriptor";
        case Broken::FormalRefDescriptor: return "frame variable is invalid";
        case Broken::SnapshotType:
        case Broken::SnapshotBoundType: return "snapshot must preserve its source value type";
        case Broken::AssignmentSource:
        case Broken::AssignmentElementType:
            return "assignment source must have an equivalent element type";
        case Broken::ConstantNonempty:
        case Broken::ConstantB:
        case Broken::ConstantBytes: return "a container constant must be empty";
        case Broken::RefResult: return "ref result does not describe its target";
        case Broken::LoadThroughRef:
        case Broken::StoreThroughRef:
            return "container ref access must use its target type without a leaf";
        case Broken::CallReceiver: return "argument 'receiver' must be a ref";
        case Broken::NewSize:
        case Broken::NewResult:
            return "construction needs a dynamic array and a signed four-state 64-bit size";
        case Broken::NewInitializer:
        case Broken::CopyElement: return "source must be a container with the same element type";
        case Broken::PackElement: return "packed value must have the container element type";
        case Broken::SizeResult:
        case Broken::ReadResult: return "incorrect container result type";
        case Broken::ReadIndex:
        case Broken::ReceiverIndex: return "element index must be integral";
        case Broken::ReceiverTypeRow: return "needs a container type row";
        case Broken::ReceiverRef:
        case Broken::ReceiverRawValue:
            return "receiver must be a captured ref to this container type";
        case Broken::WriteElement: return "written value must have the container element type";
        case Broken::QueueOnArray: return "method needs a queue receiver";
        case Broken::PopResult: return "pop result must have the container element type";
        case Broken::CellResult:
        case Broken::NestedCellResult: return "cell ref result must describe the element target";
        case Broken::ExtraArgument:
        case Broken::MissingArgument: return "expects ";
        case Broken::SelfCycle:
        case Broken::ForwardCycle: return "nested container descriptor must precede its parent";
        case Broken::NonQueueBound: return "only a queue descriptor has a capacity";
        case Broken::UnsupportedElement:
        case Broken::BadElementWidth: return "container element must be an implemented value type";
        case Broken::UnrelatedFields: return "container descriptor has unrelated aggregate data";
        case Broken::NetContainer: return "a container slot must be a variable";
    }
    return "unhandled negative case";
}

TEST(ContainerSir, RejectsMalformedDescriptorsOperandsAndTypedReferences) {
    for (unsigned v = 0; v <= static_cast<unsigned>(Broken::AssignmentElementType); ++v) {
        SCOPED_TRACE(v);
        auto d = playground();
        ASSERT_EQ(errorsOf(d), "");
        breakDesign(d, static_cast<Broken>(v));
        EXPECT_NE(errorsOf(d).find(reason(static_cast<Broken>(v))), std::string::npos)
            << errorsOf(d);
    }
}

TEST(ContainerSir, MutationClassificationNamesOnlyActualHeapChanges) {
    for (const Op op : {Op::ContainerAssign, Op::ContainerWrite, Op::ContainerClear,
                        Op::QueueInsert, Op::QueuePushFront, Op::QueuePushBack, Op::QueuePopFront,
                        Op::QueuePopBack, Op::QueueErase}) {
        EXPECT_TRUE(isContainerOp(op));
        EXPECT_TRUE(isContainerMutation(op));
        EXPECT_FALSE(isTerminator(op));
        EXPECT_FALSE(isWait(none(op)));
    }
    for (const Op op :
         {Op::ContainerNew, Op::ContainerCopy, Op::ContainerSnapshot, Op::ContainerPack,
          Op::ContainerSize, Op::ContainerRead, Op::ContainerCell}) {
        EXPECT_TRUE(isContainerOp(op));
        EXPECT_FALSE(isContainerMutation(op));
        EXPECT_FALSE(isTerminator(op));
        EXPECT_FALSE(isWait(none(op)));
    }
    for (const Op op : {Op::Store, Op::StoreRef, Op::Builtin, Op::Br, Op::WaitDelay}) {
        EXPECT_FALSE(isContainerOp(op));
        EXPECT_FALSE(isContainerMutation(op));
    }
}

TEST(ContainerSir, SwapRequiresCapturedTypedReceiverAndTwoIntegralIndices) {
    auto design = playground();
    auto& process = design.processes[0];
    auto& instructions = process.blocks[0].instrs;
    const Type queue = queueType(TypeId{1});
    const TempId ref{process.numTemps++};
    instructions.insert(instructions.begin(), make(Op::RefMake, refType(queue.detail), ref,
                                                   {Operand::frame(FrameVarId{1})}));
    const auto position = instructions.begin() + 1;
    instructions.insert(position,
                        none(Op::ContainerSwapValues,
                             {Operand::typeRow(queue.detail), Operand::temp(ref),
                              Operand::constant(ConstId{0}), Operand::constant(ConstId{0})}));
    ASSERT_EQ(errorsOf(design), "");
    EXPECT_TRUE(isContainerMutation(Op::ContainerSwapValues));
    EXPECT_TRUE(isContainerOp(Op::ContainerSwapValues));
    EXPECT_FALSE(isTerminator(Op::ContainerSwapValues));
    EXPECT_FALSE(isWait(none(Op::ContainerSwapValues)));
    EXPECT_NE(toText(design).find("container.swap.values"), std::string::npos);
    for (unsigned broken = 0; broken < 6; ++broken) {
        auto d = design;
        auto& swap = instruction(d, Op::ContainerSwapValues);
        switch (broken) {
            case 0: swap.operands.pop_back(); break;
            case 1: swap.operands.push_back(Operand::constant(ConstId{0})); break;
            case 2: swap.operands[2] = Operand::constant(ConstId{1}); break;
            case 3: swap.operands[3] = Operand::constant(ConstId{2}); break;
            case 4: swap.operands[0] = Operand::typeRow(TypeId{0}); break;
            case 5:
                swap.type = s4(32);
                swap.result = TempId{d.processes[0].numTemps++};
                break;
            default: ADD_FAILURE() << broken; break;
        }
        EXPECT_FALSE(errorsOf(d).empty()) << broken;
    }
}

}  // namespace
