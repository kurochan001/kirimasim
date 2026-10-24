// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of unpacked structures and arrays of them (SIR_SPEC.md 4.2).
//
// A variable of an unpacked structure type has one slot, or one frame
// variable when it is automatic, for each innermost member (a leaf), in
// declaration order. An array of structures has the same slots with the
// array's dimensions in front: a[i].x is element i of the slot of x. A
// member is then read and written like a variable of its own type, inside
// the element its indices choose. A whole structure or array of them is
// assigned member by member and element by element: from an assignment
// pattern, whose elements slang gives in order, or from another one of
// the same type, leaf by leaf; every value first, then every write
// (VALUE_SEMANTICS.md 3.13).

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/SubroutineSymbols.h"

namespace kirimasim::front {

namespace {

/// The expression without the conversions slang puts around an aggregate.
const ast::Expression& unwrapAggregate(const ast::Expression& e) {
    const ast::Expression* r = &e;
    while (r->kind == ast::ExpressionKind::Conversion &&
           Lowerer::isAggregate(*r->as<ast::ConversionExpression>().operand().type))
        r = &r->as<ast::ConversionExpression>().operand();
    return *r;
}

}  // namespace

bool hasMemberDefaults(const ast::Type& t) {
    const ast::UnpackedStructType* st = structElement(t);
    if (st == nullptr) return false;
    for (const ast::FieldSymbol* f : st->fields)
        if (f->getInitializer() != nullptr || hasMemberDefaults(f->getType())) return true;
    return false;
}

bool Lowerer::isUnpackedMember(const ast::Expression& e) {
    return e.kind == ast::ExpressionKind::MemberAccess &&
           unpackedStruct(*e.as<ast::MemberAccessExpression>().value().type) != nullptr;
}

Lowerer::Storage Lowerer::leafAt(Storage s, std::uint32_t off) {
    // The leaves of a structure are consecutive slots, or consecutive frame
    // variables (SIR_SPEC.md 4.2): the one place a leaf's number is counted.
    if (s.frame)
        s.var = sir::FrameVarId{s.var.index() + off};
    else
        s.slot = sir::SlotId{s.slot.index() + off};
    return s;
}

Lowerer::Storage Lowerer::withIndex(Storage s, std::int64_t index) {
    s.elem.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(index)).op);
    s.constIdx.push_back(index);
    return s;
}

bool Lowerer::isArrayLeaf(const Storage& s) {
    if (s.frame) return !frameVar(s.var).dims.empty();
    return design_.slot(s.slot).dims.size() > s.elem.size();
}

Lowerer::ArrayBase Lowerer::storageBase(const Storage& s) {
    if (!s.frame) return s.slot;
    const bool ref = frameVar(s.var).type.kind == sir::TypeKind::Ref;
    return {ref ? Place::Home::Ref : Place::Home::Frame, s.var};
}

std::vector<std::int64_t> Lowerer::fixedIndices(const Storage& s, slang::SourceLocation sl) {
    if (s.dynamic) unsupported(sl, "an array member of an element chosen at run time");
    return s.constIdx;
}

sir::FrameVarId Lowerer::structFrameVars(const ast::VariableSymbol& v, slang::SourceLocation sl,
                                         bool returnValue) {
    if (auto it = b_.structFrames.find(&v); it != b_.structFrames.end()) return it->second;
    std::vector<Leaf> leaves;
    structLeaves(v.getType(), "", leaves);
    const auto first = static_cast<sir::FrameVarId>(frameVars().size());
    for (const Leaf& l : leaves) {
        if (!returnValue &&
            ((l.type->getCanonicalType().isUnpackedArray() && !isDynamicContainer(*l.type)) ||
             !l.dims.empty()))
            unsupported(sl, "automatic unpacked array in a structure or of structures");
        std::vector<sir::ArrayDim> dims = l.dims;
        const ast::Type& element = arrayElement(*l.type, dims);
        const sir::Type t = typeOf(element, sl);
        if (t.kind == sir::TypeKind::Event) unsupported(sl, "automatic event variable");
        frameVars().push_back({std::string(v.name) + l.path, t, sir::kNone, std::move(dims), {}});
    }
    b_.structFrames.emplace(&v, first);
    return first;
}

std::optional<Lowerer::Storage> Lowerer::storageOf(const ast::Expression& e,
                                                   slang::SourceLocation sl) {
    if (e.kind == ast::ExpressionKind::Call && !e.as<ast::CallExpression>().isSystemCall()) {
        // A function's aggregate value: the frame variables that receive it,
        // the call made now (SIR_SPEC.md 8).
        const auto& c = e.as<ast::CallExpression>();
        const ast::Type& rt = std::get<0>(c.subroutine)->getReturnType();
        if (!isAggregate(rt)) return std::nullopt;
        return callAggregate(c);
    }
    if (isUnpackedMember(e)) {
        // A member: the structure's leaves, from the member's first one.
        const auto& m = e.as<ast::MemberAccessExpression>();
        if (m.member.kind != ast::SymbolKind::Field) return std::nullopt;
        std::optional<Storage> base = storageOf(m.value(), sl);
        if (!base) return std::nullopt;
        return leafAt(std::move(*base), leafOffset(*unpackedStruct(*m.value().type),
                                                   m.member.as<ast::FieldSymbol>()));
    }
    if (e.kind == ast::ExpressionKind::ElementSelect) {
        // An element of an array of structures: the array's leaves with one
        // more index, evaluated now.
        const auto& es = e.as<ast::ElementSelectExpression>();
        const ast::Type& vt = es.value().type->getCanonicalType();
        if (vt.kind != ast::SymbolKind::FixedSizeUnpackedArrayType) return std::nullopt;
        std::optional<Storage> base = storageOf(es.value(), sl);
        if (!base) return std::nullopt;
        if (const std::optional<std::int64_t> c = constInt(es.selector()))
            return withIndex(std::move(*base), *c);
        const Val iv = lowerExpr(es.selector());
        if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
        base->elem.push_back(iv.op);
        base->dynamic = true;
        return base;
    }
    const ast::Symbol* sym = namedSymbol(e);
    if (sym != nullptr && sym->kind == ast::SymbolKind::Parameter &&
        sym->as<ast::ParameterSymbol>().getType().isUnpackedArray()) {
        Storage st;
        st.slot = paramSlot(sym->as<ast::ParameterSymbol>(), sl);
        return st;
    }
    if (sym == nullptr || (!isVariable(*sym) && sym->kind != ast::SymbolKind::Net))
        return std::nullopt;
    const auto& v = sym->as<ast::ValueSymbol>();
    Storage st;
    if (structElement(v.getType()) != nullptr) {
        if (auto it = b_.structFrames.find(sym); it != b_.structFrames.end()) {
            st.frame = true;
            st.var = it->second;
            return st;
        }
        if (!inSlot(*sym)) unsupported(sl, "automatic variable '" + std::string(v.name) + "' here");
        st.slot = structSlots(v, sl);
        return st;
    }
    if (const std::optional<sir::FrameVarId> f = findFrameVar(*sym)) {
        st.frame = true;
        st.var = *f;
        return st;
    }
    if (!inSlot(*sym)) return std::nullopt;
    st.slot = slotOf(v, sl);
    return st;
}

Lowerer::Place Lowerer::storagePlace(Storage st) {
    // One single leaf: a frame variable, a whole slot, or the element of an
    // array of structures.
    Place p;
    if (st.frame) {
        // An automatic variable, or what a ref argument refers to (SIR_SPEC.md 8).
        const sir::FrameVar& fv = frameVar(st.var);
        const bool ref = fv.type.kind == sir::TypeKind::Ref;
        p.kind = ref ? Place::Kind::Ref : Place::Kind::Frame;
        p.frame = st.var;
        p.type = ref ? fv.target : fv.type;
        if (!fv.dims.empty()) {
            p.kind = Place::Kind::Element;
            p.home = ref ? Place::Home::Ref : Place::Home::Frame;
            p.indices = std::move(st.elem);
        }
        return p;
    }
    const sir::Slot& slot = design().slot(st.slot);
    p.slot = st.slot;
    p.type = slot.type;
    if (st.elem.empty()) {
        p.kind = Place::Kind::Slot;
        return p;
    }
    p.kind = Place::Kind::Element;
    p.indices = std::move(st.elem);
    if (!st.dynamic) p.elem = constElementOf(slot, st.constIdx);
    return p;
}

void Lowerer::assignStruct(Storage dst, const ast::Type& t, const ast::Expression& rhs, Stores& out,
                           slang::SourceLocation sl) {
    const ast::UnpackedStructType& st = *unpackedStruct(t);
    const ast::Expression& r = unwrapAggregate(rhs);
    if (isPattern(r)) {
        // One element per member, in declaration order (10.9.2).
        const std::vector<const ast::Expression*> elems = patternElements(r);
        if (elems.size() != st.fields.size()) unsupported(sl, "assignment pattern of this size");
        std::uint32_t off = 0;
        for (std::size_t i = 0; i < elems.size(); ++i) {
            const ast::Type& ft = st.fields[i]->getType();
            assignAggregate(leafAt(dst, off), ft, *elems[i], out, sl);
            off += leafCount(ft);
        }
        return;
    }
    // Another structure of the same type (6.22.2): leaf by leaf.
    const std::optional<Storage> src =
        unpackedStruct(*r.type) != nullptr ? storageOf(r, sl) : std::nullopt;
    if (!src) unsupported(sl, "assignment of this unpacked structure value");
    copyAggregate(std::move(dst), *src, t, out, sl);
}

void Lowerer::assignAggregate(Storage dst, const ast::Type& t, const ast::Expression& rhs,
                              Stores& out, slang::SourceLocation sl) {
    const ast::Expression& expression = unwrapAggregate(rhs);
    if (t.getCanonicalType().kind == ast::SymbolKind::FixedSizeUnpackedArrayType &&
        expression.kind == ast::ExpressionKind::ConditionalOp) {
        const auto& conditional = expression.as<ast::ConditionalExpression>();
        if (conditional.conditions.size() != 1 || conditional.conditions[0].pattern != nullptr)
            unsupported(sl, "aggregate conditional with a pattern");
        Val condition = lowerExpr(*conditional.conditions[0].expr);
        if (condition.type.width != 1 || sir::isSigned(condition.type))
            condition =
                emit(sir::Op::Truth, sir::isFourState(condition.type) ? sir::u4(1) : sir::u2(1),
                     {condition.op}, sl);
        const auto first = newBlock(), notOne = newBlock(), second = newBlock(), both = newBlock(),
                   join = newBlock();
        const auto one =
            emit(sir::Op::Ceq, sir::u2(1), {condition.op, constant(condition.type, 1).op}, sl);
        terminate(sir::Op::BrIf, {one.op, sir::Operand::block(first), sir::Operand::block(notOne)},
                  sl);
        setBlock(notOne);
        const auto zero =
            emit(sir::Op::Ceq, sir::u2(1), {condition.op, constant(condition.type, 0).op}, sl);
        terminate(sir::Op::BrIf, {zero.op, sir::Operand::block(second), sir::Operand::block(both)},
                  sl);
        setBlock(first);
        Stores left;
        assignAggregate(dst, t, conditional.left(), left, sl);
        const auto branch = [&](const Stores& values) {
            std::vector<sir::Operand> args{
                sir::Operand::block(join, static_cast<std::uint32_t>(values.size()))};
            for (const auto& [place, value] : values)
                args.push_back(value.op);
            terminate(sir::Op::Br, std::move(args), sl);
        };
        for (const auto& [place, value] : left)
            out.emplace_back(place, blockParam(join, value.type));
        branch(left);
        setBlock(second);
        Stores right;
        assignAggregate(dst, t, conditional.right(), right, sl);
        branch(right);
        setBlock(both);
        left.clear();
        right.clear();
        assignAggregate(dst, t, conditional.left(), left, sl);
        assignAggregate(dst, t, conditional.right(), right, sl);
        for (std::size_t k = 0; k < left.size(); ++k)
            left[k].second =
                snapshotValue(emit(sir::Op::Select, left[k].second.type,
                                   {condition.op, left[k].second.op, right[k].second.op}, sl),
                              sl);
        branch(left);
        setBlock(join);
        return;
    }
    if (unpackedStruct(t) != nullptr) {
        assignStruct(std::move(dst), t, rhs, out, sl);
        return;
    }
    const ast::Type& ct = t.getCanonicalType();
    if (structElement(ct) != nullptr) {
        // An array of structures, one dimension at a time: the pattern's
        // elements from the left bound, or the other array's (10.9.1, 7.6).
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const ast::Expression& r = unwrapAggregate(rhs);
        const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
        if (isPattern(r)) {
            const std::vector<const ast::Expression*> elems = patternElements(r);
            if (elems.size() != at.range.width())
                unsupported(sl, "assignment pattern of this size");
            for (std::size_t k = 0; k < elems.size(); ++k)
                assignAggregate(withIndex(dst, at.range.left + static_cast<std::int64_t>(k) * step),
                                at.elementType, *elems[k], out, sl);
            return;
        }
        const std::optional<Storage> src =
            structElement(*r.type) != nullptr ? storageOf(r, sl) : std::nullopt;
        if (!src) unsupported(sl, "assignment of this array of structures");
        copyAggregate(std::move(dst), *src, t, out, sl);
        return;
    }
    if (ct.kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const ast::Expression& r = unwrapAggregate(rhs);
        if (isPattern(r)) {
            const std::vector<const ast::Expression*> elems = patternElements(r);
            if (elems.size() != at.range.width())
                unsupported(sl, "assignment pattern of this size");
            const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
            for (std::size_t k = 0; k < elems.size(); ++k)
                assignAggregate(withIndex(dst, at.range.left + static_cast<std::int64_t>(k) * step),
                                at.elementType, *elems[k], out, sl);
        } else {
            const std::optional<Storage> src = storageOf(r, sl);
            if (!src) unsupported(sl, "assignment of this unpacked array value");
            copyArrayStorage(std::move(dst), *src, out, sl);
        }
        return;
    }
    Place p = storagePlace(std::move(dst));
    const Val v = snapshotValue(coerce(lowerExpr(rhs), p.type, sl), sl);
    out.emplace_back(std::move(p), v);
}

void Lowerer::copyAggregate(Storage dst, Storage src, const ast::Type& t, Stores& out,
                            slang::SourceLocation sl) {
    const ast::Type& ct = t.getCanonicalType();
    if (const ast::UnpackedStructType* st = unpackedStruct(ct)) {
        std::uint32_t off = 0;
        for (const ast::FieldSymbol* f : st->fields) {
            copyAggregate(leafAt(dst, off), leafAt(src, off), f->getType(), out, sl);
            off += leafCount(f->getType());
        }
        return;
    }
    if (structElement(ct) != nullptr) {
        // Assignment-compatible arrays can have different declared bounds.
        // Match their elements by position, preserving runtime prefixes (7.6).
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const std::vector<sir::ArrayDim> td = dimsOf(storageBase(dst));
        const std::vector<sir::ArrayDim> sd = dimsOf(storageBase(src));
        if (dst.elem.size() >= td.size() || src.elem.size() >= sd.size())
            unsupported(sl, "assignment of an array of structures of another shape");
        const sir::ArrayDim to = td[dst.elem.size()];
        const sir::ArrayDim from = sd[src.elem.size()];
        const auto count = [](sir::ArrayDim d) {
            return (d.left <= d.right ? d.right - d.left : d.left - d.right) + 1;
        };
        if (count(to) != count(from))
            unsupported(sl, "assignment of an array of structures of another shape");
        for (std::int64_t k = 0; k < count(to); ++k) {
            copyAggregate(withIndex(dst, to.left + (to.left <= to.right ? k : -k)),
                          withIndex(src, from.left + (from.left <= from.right ? k : -k)),
                          at.elementType, out, sl);
        }
        return;
    }
    if (ct.kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        copyArrayStorage(std::move(dst), std::move(src), out, sl);
        return;
    }
    const Val v = snapshotValue(loadPlace(storagePlace(std::move(src)), sl), sl);
    out.emplace_back(storagePlace(std::move(dst)), v);
}

void Lowerer::copyArrayStorage(Storage dst, Storage src, Stores& out, slang::SourceLocation sl) {
    // The prefix may contain runtime indices, evaluated once by storageOf.
    // Walk the remaining dimensions from each side's own left bound (7.6).
    const std::vector<sir::ArrayDim> td = dimsOf(storageBase(dst));
    const std::vector<sir::ArrayDim> sd = dimsOf(storageBase(src));
    if (dst.elem.size() > td.size() || src.elem.size() > sd.size() ||
        td.size() - dst.elem.size() != sd.size() - src.elem.size())
        unsupported(sl, "assignment of an unpacked array of another shape");
    if (dst.elem.size() == td.size()) {
        const Place to = storagePlace(std::move(dst));
        const Val value =
            snapshotValue(coerce(loadPlace(storagePlace(std::move(src)), sl), to.type, sl), sl);
        out.emplace_back(to, value);
        return;
    }
    const sir::ArrayDim t = td[dst.elem.size()];
    const sir::ArrayDim s = sd[src.elem.size()];
    const auto count = [](sir::ArrayDim d) {
        return (d.left <= d.right ? d.right - d.left : d.left - d.right) + 1;
    };
    if (count(t) != count(s)) unsupported(sl, "assignment of an unpacked array of another shape");
    for (std::int64_t k = 0; k < count(t); ++k)
        copyArrayStorage(withIndex(dst, t.left + (t.left <= t.right ? k : -k)),
                         withIndex(src, s.left + (s.left <= s.right ? k : -k)), out, sl);
}

void Lowerer::storageValues(const Storage& s, const ast::Type& t, std::vector<Val>& out,
                            slang::SourceLocation sl) {
    const ast::Type& ct = t.getCanonicalType();
    if (const ast::UnpackedStructType* st = unpackedStruct(ct)) {
        std::uint32_t off = 0;
        for (const ast::FieldSymbol* f : st->fields) {
            storageValues(leafAt(s, off), f->getType(), out, sl);
            off += leafCount(f->getType());
        }
        return;
    }
    if (structElement(ct) != nullptr) {
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
        for (std::int64_t i = at.range.left;; i += step) {
            storageValues(withIndex(s, i), at.elementType, out, sl);
            if (i == at.range.right) break;
        }
        return;
    }
    if (ct.kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        arrayValues(storageBase(s), fixedIndices(s, sl), out, sl);
        return;
    }
    out.push_back(loadPlace(storagePlace(s), sl));
}

void Lowerer::storagePlaces(const Storage& s, const ast::Type& t, std::vector<Place>& out,
                            slang::SourceLocation sl) {
    // The single places of the leaves at s, in the order of storageValues.
    const ast::Type& ct = t.getCanonicalType();
    if (const ast::UnpackedStructType* st = unpackedStruct(ct)) {
        std::uint32_t off = 0;
        for (const ast::FieldSymbol* f : st->fields) {
            storagePlaces(leafAt(s, off), f->getType(), out, sl);
            off += leafCount(f->getType());
        }
        return;
    }
    if (structElement(ct) != nullptr) {
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
        for (std::int64_t i = at.range.left;; i += step) {
            storagePlaces(withIndex(s, i), at.elementType, out, sl);
            if (i == at.range.right) break;
        }
        return;
    }
    if (ct.kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        // Keep runtime prefixes of array members captured by storageOf;
        // remaining dimensions follow this array's declared order.
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
        for (std::int64_t i = at.range.left;; i += step) {
            storagePlaces(withIndex(s, i), at.elementType, out, sl);
            if (i == at.range.right) break;
        }
        return;
    }
    out.push_back(storagePlace(s));
}

void Lowerer::memberDefaults(const Storage& dst, const ast::Type& t, Stores& out,
                             slang::SourceLocation sl) {
    // A member's own initial value (7.2.2); the members of a nested
    // structure that has none get theirs; in an array of structures, in
    // every element.
    const ast::Type& ct = t.getCanonicalType();
    if (structElement(ct) != nullptr && unpackedStruct(ct) == nullptr) {
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
        for (std::int64_t i = at.range.left;; i += step) {
            memberDefaults(withIndex(dst, i), at.elementType, out, sl);
            if (i == at.range.right) break;
        }
        return;
    }
    const ast::UnpackedStructType* st = unpackedStruct(ct);
    if (st == nullptr) return;
    std::uint32_t off = 0;
    for (const ast::FieldSymbol* f : st->fields) {
        const Storage at = leafAt(dst, off);
        if (const ast::Expression* init = f->getInitializer())
            assignAggregate(at, f->getType(), *init, out, sl);
        else
            memberDefaults(at, f->getType(), out, sl);
        off += leafCount(f->getType());
    }
}

}  // namespace kirimasim::front
