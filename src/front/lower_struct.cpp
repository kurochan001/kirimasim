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
    s.id += off;
    return s;
}

Lowerer::Storage Lowerer::withIndex(Storage s, std::int64_t index) {
    s.elem.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(index)).op);
    s.constIdx.push_back(index);
    return s;
}

bool Lowerer::isArrayLeaf(const Storage& s) {
    if (s.frame) return !frameVars()[s.id].dims.empty();
    return design_.slots[s.id].dims.size() > s.elem.size();
}

Lowerer::ArrayBase Lowerer::storageBase(const Storage& s) {
    if (!s.frame) return s.id;
    const bool ref = frameVars()[s.id].type.kind == sir::TypeKind::Ref;
    return {ref ? Place::Home::Ref : Place::Home::Frame, s.id};
}

std::vector<std::int64_t> Lowerer::fixedIndices(const Storage& s, slang::SourceLocation sl) {
    if (s.dynamic) unsupported(sl, "an array member of an element chosen at run time");
    return s.constIdx;
}

sir::FrameVarId Lowerer::structFrameVars(const ast::VariableSymbol& v, slang::SourceLocation sl) {
    if (auto it = b_.structFrames.find(&v); it != b_.structFrames.end()) return it->second;
    std::vector<Leaf> leaves;
    structLeaves(v.getType(), "", leaves);
    const auto first = static_cast<sir::FrameVarId>(frameVars().size());
    for (const Leaf& l : leaves) {
        if (l.type->getCanonicalType().isUnpackedArray() || !l.dims.empty())
            unsupported(sl, "automatic unpacked array in a structure or of structures");
        const sir::Type t = typeOf(*l.type, sl);
        if (t.kind == sir::TypeKind::Event) unsupported(sl, "automatic event variable");
        frameVars().push_back({std::string(v.name) + l.path, t, sir::kNone});
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
        if (unpackedStruct(rt) == nullptr && !rt.getCanonicalType().isUnpackedArray())
            return std::nullopt;
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
        if (!vt.isUnpackedArray() || structElement(vt) == nullptr) return std::nullopt;
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
        st.id = paramSlot(sym->as<ast::ParameterSymbol>(), sl);
        return st;
    }
    if (sym == nullptr || (!isVariable(*sym) && sym->kind != ast::SymbolKind::Net))
        return std::nullopt;
    const auto& v = sym->as<ast::ValueSymbol>();
    Storage st;
    if (structElement(v.getType()) != nullptr) {
        if (auto it = b_.structFrames.find(sym); it != b_.structFrames.end()) {
            st.frame = true;
            st.id = it->second.index();
            return st;
        }
        if (!inSlot(*sym)) unsupported(sl, "automatic variable '" + std::string(v.name) + "' here");
        st.id = structSlots(v, sl);
        return st;
    }
    if (const std::optional<sir::FrameVarId> f = findFrameVar(*sym)) {
        st.frame = true;
        st.id = f->index();
        return st;
    }
    if (!inSlot(*sym)) return std::nullopt;
    st.id = slotOf(v, sl);
    return st;
}

Lowerer::Place Lowerer::storagePlace(Storage st) {
    // One single leaf: a frame variable, a whole slot, or the element of an
    // array of structures.
    Place p;
    if (st.frame) {
        // An automatic variable, or what a ref argument refers to (SIR_SPEC.md 8).
        const sir::FrameVar& fv = frameVars()[st.id];
        const bool ref = fv.type.kind == sir::TypeKind::Ref;
        p.kind = ref ? Place::Kind::Ref : Place::Kind::Frame;
        p.frame = sir::FrameVarId{st.id};
        p.type = ref ? fv.target : fv.type;
        return p;
    }
    const sir::Slot& slot = design().slots[st.id];
    p.slot = st.id;
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
    if (ct.isUnpackedArray()) {
        assignArray(storageBase(dst), fixedIndices(dst, sl), rhs, out, sl);
        return;
    }
    Place p = storagePlace(std::move(dst));
    const Val v = coerce(lowerExpr(rhs), p.type, sl);
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
        // Element by element; both sides have the same type, so the same
        // ranges (7.6).
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        const std::int64_t step = at.range.left <= at.range.right ? 1 : -1;
        for (std::int64_t i = at.range.left;; i += step) {
            copyAggregate(withIndex(dst, i), withIndex(src, i), at.elementType, out, sl);
            if (i == at.range.right) break;
        }
        return;
    }
    if (ct.isUnpackedArray()) {
        copyArray(storageBase(dst), fixedIndices(dst, sl), storageBase(src), fixedIndices(src, sl),
                  out, sl);
        return;
    }
    const Val v = loadPlace(storagePlace(std::move(src)), sl);
    out.emplace_back(storagePlace(std::move(dst)), v);
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
    if (ct.isUnpackedArray()) {
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
    if (ct.isUnpackedArray()) {
        arrayPlaces(storageBase(s), fixedIndices(s, sl), out);
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
