// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Typed dynamic-container producers (SIR_SPEC 5.8). The shared heap
// decides copying, bounds, stable cells and method diagnostics.
#include <algorithm>
#include <stdexcept>

#include "lowerer.h"

#include "slang/ast/ASTVisitor.h"

#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"

namespace kirimasim::front {
namespace {
using sir::Op;
using sir::Operand;
using sir::Type;

const ast::Expression* methodReceiver(const ast::Expression* e) {
    if (e->kind == ast::ExpressionKind::Assignment &&
        e->as<ast::AssignmentExpression>().isLValueArg())
        return &e->as<ast::AssignmentExpression>().left();
    return e;
}
}  // namespace

bool isDynamicContainer(const ast::Type& type) {
    const auto kind = type.getCanonicalType().kind;
    return kind == ast::SymbolKind::DynamicArrayType || kind == ast::SymbolKind::QueueType;
}

Type Lowerer::containerType(const ast::Type& type, slang::SourceLocation sl) {
    checkStack(sl);
    const auto& canonical = type.getCanonicalType();
    const bool queue = canonical.kind == ast::SymbolKind::QueueType;
    const auto& element = *canonical.getArrayElementType();
    const Type value = typeOf(element, sl);
    if (!sir::isIntegral(value) && value.kind != sir::TypeKind::Real &&
        value.kind != sir::TypeKind::ShortReal && value.kind != sir::TypeKind::String &&
        value.kind != sir::TypeKind::Chandle && !sir::isContainer(value))
        unsupported(sl, "dynamic container element of type '" + element.toString() + "'");
    const auto bound = queue ? canonical.as<ast::QueueType>().maxBound : 0;
    const std::uint64_t capacity = bound == 0 ? 0 : std::uint64_t{bound} + 1;
    const auto kind = queue ? sir::TypeEntryKind::Queue : sir::TypeEntryKind::DynArray;
    for (std::uint32_t i = 0; i < design_.types.size(); ++i) {
        const auto& row = design_.types[i];
        if (row.kind == kind && row.value == value && row.maxElements == capacity)
            return queue ? sir::queueType(sir::TypeId{i}) : sir::dynarrayType(sir::TypeId{i});
    }
    sir::TypeEntry row;
    row.kind = kind;
    row.name = "$container" + std::to_string(design_.types.size());
    row.value = value;
    row.maxElements = capacity;
    const sir::TypeId id{static_cast<std::uint32_t>(design_.types.size())};
    design_.types.push_back(std::move(row));
    return queue ? sir::queueType(id) : sir::dynarrayType(id);
}

// Queue bounds constrain writes to variables (7.10.5), not the temporary
// element sequence of a pattern or constructor. Ref formals can have bounds
// different from the actual destination, including at nested levels.
Type Lowerer::unboundedContainerType(Type type, slang::SourceLocation sl) {
    checkStack(sl);
    if (!sir::isContainer(type)) return type;
    auto row = design_.types[type.detail.index()];
    row.value = unboundedContainerType(row.value, sl);
    row.maxElements = 0;
    for (std::uint32_t i = 0; i < design_.types.size(); ++i) {
        const auto& candidate = design_.types[i];
        if (candidate.kind == row.kind && candidate.value == row.value &&
            candidate.maxElements == 0) {
            type.detail = sir::TypeId{i};
            return type;
        }
    }
    row.name = "$container" + std::to_string(design_.types.size());
    type.detail = sir::TypeId{static_cast<std::uint32_t>(design_.types.size())};
    design_.types.push_back(std::move(row));
    return type;
}

Val Lowerer::snapshotValue(const Val& value, slang::SourceLocation sl) {
    return sir::isContainer(value.type) ? emit(Op::ContainerSnapshot, value.type, {value.op}, sl)
                                        : value;
}
Val Lowerer::copyValue(const Val& value, Type destination, slang::SourceLocation sl) {
    if (sir::isContainer(destination)) return emit(Op::ContainerCopy, destination, {value.op}, sl);
    return coerce(value, destination, sl);
}

Val Lowerer::referencePlace(const Place& place, bool isConst, slang::SourceLocation sl) {
    const auto type =
        sir::refType(sir::isContainer(place.type) ? place.type.detail : sir::TypeId{sir::kNone});
    if (place.home == Place::Home::Heap) {
        if (!place.receiver) throw std::logic_error("missing heap receiver in lowering");
        if (place.kind != Place::Kind::Element)
            unsupported(sl, "reference to a packed part of a dynamic element");
        return emit(
            Op::ContainerCell, type,
            {Operand::typeRow(place.container.detail), place.receiver->op, place.indices[0]}, sl);
    }
    const bool frame = place.kind == Place::Kind::Frame || place.kind == Place::Kind::Ref ||
                       (place.kind == Place::Kind::Element && place.home != Place::Home::Slot);
    if (place.kind != Place::Kind::Frame && place.kind != Place::Kind::Ref &&
        place.kind != Place::Kind::Slot && place.kind != Place::Kind::Element)
        unsupported(sl, "container receiver of this target");
    std::vector<Operand> args{frame ? Operand::frame(place.frame) : Operand::slot(place.slot)};
    args.insert(args.end(), place.indices.begin(), place.indices.end());
    if (!frame) {
        noteRead(rangeOf(place));
        if (!isConst) noteWrite(rangeOf(place));
    }
    return emit(Op::RefMake, type, std::move(args), sl);
}

Val Lowerer::containerReference(const ast::Expression& e, bool isConst, slang::SourceLocation sl) {
    return referencePlace(placeOf(e, sl), isConst, sl);
}

Val Lowerer::containerIndex(const ast::Expression& e, const Val& receiver, Type type,
                            slang::SourceLocation sl) {
    bool last = false;
    auto visitor = ast::makeVisitor([&](auto&, const ast::UnboundedLiteral&) { last = true; },
                                    [](auto&, const ast::ElementSelectExpression&) {});
    e.visit(visitor);
    if (!last) return lowerExpr(e);
    const auto value = receiver.type.kind == sir::TypeKind::Ref
                           ? emit(Op::LoadRef, type, {receiver.op}, sl)
                           : receiver;
    const auto size = emit(Op::ContainerSize, sir::u2(64), {value.op}, sl);
    queueLast_.push_back(emit(Op::Sub, sir::s4(64),
                              {coerce(size, sir::s4(64), sl).op, constant(sir::s4(64), 1).op}, sl));
    try {
        const auto index = lowerExpr(e);
        queueLast_.pop_back();
        return index;
    } catch (...) {
        queueLast_.pop_back();
        throw;
    }
}

// Unbounded construction buffers preserve the complete operand sequence;
// language assignments apply the actual destination's bounds afterwards.
Type Lowerer::sequenceBufferType(Type element) {
    for (std::uint32_t i = 0; i < design_.types.size(); ++i) {
        const auto& row = design_.types[i];
        if (row.kind == sir::TypeEntryKind::Queue && row.value == element && row.maxElements == 0)
            return sir::queueType(sir::TypeId{i});
    }
    const auto id = sir::TypeId{static_cast<std::uint32_t>(design_.types.size())};
    sir::TypeEntry row;
    row.kind = sir::TypeEntryKind::Queue;
    row.name = "$container" + std::to_string(id.index());
    row.value = element;
    design_.types.push_back(std::move(row));
    return sir::queueType(id);
}

void Lowerer::appendContainerSequence(Type buffer, const Val& receiver, const Val& source,
                                      slang::SourceLocation sl) {
    const auto count = emit(Op::ContainerSize, sir::u2(64), {source.op}, sl);
    appendContainerRange(buffer, receiver, source, constant(sir::u2(64), 0), count, sl);
}

void Lowerer::appendContainerRange(Type buffer, const Val& receiver, const Val& source,
                                   const Val& first, const Val& limit, slang::SourceLocation sl) {
    const auto element = design_.types[buffer.detail.index()].value;
    const auto from = design_.types[source.type.detail.index()].value;
    const auto index = newFrameVar(first.type);
    emitVoid(Op::Fstore, {Operand::frame(index), first.op}, sl);
    const auto condition = newBlock(), body = newBlock(), done = newBlock();
    jump(condition, sl);
    setBlock(condition);
    const auto current = emit(Op::Fload, first.type, {Operand::frame(index)}, sl);
    const auto test = emit(Op::Lt, sir::isFourState(first.type) ? sir::u4(1) : sir::u2(1),
                           {current.op, limit.op}, sl);
    terminate(Op::BrIf, {test.op, Operand::block(body), Operand::block(done)}, sl);
    setBlock(body);
    const auto value = emit(Op::ContainerRead, from, {source.op, current.op}, sl);
    emitVoid(Op::QueuePushBack,
             {Operand::typeRow(buffer.detail), receiver.op, coerce(value, element, sl).op}, sl);
    const auto next = emit(Op::Add, first.type, {current.op, constant(first.type, 1).op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(index), next.op}, sl);
    jump(condition, sl);
    setBlock(done);
}

// Queue slices clamp signed bounds without truncating wide unsigned
// indices. Unknown comparisons take no loop iterations (7.10.1).
Val Lowerer::lowerQueueSlice(const ast::Expression& expression, slang::SourceLocation sl) {
    const auto& slice = expression.as<ast::RangeSelectExpression>();
    if (slice.getSelectionKind() != ast::RangeSelectionKind::Simple)
        unsupported(sl, "indexed queue slice");
    const auto source = snapshotValue(lowerExpr(slice.value()), sl);
    auto first = containerIndex(slice.left(), source, source.type, sl);
    auto last = containerIndex(slice.right(), source, source.type, sl);
    const auto width = std::max({65U, first.type.width + (sir::isSigned(first.type) ? 0U : 1U),
                                 last.type.width + (sir::isSigned(last.type) ? 0U : 1U)});
    const auto bound = sir::s4(width);
    first = coerce(first, bound, sl);
    last = coerce(last, bound, sl);
    const auto zero = constant(bound, 0), one = constant(bound, 1);
    const auto count = coerce(emit(Op::ContainerSize, sir::u2(64), {source.op}, sl), bound, sl);
    const auto end = emit(Op::Sub, bound, {count.op, one.op}, sl);
    const auto negative = emit(Op::Lt, sir::u4(1), {first.op, zero.op}, sl);
    first = emit(Op::Select, bound, {negative.op, zero.op, first.op}, sl);
    const auto excess = emit(Op::Gt, sir::u4(1), {last.op, end.op}, sl);
    last = emit(Op::Select, bound, {excess.op, end.op, last.op}, sl);
    const auto limit = emit(Op::Add, bound, {last.op, one.op}, sl);
    const auto type = unboundedContainerType(typeOf(*slice.type, sl), sl);
    const auto buffer = sequenceBufferType(design_.types[type.detail.index()].value);
    const auto storage = newFrameVar(buffer);
    emitVoid(Op::Fstore, {Operand::frame(storage), constant(buffer, 0).op}, sl);
    const auto receiver =
        emit(Op::RefMake, sir::refType(buffer.detail), {Operand::frame(storage)}, sl);
    appendContainerRange(buffer, receiver, source, first, limit, sl);
    return coerce(emit(Op::Fload, buffer, {Operand::frame(storage)}, sl), type, sl);
}

Val Lowerer::lowerContainerValue(const ast::Expression& e) {
    const auto sl = e.sourceRange.start();
    const auto type = unboundedContainerType(typeOf(*e.type, sl), sl);
    const auto element = design_.types[type.detail.index()].value;
    if (e.kind == ast::ExpressionKind::NewArray) {
        const auto& array = e.as<ast::NewArrayExpression>();
        const auto size = coerce(lowerExpr(array.sizeExpr()), sir::s4(64), sl);
        const auto init =
            array.initExpr() ? snapshotValue(lowerExpr(*array.initExpr()), sl) : constant(type, 0);
        return emit(Op::ContainerNew, type, {size.op, init.op}, sl);
    }
    if (isPattern(e)) {
        std::vector<Operand> values;
        for (const auto* item : patternElements(e))
            values.push_back(snapshotValue(sir::isContainer(element)
                                               ? lowerExpr(*item)
                                               : coerce(lowerExpr(*item), element, sl),
                                           sl)
                                 .op);
        return emit(Op::ContainerPack, type, std::move(values), sl);
    }
    if (e.kind == ast::ExpressionKind::Conversion) {
        const auto& source = e.as<ast::ConversionExpression>().operand();
        if (isDynamicContainer(*source.type)) return coerce(lowerExpr(source), type, sl);
        if (const auto array = arrayTarget(source)) {
            std::vector<Place> elements;
            arrayPlaces(baseOf(*array), array->indices, elements);
            std::vector<Operand> values;
            values.reserve(elements.size());
            for (const auto& place : elements)
                values.push_back(snapshotValue(coerce(loadPlace(place, sl), element, sl), sl).op);
            return emit(Op::ContainerPack, type, std::move(values), sl);
        }
    }
    if (e.kind == ast::ExpressionKind::Concatenation) {
        const auto buffer = sequenceBufferType(element);
        const auto storage = newFrameVar(buffer);
        emitVoid(Op::Fstore, {Operand::frame(storage), constant(buffer, 0).op}, sl);
        const auto receiver =
            emit(Op::RefMake, sir::refType(buffer.detail), {Operand::frame(storage)}, sl);
        const auto& languageElement = *e.type->getArrayElementType();
        for (const auto* item : e.as<ast::ConcatenationExpression>().operands()) {
            // Slang has assignment-converted single elements, including
            // nested arrays. Only the remaining array operands are flattened.
            if (languageElement.isEquivalent(*item->type)) {
                const auto value = coerce(lowerExpr(*item), element, sl);
                emitVoid(Op::QueuePushBack,
                         {Operand::typeRow(buffer.detail), receiver.op, value.op}, sl);
            } else if (isDynamicContainer(*item->type)) {
                const auto source = snapshotValue(lowerExpr(*item), sl);
                appendContainerSequence(buffer, receiver, source, sl);
            } else if (const auto source = arrayTarget(*item)) {
                std::vector<Place> places;
                arrayPlaces(baseOf(*source), source->indices, places);
                for (const auto& place : places) {
                    const auto value = coerce(loadPlace(place, sl), element, sl);
                    emitVoid(Op::QueuePushBack,
                             {Operand::typeRow(buffer.detail), receiver.op, value.op}, sl);
                }
            } else {
                unsupported(sl, "unpacked concatenation operand of this form");
            }
        }
        return coerce(emit(Op::Fload, buffer, {Operand::frame(storage)}, sl), type, sl);
    }
    unsupported(sl, "dynamic container value of this expression kind");
}

std::optional<Val> Lowerer::lowerContainerMethod(const ast::CallExpression& call,
                                                 slang::SourceLocation sl) {
    if (const auto array = lowerArrayMethod(call, sl)) return array;
    if (!call.isSystemCall() || call.arguments().empty()) return std::nullopt;
    const auto args = call.arguments();
    const auto& self = *methodReceiver(args[0]);
    if (!isDynamicContainer(*self.type)) return std::nullopt;
    const std::string_view name = call.getSubroutineName();
    const auto& builtin = std::get<ast::CallExpression::SystemCallInfo>(call.subroutine);
    if (builtin.subroutine.get() !=
        builtin.scope->getCompilation().getSystemMethod(self.type->getCanonicalType().kind, name))
        return std::nullopt;
    const auto type = typeOf(*self.type, sl);
    const auto element = design_.types[type.detail.index()].value;
    if (name == "size" && args.size() == 1) {
        const auto value = lowerExpr(self);
        return coerce(emit(Op::ContainerSize, sir::u2(64), {value.op}, sl), typeOf(*call.type, sl),
                      sl);
    }
    if (name != "delete" && name != "insert" && name != "push_front" && name != "push_back" &&
        name != "pop_front" && name != "pop_back")
        unsupported(sl, "dynamic container method '" + std::string(name) + "'");
    const auto receiver = containerReference(self, false, sl);
    const auto argument = [&](const ast::Expression& e) {
        const auto value = lowerExpr(e);
        return snapshotValue(sir::isContainer(element) ? value : coerce(value, element, sl), sl);
    };
    std::vector<Operand> operands{Operand::typeRow(type.detail), receiver.op};
    Op op = Op::ContainerClear;
    if (name == "delete") {
        if (args.size() == 2) {
            op = Op::QueueErase;
            operands.push_back(containerIndex(*args[1], receiver, type, sl).op);
        }
    } else if (name == "insert") {
        op = Op::QueueInsert;
        operands.push_back(containerIndex(*args[1], receiver, type, sl).op);
        operands.push_back(argument(*args[2]).op);
    } else if (name == "push_front" || name == "push_back") {
        op = name == "push_front" ? Op::QueuePushFront : Op::QueuePushBack;
        operands.push_back(argument(*args[1]).op);
    } else {
        op = name == "pop_front" ? Op::QueuePopFront : Op::QueuePopBack;
        return emit(op, element, std::move(operands), sl);
    }
    emitVoid(op, std::move(operands), sl);
    return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
}
}  // namespace kirimasim::front
