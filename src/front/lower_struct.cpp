// Lowering of unpacked structures (SIR_SPEC.md 4.2).
//
// A variable of an unpacked structure type has one slot, or one frame
// variable when it is automatic, for each innermost member (a leaf), in
// declaration order. A member is then read and written like a variable of
// its own type. A whole structure is assigned member by member: from an
// assignment pattern, whose elements slang gives in member order, or from
// another structure of the same type, leaf by leaf.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"

namespace kirimasim::front {

bool hasMemberDefaults(const ast::Type& t) {
    const ast::UnpackedStructType* st = unpackedStruct(t);
    if (st == nullptr) return false;
    for (const ast::FieldSymbol* f : st->fields)
        if (f->getInitializer() != nullptr || hasMemberDefaults(f->getType())) return true;
    return false;
}

bool Lowerer::isUnpackedMember(const ast::Expression& e) {
    return e.kind == ast::ExpressionKind::MemberAccess &&
           unpackedStruct(*e.as<ast::MemberAccessExpression>().value().type) != nullptr;
}

sir::FrameVarId Lowerer::structFrameVars(const ast::VariableSymbol& v, slang::SourceLocation sl) {
    if (auto it = b_.structFrames.find(&v); it != b_.structFrames.end()) return it->second;
    std::vector<Leaf> leaves;
    structLeaves(v.getType(), "", leaves);
    const auto first = static_cast<sir::FrameVarId>(frameVars().size());
    for (const Leaf& l : leaves) {
        if (l.type->getCanonicalType().isUnpackedArray())
            unsupported(sl, "unpacked array member of an automatic structure");
        const sir::Type t = typeOf(*l.type, sl);
        if (t.kind == sir::TypeKind::Event) unsupported(sl, "automatic event variable");
        frameVars().push_back({std::string(v.name) + l.path, t, sir::kNone});
    }
    b_.structFrames.emplace(&v, first);
    return first;
}

std::optional<Lowerer::Storage> Lowerer::storageOf(const ast::Expression& e,
                                                   slang::SourceLocation sl) {
    if (isUnpackedMember(e)) {
        // A member: the structure's leaves, from the member's first one.
        const auto& m = e.as<ast::MemberAccessExpression>();
        if (m.member.kind != ast::SymbolKind::Field) return std::nullopt;
        std::optional<Storage> base = storageOf(m.value(), sl);
        if (!base) return std::nullopt;
        base->id += leafOffset(*unpackedStruct(*m.value().type), m.member.as<ast::FieldSymbol>());
        return base;
    }
    const ast::Symbol* sym = namedSymbol(e);
    if (sym == nullptr || (!isVariable(*sym) && sym->kind != ast::SymbolKind::Net))
        return std::nullopt;
    const auto& v = sym->as<ast::ValueSymbol>();
    if (unpackedStruct(v.getType()) != nullptr) {
        if (auto it = b_.structFrames.find(sym); it != b_.structFrames.end())
            return Storage{true, it->second};
        if (!inSlot(*sym)) unsupported(sl, "automatic variable '" + std::string(v.name) + "' here");
        return Storage{false, structSlots(v, sl)};
    }
    if (const std::optional<sir::FrameVarId> f = findFrameVar(*sym)) return Storage{true, *f};
    if (!inSlot(*sym)) return std::nullopt;
    return Storage{false, slotOf(v, sl)};
}

Lowerer::Place Lowerer::storagePlace(Storage st) {
    Place p;
    if (st.frame) {
        p.kind = Place::Kind::Frame;
        p.frame = st.id;
        p.type = frameVars()[st.id].type;
    } else {
        p.kind = Place::Kind::Slot;
        p.slot = st.id;
        p.type = design().slots[st.id].type;
    }
    return p;
}

void Lowerer::assignStruct(Storage dst, const ast::Type& t, const ast::Expression& rhs,
                           bool nonblocking, slang::SourceLocation sl) {
    const ast::UnpackedStructType& st = *unpackedStruct(t);
    const ast::Expression* r = &rhs;
    while (r->kind == ast::ExpressionKind::Conversion &&
           unpackedStruct(*r->as<ast::ConversionExpression>().operand().type) != nullptr)
        r = &r->as<ast::ConversionExpression>().operand();
    if (isPattern(*r)) {
        // One element per member, in declaration order (10.9.2).
        const std::vector<const ast::Expression*> elems = patternElements(*r);
        if (elems.size() != st.fields.size()) unsupported(sl, "assignment pattern of this size");
        std::uint32_t off = 0;
        for (std::size_t i = 0; i < elems.size(); ++i) {
            const ast::Type& ft = st.fields[i]->getType();
            assignMember({dst.frame, dst.id + off}, ft, *elems[i], nonblocking, sl);
            off += leafCount(ft);
        }
        return;
    }
    // Another structure of the same type (6.22.2): leaf by leaf.
    const std::optional<Storage> src =
        unpackedStruct(*r->type) != nullptr ? storageOf(*r, sl) : std::nullopt;
    if (!src) unsupported(sl, "assignment of this unpacked structure value");
    std::vector<Leaf> leaves;
    structLeaves(t, "", leaves);
    for (std::uint32_t k = 0; k < leaves.size(); ++k) {
        const Storage from{src->frame, src->id + k};
        const Storage to{dst.frame, dst.id + k};
        if (leaves[k].type->getCanonicalType().isUnpackedArray()) {
            // Automatic structures have no array members (structFrameVars).
            copyArray(to.id, {}, from.id, {}, nonblocking, sl);
            continue;
        }
        storePlace(storagePlace(to), loadPlace(storagePlace(from), sl), nonblocking, sl);
    }
}

void Lowerer::assignMember(Storage dst, const ast::Type& t, const ast::Expression& rhs,
                           bool nonblocking, slang::SourceLocation sl) {
    if (unpackedStruct(t) != nullptr) {
        assignStruct(dst, t, rhs, nonblocking, sl);
        return;
    }
    if (t.getCanonicalType().isUnpackedArray()) {
        assignArray(dst.id, {}, rhs, nonblocking, sl);
        return;
    }
    const Place p = storagePlace(dst);
    storePlace(p, coerce(lowerExpr(rhs), p.type, sl), nonblocking, sl);
}

void Lowerer::memberDefaults(Storage dst, const ast::Type& t, slang::SourceLocation sl) {
    // A member's own initial value (7.2.2); the members of a nested
    // structure that has none get theirs.
    const ast::UnpackedStructType* st = unpackedStruct(t);
    if (st == nullptr) return;
    std::uint32_t off = 0;
    for (const ast::FieldSymbol* f : st->fields) {
        const Storage at{dst.frame, dst.id + off};
        if (const ast::Expression* init = f->getInitializer())
            assignMember(at, f->getType(), *init, false, sl);
        else
            memberDefaults(at, f->getType(), sl);
        off += leafCount(f->getType());
    }
}

}  // namespace kirimasim::front
