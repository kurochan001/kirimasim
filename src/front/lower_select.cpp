// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of element, bit and part selects to SIR.

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

/// The type a bit position is computed in from a run-time index, and a
/// constant of it. 64 bits for an index of 32 bits or fewer; for a wider
/// index, wide enough that the arithmetic never wraps (a declared bound
/// takes 32 bits and an element at most 2^24), so that an index outside
/// the range stays outside however large it is (VALUE_SEMANTICS.md 3.11).
Type Lowerer::positionType(Type index) {
    return integral(sir::isFourState(index), true, index.width <= 32 ? 64 : index.width + 32);
}

Val Lowerer::positionConstant(Type t, std::int64_t k) {
    const std::size_t words = sir::integralWords(t.width);
    std::vector<std::uint64_t> a(words, k < 0 ? ~std::uint64_t{0} : 0);
    a[0] = static_cast<std::uint64_t>(k);
    return constantWords(t, std::move(a), std::vector<std::uint64_t>(words, 0));
}

Val Lowerer::bitPosition(const ast::Expression& selector, const slang::ConstantRange& r,
                         std::uint32_t elemWidth) {
    const slang::SourceLocation sl = selector.sourceRange.start();
    const std::int64_t count =
        std::int64_t{r.left >= r.right ? r.left - r.right : r.right - r.left} + 1;
    if (const std::optional<std::int64_t> idx = constInt(selector)) {
        observeRuntimeConstant(selector);
        // A constant outside the range selects nothing: position `count *
        // elemWidth` is past the end, so the read gives x.
        const std::optional<std::int64_t> pos = positionOf(*idx, r);
        const std::int64_t p = pos ? *pos * elemWidth : count * elemWidth;
        return constant(sir::s2(64), static_cast<std::uint64_t>(p));
    }
    Val i = lowerExpr(selector);
    if (!sir::isIntegral(i.type)) unsupported(sl, "index of this type");
    if (r.left >= r.right && r.right == 0 && elemWidth == 1) return i;
    const Type pt = positionType(i.type);
    i = coerce(i, pt, sl);
    const Val right = positionConstant(pt, r.right);
    Val pos = r.left >= r.right ? emit(Op::Sub, pt, {i.op, right.op}, sl)
                                : emit(Op::Sub, pt, {right.op, i.op}, sl);
    if (elemWidth != 1) pos = emit(Op::Mul, pt, {pos.op, positionConstant(pt, elemWidth).op}, sl);
    return pos;
}

std::optional<std::uint32_t> Lowerer::constElementOf(const sir::Slot& slot,
                                                     const std::vector<std::int64_t>& indices) {
    // The element number of constant declared indices, or nothing when one
    // is outside its range.
    std::uint32_t elem = 0;
    if (indices.size() != slot.dims.size()) return std::nullopt;
    for (std::size_t d = 0; d < indices.size(); ++d) {
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        const auto n = static_cast<std::uint32_t>(std::abs(l - r) + 1);
        const std::int64_t off = l <= r ? indices[d] - l : l - indices[d];
        if (off < 0 || off >= n) return std::nullopt;
        elem = elem * n + static_cast<std::uint32_t>(off);
    }
    return elem;
}

Lowerer::Place Lowerer::arrayElementPlace(const ast::Expression& base,
                                          const std::vector<const ast::Expression*>& indices,
                                          slang::SourceLocation sl) {
    // The indices of the element of an array of structures come first, then
    // those written after the member (SIR_SPEC.md 4.2); all evaluated in
    // order. Constant indices make the sensitivity that element alone.
    const std::optional<Storage> st = storageOf(base, sl);
    if (!st) unsupported(sl, "selecting from this array");
    Place p;
    p.kind = Place::Kind::Element;
    if (st->frame)
        p.frame = st->var;
    else
        p.slot = st->slot;
    p.indices = st->elem;
    if (st->frame) {
        // An automatic array, or what an array ref argument refers to
        // (SIR_SPEC.md 7.4, 8).
        const bool ref = frameVar(st->var).type.kind == sir::TypeKind::Ref;
        p.home = ref ? Place::Home::Ref : Place::Home::Frame;
        if (frameVar(st->var).dims.size() != st->elem.size() + indices.size())
            unsupported(sl, "a part of an unpacked array");
        for (const ast::Expression* ix : indices) {
            const Val iv = lowerExpr(*ix);
            if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
            p.indices.push_back(iv.op);
        }
        p.type = homeType(p);
        return p;
    }
    if (design().slot(st->slot).dims.size() != st->elem.size() + indices.size())
        unsupported(sl, "a part of an unpacked array");
    bool fixed = !st->dynamic;
    std::vector<std::int64_t> idx = st->constIdx;
    for (const ast::Expression* ix : indices) {
        const Val iv = lowerExpr(*ix);
        if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
        p.indices.push_back(iv.op);
        const std::optional<std::int64_t> c = constInt(*ix);
        if (c)
            idx.push_back(*c);
        else
            fixed = false;
    }
    // Looked up after the indices: the first call of a static function
    // among them adds its variables to the slot table.
    const sir::Slot& slot = design().slot(st->slot);
    if (fixed) p.elem = constElementOf(slot, idx);
    p.type = slot.type;
    return p;
}

Val Lowerer::lowerElementSelect(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& es = e.as<ast::ElementSelectExpression>();
    const ast::Type& vt = es.value().type->getCanonicalType();
    if (isDynamicContainer(vt)) {
        const auto firstRead = b_.reads.size();
        const auto value = lowerExpr(es.value());
        for (std::size_t i = firstRead; i < b_.reads.size(); ++i)
            if (sir::isContainer(design_.slot(b_.reads[i].slot).type))
                containerSelections_[{b_.isSub, b_.isSub ? b_.sub.index() : b_.proc.index()}]
                    .insert(b_.reads[i].slot);
        const auto index = containerIndex(es.selector(), value, value.type, sl);
        return emit(Op::ContainerRead, typeOf(*e.type, sl), {value.op, index.op}, sl);
    }
    if (vt.isUnpackedArray()) {
        if (structElement(*e.type) != nullptr)
            unsupported(sl, "unpacked structure used as a whole here");
        const std::optional<ArrayAccess> acc = arrayAccess(e);
        if (!acc) unsupported(sl, "selecting from this array");
        // An element of a parameter at constant indices is slang's constant
        // (SIR_SPEC.md 6).
        const ast::Symbol* base = namedSymbol(*acc->base);
        if (base != nullptr && base->kind == ast::SymbolKind::Parameter)
            if (const std::optional<slang::ConstantValue> cv = constantValue(e)) {
                for (const auto* index : acc->indices)
                    observeRuntimeConstant(*index);
                return constantFrom(*cv, typeOf(*e.type, sl), sl);
            }
        const Place place = arrayElementPlace(*acc->base, acc->indices, sl);
        const auto firstRead = b_.reads.size();
        const Val value = loadPlace(place, sl);
        narrowRead(e, *acc->base, firstRead);
        return coerce(value, typeOf(*e.type, sl), sl);
    }
    if (vt.isString()) {
        // s[i]: the byte, or 0 outside the string (table 6-9).
        const Val v = lowerExpr(es.value());
        const Val i = lowerExpr(es.selector());
        return coerce(emit(Op::StrGet, sir::u2(8), {v.op, i.op}, sl), typeOf(*e.type, sl), sl);
    }
    if (!vt.isIntegral()) unsupported(sl, "selecting from a value of this type");
    const auto firstRead = b_.reads.size();
    const Val v = lowerExpr(es.value());
    narrowRead(e, es.value(), firstRead);
    const std::uint32_t ew = e.type->getBitWidth();
    const Val pos = bitPosition(es.selector(), vt.getFixedRange(), ew);
    const bool four = sir::isFourState(v.type);
    const Val r = ew == 1 ? emit(Op::Bitsel, integral(four, false, 1), {v.op, pos.op}, sl)
                          : emit(Op::IdxPartsel, integral(four, false, ew), {v.op, pos.op}, sl);
    return coerce(r, typeOf(*e.type, sl), sl);
}

/// The position of the least significant bit of a range select, and its
/// width. Constant when the selection is.
Lowerer::Part Lowerer::partOf(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& rs = e.as<ast::RangeSelectExpression>();
    const ast::Type& vt = rs.value().type->getCanonicalType();
    // The bounds are indices of the dimension the select indexes, whose
    // elements may be several bits wide (7.4.1, 11.5.1): the element
    // number nearest the right bound times the element width gives the
    // lsb.
    const slang::ConstantRange r = vt.getFixedRange();
    const std::uint32_t width = e.type->getBitWidth();
    const std::int64_t ew = vt.getBitWidth() / r.width();
    const bool down = r.left >= r.right;
    if (rs.getSelectionKind() == ast::RangeSelectionKind::Simple) {
        const std::optional<std::int64_t> l = constInt(rs.left());
        const std::optional<std::int64_t> rr = constInt(rs.right());
        if (!l || !rr) unsupported(sl, "part-select with unknown bounds");
        // The element nearest the right bound of the declared range.
        const std::int64_t near = down ? std::min(*l, *rr) : std::max(*l, *rr);
        const std::int64_t lsb = (down ? near - r.right : r.right - near) * ew;
        return {std::nullopt, lsb, width};
    }
    const bool up = rs.getSelectionKind() == ast::RangeSelectionKind::IndexedUp;
    // Declared indices covered, n elements: [base, base + n - 1] going up,
    // [base - n + 1, base] going down. The one nearest the right bound
    // gives the lsb.
    const std::int64_t n = width / ew;
    if (const std::optional<std::int64_t> base = constInt(rs.left())) {
        observeRuntimeConstant(rs.left());
        const std::int64_t lo = up ? *base : *base - n + 1;
        const std::int64_t hi = up ? *base + n - 1 : *base;
        const std::int64_t lsb = (down ? lo - r.right : r.right - hi) * ew;
        return {std::nullopt, lsb, width};
    }
    Val b = lowerExpr(rs.left());
    if (!sir::isIntegral(b.type)) unsupported(sl, "part-select base of this type");
    const Type pt = positionType(b.type);
    b = coerce(b, pt, sl);
    // lsb = ((up ? base : base - n + 1) - right) * ew when the range goes
    // down, (right - (up ? base + n - 1 : base)) * ew when it goes up.
    std::int64_t k = 0;
    Val pos;
    if (down) {
        k = (up ? 0 : -(n - 1)) - r.right;
        pos = emit(Op::Add, pt, {b.op, positionConstant(pt, k).op}, sl);
    } else {
        k = r.right - (up ? n - 1 : 0);
        pos = emit(Op::Sub, pt, {positionConstant(pt, k).op, b.op}, sl);
    }
    if (ew != 1) pos = emit(Op::Mul, pt, {pos.op, positionConstant(pt, ew).op}, sl);
    return {pos, 0, width};
}

Val Lowerer::lowerRangeSelect(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& rs = e.as<ast::RangeSelectExpression>();
    const ast::Type& vt = rs.value().type->getCanonicalType();
    if (vt.kind == ast::SymbolKind::QueueType) return lowerQueueSlice(rs, sl);
    if (!vt.isIntegral()) unsupported(sl, "part-select of a value of this type");
    const auto firstRead = b_.reads.size();
    const Val v = lowerExpr(rs.value());
    narrowRead(e, rs.value(), firstRead);
    const Part p = partOf(e);
    const bool four = sir::isFourState(v.type);
    const Type rt = integral(four, false, p.width);
    Val r;
    if (p.pos) {
        r = emit(Op::IdxPartsel, rt, {v.op, p.pos->op}, sl);
    } else if (p.lsb >= 0 && p.lsb + p.width <= v.type.width) {
        r = emit(Op::Partsel, rt,
                 {v.op, constant(sir::u2(32), static_cast<std::uint64_t>(p.lsb)).op}, sl);
    } else {
        r = emit(Op::IdxPartsel, rt,
                 {v.op, constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb)).op}, sl);
    }
    return coerce(r, typeOf(*e.type, sl), sl);
}

}  // namespace kirimasim::front
