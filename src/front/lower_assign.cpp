// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of aggregate values and of assignments to SIR.

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lower_expr_helpers.h"
#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/EvalContext.h"
#include "slang/ast/ValuePath.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/LiteralExpressions.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/numeric/SVInt.h"

namespace kirimasim::front {

bool Lowerer::isAggregate(const ast::Type& t) {
    return unpackedStruct(t) != nullptr ||
           t.getCanonicalType().kind == ast::SymbolKind::FixedSizeUnpackedArrayType;
}

void Lowerer::aggregateValues(const ast::Expression& e, const ast::Type& t, std::vector<Val>& out,
                              slang::SourceLocation sl) {
    // The values of an unpacked array or structure, single values in order:
    // array elements from the left bound, structure members in declaration
    // order, nested ones in turn (VALUE_SEMANTICS.md 3.14).
    const ast::Expression* r = &e;
    while (r->kind == ast::ExpressionKind::Conversion &&
           isAggregate(*r->as<ast::ConversionExpression>().operand().type))
        r = &r->as<ast::ConversionExpression>().operand();
    const ast::Type& ct = t.getCanonicalType();
    const ast::UnpackedStructType* st = unpackedStruct(ct);
    if (isPattern(*r)) {
        const std::vector<const ast::Expression*> elems = patternElements(*r);
        if (st != nullptr) {
            if (elems.size() != st->fields.size()) unsupported(sl, "pattern of this size");
            for (std::size_t i = 0; i < elems.size(); ++i)
                aggregateValues(*elems[i], st->fields[i]->getType(), out, sl);
            return;
        }
        if (ct.kind != ast::SymbolKind::FixedSizeUnpackedArrayType)
            unsupported(sl, "comparison of this array");
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        if (elems.size() != at.range.width()) unsupported(sl, "pattern of this size");
        for (const ast::Expression* el : elems)
            aggregateValues(*el, at.elementType, out, sl);
        return;
    }
    if (!isAggregate(ct)) {
        out.push_back(coerce(lowerExpr(*r), typeOf(ct, sl), sl));
        return;
    }
    if (structElement(ct) != nullptr) {
        const std::optional<Storage> src = storageOf(*r, sl, true);
        if (!src || !isAggregate(*r->type)) unsupported(sl, "comparison of this structure value");
        storageValues(*src, ct, out, sl);
        return;
    }
    const std::optional<ArrayTarget> a = arrayTarget(*r);
    if (!a) {
        // A slice (VALUE_SEMANTICS.md 3.13).
        const std::optional<Storage> src = storageOf(*r, sl, true);
        if (!src || !src->slice) unsupported(sl, "comparison of this array value");
        storageValues(*src, ct, out, sl);
        return;
    }
    arrayValues(baseOf(*a), a->indices, out, sl);
}

void Lowerer::arrayValues(ArrayBase s, std::vector<std::int64_t> indices, std::vector<Val>& out,
                          slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim>& dims = dimsOf(s);
    if (indices.size() == dims.size()) {
        const Place p = elementPlace(s, indices);
        out.push_back(loadPlace(p, sl));
        return;
    }
    const sir::ArrayDim d = dims[indices.size()];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    for (std::int64_t i = d.left;; i += step) {
        indices.push_back(i);
        arrayValues(s, indices, out, sl);
        indices.pop_back();
        if (i == d.right) break;
    }
}

Val Lowerer::compareAggregates(const ast::BinaryExpression& b, slang::SourceLocation sl) {
    // == is 0 if a pair is 0, else x if a pair is x, else 1: the AND of
    // the pairs' ==; === the AND of their ===; != and !== the negation
    // (VALUE_SEMANTICS.md 3.14). Both sides are evaluated, left first.
    using BO = ast::BinaryOperator;
    const bool caseEq = b.op == BO::CaseEquality || b.op == BO::CaseInequality;
    std::vector<Val> l;
    std::vector<Val> r;
    aggregateValues(b.left(), *b.left().type, l, sl);
    aggregateValues(b.right(), *b.left().type, r, sl);
    if (l.size() != r.size() || l.empty()) unsupported(sl, "comparison of these aggregates");
    const auto pair = [&](std::size_t i) {
        // Reals and strings have no x or z: their === is ==.
        const bool bits = sir::isIntegral(l[i].type) && sir::isIntegral(r[i].type);
        return compareValues(caseEq && bits ? Op::Ceq : Op::Eq, l[i], r[i], sl);
    };
    Val all = pair(0);
    for (std::size_t i = 1; i < l.size(); ++i)
        all = bitOp(Op::And, all, pair(i), sl);
    if (b.op == BO::Inequality || b.op == BO::CaseInequality)
        all = emit(Op::Not, all.type, {all.op}, sl);
    return all;
}

// --- Assignments -------------------------------------------------------------

Val Lowerer::lowerStreamAssignment(const ast::AssignmentExpression& a,
                                   const ast::StreamingConcatenationExpression& stream,
                                   const ast::TimingControl* event) {
    const auto sl = a.sourceRange.start();
    Val st = lowerStream(stream, sl);
    const auto count = event ? std::optional{eventNbaCount(*event, sl)} : std::nullopt;
    EventNbaCapture capture;
    std::vector<Place> places;
    bitstreamPlaces(a.left(), places, sl);
    const Place all = concatPlace(std::move(places), st.type, sl);
    if (st.type.width > all.width) unsupported(sl, "stream wider than its target");
    if (st.type.width < all.width)
        st = emit(Op::Concat, integral(sir::isFourState(st.type), false, all.width),
                  {st.op, constant(sir::u2(all.width - st.type.width), 0).op}, sl);
    storePlace(all, st, a.isNonBlocking(), sl, std::nullopt, event ? &capture : nullptr);
    if (event != nullptr && count.has_value()) finishEventNba(capture, *event, *count, sl);
    return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
}

Val Lowerer::lowerAggregateAssignment(const ast::AssignmentExpression& a,
                                      const ast::Expression* delay,
                                      const ast::TimingControl* event) {
    const slang::SourceLocation sl = a.sourceRange.start();
    // A fixed layout emits no runtime target evaluation and can be reused
    // as pending stores. Other targets use a private layout until every
    // RHS value is captured (VALUE_SEMANTICS 3.1, 3.13; SIR_SPEC.md 7.3).
    const auto fixedTarget = [&](const ast::Expression& e, auto&& self) -> bool {
        checkStack(e.sourceRange.start());
        if (e.kind == ast::ExpressionKind::NamedValue ||
            e.kind == ast::ExpressionKind::HierarchicalValue)
            return true;
        if (isUnpackedMember(e)) return self(e.as<ast::MemberAccessExpression>().value(), self);
        if (e.kind == ast::ExpressionKind::ElementSelect) {
            const auto& select = e.as<ast::ElementSelectExpression>();
            return select.value().type->getCanonicalType().kind ==
                       ast::SymbolKind::FixedSizeUnpackedArrayType &&
                   !hasConditionChecks(select.selector()) &&
                   constInt(select.selector()).has_value() && self(select.value(), self);
        }
        return false;
    };
    std::optional<Storage> dst =
        fixedTarget(a.left(), fixedTarget) ? storageOf(a.left(), sl) : std::nullopt;
    Stores values;
    assignAggregate(dst ? *dst : tempStorage(*a.left().type, sl), *a.left().type, a.right(), values,
                    sl);
    const std::optional<Val> ticks =
        delay != nullptr ? std::optional{delayValue(*delay, sl)} : std::nullopt;
    const auto count = event ? std::optional{eventNbaCount(*event, sl)} : std::nullopt;
    EventNbaCapture capture;
    if (dst) {
        flushStores(values, a.isNonBlocking(), sl, ticks, event ? &capture : nullptr);
        if (event != nullptr && count.has_value()) finishEventNba(capture, *event, *count, sl);
        return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    dst = storageOf(a.left(), sl, true);
    if (!dst) unsupported(sl, "aggregate assignment target");
    std::vector<Place> targets;
    storagePlaces(*dst, *a.left().type, targets, sl);
    if (targets.size() != values.size())
        throw std::logic_error("aggregate capture and target shapes differ");
    for (std::size_t i = 0; i < targets.size(); ++i)
        storePlace(targets[i], values[i].second, a.isNonBlocking(), sl, ticks,
                   event ? &capture : nullptr);
    if (event != nullptr && count.has_value()) finishEventNba(capture, *event, *count, sl);
    return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
}

Val Lowerer::lowerDelayedAssignment(const ast::AssignmentExpression& a) {
    // a = #d b and a = @(e) b compute b first and assign it when the delay
    // or event comes, the target's indices then; a <= #d b decides both
    // now and updates in the NBA region d later (SCHEDULER_SPEC.md 8, 13;
    // 9.4.5). The value lives in a frame variable across the wait.
    const slang::SourceLocation sl = a.sourceRange.start();
    const ast::TimingControl& t = *a.timingControl;
    if (a.isNonBlocking() && t.kind != ast::TimingControlKind::Delay) return lowerEventNba(a);
    if (a.isNonBlocking() && t.kind == ast::TimingControlKind::Delay &&
        a.left().kind == ast::ExpressionKind::Streaming) {
        lowerUnpack(a.left().as<ast::StreamingConcatenationExpression>(), a.right(), true, sl,
                    &t.as<ast::DelayControl>().expr);
        return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    const bool aggregate = isAggregate(*a.left().type);
    if (aggregate && a.isNonBlocking() && !a.isCompound() &&
        t.kind == ast::TimingControlKind::Delay)
        return lowerAggregateAssignment(a, &t.as<ast::DelayControl>().expr);
    if (aggregate && !a.isNonBlocking() && !a.isCompound()) {
        // Only private frame storage survives this wait. Capture all RHS
        // leaves now and choose the actual target after the control (8).
        const Storage saved = tempStorage(*a.left().type, sl);
        Stores captured;
        assignAggregate(saved, *a.left().type, a.right(), captured, sl);
        flushStores(captured, false, sl);
        lowerTiming(t);
        const std::optional<Storage> dst = storageOf(a.left(), sl, true);
        if (!dst) unsupported(sl, "aggregate assignment target");
        Stores resumed;
        copyAggregate(*dst, saved, *a.left().type, resumed, sl);
        flushStores(resumed, false, sl);
        return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    if (a.isCompound() || unpackedStruct(*a.left().type) != nullptr ||
        isAggregate(*a.left().type) || a.left().kind == ast::ExpressionKind::Streaming)
        unsupported(sl, "intra-assignment timing control on this assignment");
    const Type lt = typeOf(*a.left().type, sl);
    Val v = sir::isContainer(lt) ? snapshotValue(lowerExpr(a.right()), sl)
                                 : coerce(lowerExpr(a.right()), lt, sl);
    if (a.isNonBlocking()) {
        if (t.kind != ast::TimingControlKind::Delay)
            unsupported(sl, "nonblocking assignment with an event control");
        const Val ticks = delayValue(t.as<ast::DelayControl>().expr, sl);
        // Capture every target index now; storePlace handles the same
        // places as an undelayed write (SIR_SPEC.md 5.3, 7.3).
        const Place p = placeOf(a.left(), sl);
        storePlace(p, v, true, sl, ticks);
        return v;
    }
    const sir::FrameVarId f = newFrameVar(v.type);
    emitVoid(Op::Fstore, {Operand::frame(f), v.op}, sl);
    lowerTiming(t);
    const Val later = emit(Op::Fload, v.type, {Operand::frame(f)}, sl);
    lowerStore(a.left(), later, false, sl);
    return later;
}

Val Lowerer::lowerAssignment(const ast::AssignmentExpression& a) {
    const slang::SourceLocation sl = a.sourceRange.start();
    if (isAggregate(*a.left().type) && hasEventLeaves(*a.left().type))
        unsupported(sl, "event used as a value");
    if (a.timingControl != nullptr) return lowerDelayedAssignment(a);
    if (isDynamicContainer(*a.left().type)) {
        if (a.isCompound()) unsupported(sl, "compound assignment to a dynamic container value");
        const auto value = snapshotValue(lowerExpr(a.right()), sl);
        const auto target = placeOf(a.left(), sl);
        storePlace(target, value, a.isNonBlocking(), sl);
        return a.isNonBlocking() ? value : loadPlace(target, sl);
    }
    if (a.left().kind == ast::ExpressionKind::Streaming) {
        lowerUnpack(a.left().as<ast::StreamingConcatenationExpression>(), a.right(),
                    a.isNonBlocking(), sl);
        return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    if (a.isCompound()) {
        // a op= b is a = a op (b) with the indices of a evaluated once
        // (11.4.1): the target's indices, its value, then b. slang gives
        // the right side as `<lvalue> op b`, the lvalue as an
        // LValueReference.
        const Place p = placeOf(a.left(), sl);
        b_.lvalueRefs.push_back(loadPlace(p, sl));
        const Val v = coerce(lowerExpr(a.right()), typeOf(*a.left().type, sl), sl);
        b_.lvalueRefs.pop_back();
        storePlace(p, v, a.isNonBlocking(), sl);
        return v;
    }
    if (isAggregate(*a.left().type)) {
        // A stream into an unpacked array or structure: left-aligned, zeros
        // on the right, then its bits from the left into the single places
        // in order (11.4.14).
        const ast::Expression* r = &a.right();
        if (r->kind == ast::ExpressionKind::Conversion &&
            r->as<ast::ConversionExpression>().conversionKind ==
                ast::ConversionKind::StreamingConcat)
            r = &r->as<ast::ConversionExpression>().operand();
        if (r->kind == ast::ExpressionKind::Streaming) {
            return lowerStreamAssignment(a, r->as<ast::StreamingConcatenationExpression>());
        }
        return lowerAggregateAssignment(a, nullptr);
    }
    const Val v = coerce(lowerExpr(a.right()), typeOf(*a.left().type, sl), sl);
    lowerStore(a.left(), v, a.isNonBlocking(), sl);
    return v;
}

std::optional<Lowerer::ArrayTarget> Lowerer::arrayTarget(const ast::Expression& e) {
    // A whole unpacked array in a slot, or a sub-array of one selected by
    // constant indices.
    if (e.kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = e.as<ast::ElementSelectExpression>();
        if (!es.value().type->getCanonicalType().isUnpackedArray()) return std::nullopt;
        const std::optional<std::int64_t> i = constInt(es.selector());
        // Discover a runtime prefix before lowering its base: the shared
        // storage path must evaluate that base only once.
        if (!i) return std::nullopt;
        std::optional<ArrayTarget> t = arrayTarget(es.value());
        if (!t) return std::nullopt;
        observeRuntimeConstant(es.selector());
        t->indices.push_back(*i);
        return t;
    }
    // An array member of an element of an array of structures is the
    // sub-array of its slot at the element's indices.
    if (!e.type->getCanonicalType().isUnpackedArray() || structElement(*e.type) != nullptr)
        return std::nullopt;
    const std::optional<Storage> st = storageOf(e, e.sourceRange.start());
    if (!st || st->dynamic || !isArrayLeaf(*st)) return std::nullopt;
    if (!st->frame) return ArrayTarget{st->slot, st->constIdx};
    // An automatic array, or what an array ref argument refers to.
    const bool ref = frameVar(st->var).type.kind == sir::TypeKind::Ref;
    return ArrayTarget{{}, st->constIdx, ref ? Place::Home::Ref : Place::Home::Frame, st->var};
}

const std::vector<sir::ArrayDim>& Lowerer::dimsOf(ArrayBase b) {
    return b.home == Place::Home::Slot ? design().slot(b.slot).dims : frameVar(b.frame).dims;
}

Type Lowerer::elemTypeOf(ArrayBase b) {
    switch (b.home) {
        case Place::Home::Slot: return design().slot(b.slot).type;
        case Place::Home::Frame: return frameVar(b.frame).type;
        case Place::Home::Ref: return frameVar(b.frame).target;
        case Place::Home::Heap:
        case Place::Home::Object: return sir::voidType();
    }
    return sir::voidType();
}

Type Lowerer::homeType(const Place& p) {
    if (p.home == Place::Home::Object)
        return design_.classes[p.container.object.index()].fields[p.leaf].type;
    if (p.home == Place::Home::Heap) return p.heapElement;
    return elemTypeOf(p.home == Place::Home::Slot ? ArrayBase{p.slot} : ArrayBase{p.home, p.frame});
}

void Lowerer::flushStores(const Stores& stores, bool nonblocking, slang::SourceLocation sl,
                          const std::optional<Val>& delay, EventNbaCapture* event) {
    // The writes of an aggregate assignment, after every value of its right
    // side (VALUE_SEMANTICS.md 3.13).
    for (const auto& [place, value] : stores)
        storePlace(place, value, nonblocking, sl, delay, event);
}

Lowerer::Place Lowerer::elementPlace(ArrayBase s, const std::vector<std::int64_t>& indices) {
    Place p;
    p.kind = Place::Kind::Element;
    p.home = s.home;
    p.slot = s.slot;
    p.frame = s.frame;
    for (const std::int64_t i : indices)
        p.indices.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(i)).op);
    if (s.home == Place::Home::Slot) p.elem = elementNumber(design().slot(s.slot), indices);
    p.type = elemTypeOf(s);
    return p;
}

void Lowerer::assignArray(ArrayBase s, std::vector<std::int64_t> indices,
                          const ast::Expression& rhs, Stores& out, slang::SourceLocation sl,
                          bool initializeEvents) {
    const ast::Expression* r = &rhs;
    while (r->kind == ast::ExpressionKind::Conversion &&
           r->as<ast::ConversionExpression>().operand().type->getCanonicalType().isUnpackedArray())
        r = &r->as<ast::ConversionExpression>().operand();
    const std::vector<sir::ArrayDim> dims = dimsOf(s);
    const std::size_t level = indices.size();
    if (level == dims.size()) {
        // One element: its value, for its constant indices (SIR_SPEC.md 5.3).
        const Type type = elemTypeOf(s);
        const Val v = initializeEvents && type.kind == sir::TypeKind::Event
                          ? lowerEventCopy(*r)
                          : snapshotValue(coerce(lowerExpr(*r), type, sl), sl);
        out.emplace_back(elementPlace(s, indices), v);
        return;
    }
    const sir::ArrayDim d = dims[level];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    const auto count =
        static_cast<std::size_t>((d.left <= d.right ? d.right - d.left : d.left - d.right) + 1);
    if (r->kind == ast::ExpressionKind::SimpleAssignmentPattern ||
        r->kind == ast::ExpressionKind::StructuredAssignmentPattern ||
        r->kind == ast::ExpressionKind::ReplicatedAssignmentPattern) {
        // The elements of the pattern, from the left bound (10.9.1).
        const std::vector<const ast::Expression*> elems = patternElements(*r);
        if (elems.size() != count) unsupported(sl, "an assignment pattern of this size");
        for (std::size_t k = 0; k < count; ++k) {
            indices.push_back(d.left + static_cast<std::int64_t>(k) * step);
            assignArray(s, indices, *elems[k], out, sl, initializeEvents);
            indices.pop_back();
        }
        return;
    }
    if (const std::optional<ArrayTarget> src = arrayTarget(*r)) {
        // Another array of the same shape: element by element, from the
        // left bounds (7.6).
        const std::vector<sir::ArrayDim>& sdims = dimsOf(baseOf(*src));
        if (sdims.size() - src->indices.size() != dims.size() - level)
            unsupported(sl, "assignment of an unpacked array of another shape");
        copyArray(s, std::move(indices), baseOf(*src), src->indices, out, sl);
        return;
    }
    if (const std::optional<Storage> src = storageOf(*r, sl, true); src && src->slice) {
        // A slice: element by element from its left end (VALUE_SEMANTICS.md
        // 3.13).
        Storage dst;
        dst.frame = s.home != Place::Home::Slot;
        dst.slot = s.slot;
        dst.var = s.frame;
        for (const std::int64_t i : indices)
            dst = withIndex(std::move(dst), i);
        copyArrayStorage(std::move(dst), *src, out, sl);
        return;
    }
    unsupported(sl, "assignment of this unpacked array value");
}

void Lowerer::copyArray(ArrayBase to, std::vector<std::int64_t> toIndices, ArrayBase from,
                        std::vector<std::int64_t> fromIndices, Stores& out,
                        slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim> td = dimsOf(to);
    const std::vector<sir::ArrayDim> fd = dimsOf(from);
    if (toIndices.size() == td.size()) {
        const Val v = snapshotValue(
            coerce(loadPlace(elementPlace(from, fromIndices), sl), elemTypeOf(to), sl), sl);
        out.emplace_back(elementPlace(to, toIndices), v);
        return;
    }
    const sir::ArrayDim t = td[toIndices.size()];
    const sir::ArrayDim f = fd[fromIndices.size()];
    const auto count = [](sir::ArrayDim a) {
        return (a.left <= a.right ? a.right - a.left : a.left - a.right) + 1;
    };
    if (count(t) != count(f)) unsupported(sl, "assignment of an unpacked array of another shape");
    for (std::int64_t k = 0; k < count(t); ++k) {
        toIndices.push_back(t.left + (t.left <= t.right ? k : -k));
        fromIndices.push_back(f.left + (f.left <= f.right ? k : -k));
        copyArray(to, toIndices, from, fromIndices, out, sl);
        toIndices.pop_back();
        fromIndices.pop_back();
    }
}

std::uint32_t Lowerer::elementNumber(const sir::Slot& slot,
                                     const std::vector<std::int64_t>& indices) const {
    // Elements are numbered from the left bound, the rightmost dimension
    // fastest (SIR_SPEC.md 4.2).
    std::uint64_t n = 0;
    for (std::size_t d = 0; d < slot.dims.size(); ++d) {
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        const auto size = static_cast<std::uint64_t>((l <= r ? r - l : l - r) + 1);
        const auto off = static_cast<std::uint64_t>(l <= r ? indices[d] - l : l - indices[d]);
        n = n * size + off;
    }
    return static_cast<std::uint32_t>(n);
}

void Lowerer::lowerStore(const ast::Expression& lhs, const Val& v, bool nonblocking,
                         slang::SourceLocation sl) {
    // The value is computed before the indices of the target (SIR_SPEC.md
    // 7.3); the callers lower it first.
    storePlace(placeOf(lhs, sl), v, nonblocking, sl);
}

}  // namespace kirimasim::front
