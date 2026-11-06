// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Places: where a read or a write of an expression goes, and how it is done.

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

// --- Places ------------------------------------------------------------------

std::optional<ArrayAccess> arrayAccess(const ast::Expression& e) {
    ArrayAccess acc;
    const ast::Expression* cur = &e;
    while (cur->kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = cur->as<ast::ElementSelectExpression>();
        if (!es.value().type->isUnpackedArray()) break;
        acc.indices.push_back(&es.selector());
        cur = &es.value();
    }
    if (acc.indices.empty()) return std::nullopt;
    const ast::Symbol* sym = namedSymbol(*cur);
    // A parameter is read from its slot (SIR_SPEC.md 6).
    if (!Lowerer::isUnpackedMember(*cur) &&
        (sym == nullptr || (!isVariable(*sym) && sym->kind != ast::SymbolKind::Net &&
                            sym->kind != ast::SymbolKind::Parameter)))
        return std::nullopt;
    acc.base = cur;
    std::reverse(acc.indices.begin(), acc.indices.end());
    return acc;
}

Lowerer::Place Lowerer::placeOf(const ast::Expression& lhs, slang::SourceLocation sl) {
    if (auto property = classPlace(lhs, sl)) return *property;
    Place p;
    switch (lhs.kind) {
        case ast::ExpressionKind::NamedValue:
        case ast::ExpressionKind::HierarchicalValue: {
            const ast::Symbol& sym = lhs.as<ast::ValueExpressionBase>().symbol;
            if (sym.kind == ast::SymbolKind::ModportPort) {
                // What the modport port connects to (25.5, 25.5.4).
                const ast::Expression* c = sym.as<ast::ModportPortSymbol>().getConnectionExpr();
                if (c == nullptr) unsupported(sl, "modport port with no connection");
                return placeOf(*c, sl);
            }
            if (const std::optional<sir::FrameVarId> f = findFrameVar(sym)) {
                const sir::FrameVar& fv = frameVar(*f);
                if (!fv.dims.empty()) unsupported(sl, "assignment to a whole unpacked array");
                const bool ref = fv.type.kind == sir::TypeKind::Ref;
                p.kind = ref ? Place::Kind::Ref : Place::Kind::Frame;
                p.frame = *f;
                p.type = ref ? fv.target : fv.type;
                return p;
            }
            if (!isVariable(sym) && sym.kind != ast::SymbolKind::Net)
                unsupported(sl, "assignment to a " + std::string(ast::toString(sym.kind)));
            if (!inSlot(sym))
                unsupported(sl, "automatic variable '" + std::string(sym.name) + "' here");
            p.slot = slotOf(sym.as<ast::ValueSymbol>(), sl);
            if (!design().slot(p.slot).dims.empty())
                unsupported(sl, "assignment to a whole unpacked array");
            p.type = design().slot(p.slot).type;
            return p;
        }
        case ast::ExpressionKind::ElementSelect:
        case ast::ExpressionKind::RangeSelect: {
            const bool element = lhs.kind == ast::ExpressionKind::ElementSelect;
            const ast::Expression& base = element ? lhs.as<ast::ElementSelectExpression>().value()
                                                  : lhs.as<ast::RangeSelectExpression>().value();
            if (element && base.type->isString()) {
                // s[i] is written as s.putc(i, c) (6.16.2).
                p.kind = Place::Kind::StringChar;
                p.base.push_back(placeOf(base, sl));
                p.indices.push_back(
                    lowerExpr(lhs.as<ast::ElementSelectExpression>().selector()).op);
                p.type = sir::u2(8);
                return p;
            }
            if (element && isDynamicContainer(*base.type)) {
                p.kind = Place::Kind::Element;
                p.home = Place::Home::Heap;
                p.container = typeOf(*base.type, sl);
                p.receiver = containerReference(base, false, sl);
                p.indices = {containerIndex(lhs.as<ast::ElementSelectExpression>().selector(),
                                            *p.receiver, p.container, sl)
                                 .op};
                p.type = p.heapElement = typeOf(*lhs.type, sl);
                return p;
            }
            if (element && base.type->isUnpackedArray()) {
                if (structElement(*lhs.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                const std::optional<ArrayAccess> acc = arrayAccess(lhs);
                if (!acc) unsupported(sl, "assignment to an element of this array");
                return arrayElementPlace(*acc->base, acc->indices, sl);
            }
            if (base.kind == ast::ExpressionKind::ElementSelect &&
                isDynamicContainer(*base.as<ast::ElementSelectExpression>().value().type)) {
                p = placeOf(base, sl);
                p.kind = Place::Kind::Bits;
                p.width = lhs.type->getBitWidth();
                p.type = typeOf(*lhs.type, sl);
                if (element) {
                    p.pos = bitPosition(lhs.as<ast::ElementSelectExpression>().selector(),
                                        base.type->getFixedRange(), p.width);
                } else {
                    const auto part = partOf(lhs);
                    p.pos = part.pos;
                    p.lsb = part.lsb;
                }
                return p;
            }
            // Bits of a packed variable, or of an element of an array.
            const ast::Type& bt = base.type->getCanonicalType();
            if (!bt.isIntegral()) unsupported(sl, "assignment to a part of this type");
            p.kind = Place::Kind::Bits;
            const bool property = isObjectProperty(base);
            if (!property && (namedSymbol(base) != nullptr || isUnpackedMember(base))) {
                const std::optional<Storage> st = storageOf(base, sl);
                if (!st) unsupported(sl, "assignment to a part of this variable");
                if (isArrayLeaf(*st)) unsupported(sl, "assignment to a part of an unpacked array");
                // A member of an element of an array of structures is an
                // element of its slot.
                const Place leaf = storagePlace(*st);
                if (leaf.kind == Place::Kind::Frame || leaf.kind == Place::Kind::Ref) {
                    // Bits of an automatic variable, or of what a ref
                    // argument refers to (SIR_SPEC.md 7.4, 8).
                    p.home = leaf.kind == Place::Kind::Ref ? Place::Home::Ref : Place::Home::Frame;
                    p.frame = leaf.frame;
                } else {
                    // An element, of a slot, or of an array a ref argument
                    // refers to: a leaf of a ref structure behind an array
                    // of structures (SIR_SPEC.md 8).
                    p.home = leaf.home;
                    p.slot = leaf.slot;
                    p.frame = leaf.frame;
                    p.indices = leaf.indices;
                    p.elem = leaf.elem;
                }
            } else if (const std::optional<ArrayAccess> acc =
                           property ? std::nullopt : arrayAccess(base)) {
                const Place el = arrayElementPlace(*acc->base, acc->indices, sl);
                p.home = el.home;
                p.slot = el.slot;
                p.frame = el.frame;
                p.indices = el.indices;
                p.elem = el.elem;
            } else {
                // A part of a member or of another part: the bits of
                // that place (narrow).
                Place inner = placeOf(base, sl);
                const std::uint32_t w = lhs.type->getBitWidth();
                if (element) {
                    const Val pos = bitPosition(lhs.as<ast::ElementSelectExpression>().selector(),
                                                bt.getFixedRange(), w);
                    if (pos.op.kind != Operand::Kind::Const)
                        return narrowRuntime(std::move(inner), pos, w, sl);
                    return narrow(
                        std::move(inner),
                        static_cast<std::int64_t>(design().constants[pos.op.id].aWords[0]), w, sl);
                }
                const Part part = partOf(lhs);
                if (part.pos) {
                    // Only an element or bit stays whole inside a parent
                    // chosen at run time; a part can be partly outside it.
                    if (inner.pos)
                        unsupported(sl, "a runtime part-select of a parent chosen at run time");
                    return narrowRuntime(std::move(inner), *part.pos, w, sl);
                }
                return narrow(std::move(inner), part.lsb, w, sl);
            }
            const std::uint32_t w = lhs.type->getBitWidth();
            if (element) {
                const Val pos = bitPosition(lhs.as<ast::ElementSelectExpression>().selector(),
                                            bt.getFixedRange(), w);
                if (pos.op.kind == Operand::Kind::Const)
                    p.lsb = static_cast<std::int64_t>(design().constants[pos.op.id].aWords[0]);
                else
                    p.pos = pos;
            } else {
                const Part part = partOf(lhs);
                p.pos = part.pos;
                p.lsb = part.lsb;
            }
            p.width = w;
            p.type = integral(sir::isFourState(homeType(p)), false, w);
            return p;
        }
        case ast::ExpressionKind::MemberAccess: {
            if (isUnpackedMember(lhs)) {
                // A member of an unpacked structure: its own slot or frame
                // variable (SIR_SPEC.md 4.2).
                if (structElement(*lhs.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                const std::optional<Storage> st = storageOf(lhs, sl);
                if (!st) unsupported(sl, "assignment to a member of this structure");
                if (isArrayLeaf(*st)) unsupported(sl, "assignment to a whole unpacked array");
                return storagePlace(*st);
            }
            // A member of a packed structure or union: its bits (7.2.1,
            // 7.3.1; SIR_SPEC.md 4.2).
            const auto& m = lhs.as<ast::MemberAccessExpression>();
            const ast::Type& vt = m.value().type->getCanonicalType();
            if (!vt.isIntegral() || m.member.kind != ast::SymbolKind::Field || vt.isTaggedUnion())
                unsupported(sl, "assignment to a member of this type");
            const std::int64_t lsb =
                vt.isPackedUnion()
                    ? 0
                    : static_cast<std::int64_t>(m.member.as<ast::FieldSymbol>().bitOffset);
            return narrow(placeOf(m.value(), sl), lsb, lhs.type->getBitWidth(), sl);
        }
        case ast::ExpressionKind::Concatenation: {
            // One packed vector (11.4.12): the parts from the most
            // significant one, their indices evaluated from the left.
            p.kind = Place::Kind::Concat;
            for (const ast::Expression* op : lhs.as<ast::ConcatenationExpression>().operands()) {
                p.base.push_back(placeOf(*op, sl));
                if (!sir::isIntegral(p.base.back().type))
                    unsupported(sl, "a concatenation target of this type");
                p.width += p.base.back().type.width;
            }
            p.type = typeOf(*lhs.type, sl);
            return p;
        }
        default: unsupported(sl, "assignment to this kind of target");
    }
}

Lowerer::Place Lowerer::narrowRuntime(Place base, const Val& pos, std::uint32_t width,
                                      slang::SourceLocation sl) {
    if (base.kind == Place::Kind::Slot || base.kind == Place::Kind::Element) {
        // A select of what is whole here, a property of an object or a static
        // one through a handle, as of a whole variable.
        base.kind = Place::Kind::Bits;
        base.pos = pos;
        base.lsb = 0;
        base.width = width;
        base.type = integral(sir::isFourState(homeType(base)), false, width);
        return base;
    }
    if (base.kind == Place::Kind::Bits && base.pos) {
        // A parent chosen at run time: the child's position from the
        // parent's, made x, which selects nothing, when the child lies
        // outside the parent, so an invalid index never reaches a
        // neighbouring element (7.4.5, 11.5.1; VALUE_SEMANTICS.md 3.11). An
        // invalid parent element already lies outside what holds it.
        const Type pt = integral(true, true, std::max(base.pos->type.width, pos.type.width));
        const Val parent = coerce(*base.pos, pt, sl);
        const Val child = coerce(pos, pt, sl);
        const Val low = emit(Op::Ge, sir::u4(1), {child.op, positionConstant(pt, 0).op}, sl);
        const Val high =
            emit(Op::Le, sir::u4(1), {child.op, positionConstant(pt, base.width - width).op}, sl);
        const Val valid = emit(Op::Land, sir::u4(1), {low.op, high.op}, sl);
        const Val sum = emit(Op::Add, pt, {parent.op, child.op}, sl);
        base.pos = emit(Op::Select, pt, {valid.op, sum.op, allXConstant(pt).op}, sl);
        base.width = width;
        base.type = integral(sir::isFourState(homeType(base)), false, width);
        return base;
    }
    // Keep the parent's boundary instead of merely adding its offset:
    // an invalid child index must never reach a neighbouring member.
    if (base.kind != Place::Kind::Bits || base.pos || base.lsb < 0 ||
        base.lsb + base.width > (base.windowWidth ? base.windowWidth : homeType(base).width))
        unsupported(sl, "a runtime select of this parent packed range");
    base.windowLsb += static_cast<std::uint32_t>(base.lsb);
    base.windowWidth = base.width;
    base.pos = pos;
    base.lsb = 0;
    base.width = width;
    base.type = integral(sir::isFourState(homeType(base)), false, width);
    return base;
}

Lowerer::Place Lowerer::narrow(Place base, std::int64_t lsb, std::uint32_t width,
                               slang::SourceLocation sl) {
    // Bits lsb .. lsb + width - 1 of an integral place. A select outside
    // the place would reach other bits of its slot, so it is reported.
    if (base.kind == Place::Kind::Frame || base.kind == Place::Kind::Ref) {
        // Bits of an automatic variable or of what a ref argument refers to.
        base.home = base.kind == Place::Kind::Ref ? Place::Home::Ref : Place::Home::Frame;
        base.kind = Place::Kind::Slot;
    }
    if (base.kind != Place::Kind::Slot && base.kind != Place::Kind::Element &&
        base.kind != Place::Kind::Bits)
        unsupported(sl, "assignment to a part of this target");
    const std::uint32_t outer = base.kind == Place::Kind::Bits ? base.width : base.type.width;
    if (lsb < 0 || lsb + width > outer) unsupported(sl, "a select outside a member or a part");
    if (base.kind == Place::Kind::Bits) {
        if (base.pos)
            base.pos = emit(
                Op::Add, base.pos->type,
                {base.pos->op, constant(base.pos->type, static_cast<std::uint64_t>(lsb)).op}, sl);
        else
            base.lsb += lsb;
    } else {
        base.lsb = lsb;
    }
    base.kind = Place::Kind::Bits;
    base.width = width;
    base.type = integral(sir::isFourState(homeType(base)), false, width);
    return base;
}

sir::SlotRange Lowerer::rangeOf(const Place& p) const {
    // The longest static prefix (11.5.3): the bits when the element and
    // the part are constant, else the element or the whole slot.
    if (!p.indices.empty() && !p.elem) return {p.slot, 0, 0, 0, 0};
    const std::uint32_t elem = p.elem.value_or(0);
    const std::uint32_t count = p.elem ? 1 : 0;
    if (p.windowWidth != 0) {
        if (!p.pos && p.lsb >= 0 && p.lsb + p.width <= p.windowWidth)
            return {p.slot, elem, count, p.windowLsb + static_cast<std::uint32_t>(p.lsb), p.width};
        return {p.slot, elem, count, p.windowLsb, p.windowWidth};
    }
    if (p.kind == Place::Kind::Bits && !p.pos && p.lsb >= 0 &&
        p.lsb + p.width <= design_.slot(p.slot).type.width)
        return {p.slot, elem, count, static_cast<std::uint32_t>(p.lsb), p.width};
    return {p.slot, elem, count, 0, 0};
}

Val Lowerer::selectedBits(const Place& p, Val all, slang::SourceLocation sl) {
    if (p.windowWidth != 0)
        all = emit(Op::Partsel, integral(sir::isFourState(all.type), false, p.windowWidth),
                   {all.op, constant(sir::u2(32), p.windowLsb).op}, sl);
    const Val pos = p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
    return emit(p.width == 1 ? Op::Bitsel : Op::IdxPartsel, p.type, {all.op, pos.op}, sl);
}

Val Lowerer::loadPlace(const Place& p, slang::SourceLocation sl) {
    if (p.home == Place::Home::Object) {
        if (!p.receiver) throw std::logic_error("missing class receiver in lowering");
        std::vector<Operand> ops{Operand::classField({p.container.object, p.leaf}), p.receiver->op};
        ops.insert(ops.end(), p.indices.begin(), p.indices.end());
        const Val all = emit(Op::ObjectRead, homeType(p), std::move(ops), sl);
        return p.kind == Place::Kind::Bits ? selectedBits(p, all, sl) : all;
    }
    if (p.home == Place::Home::Heap) {
        if (!p.receiver) throw std::logic_error("missing heap receiver in lowering");
        const auto container = emit(Op::LoadRef, p.container, {p.receiver->op}, sl);
        const auto value = emit(Op::ContainerRead, p.heapElement, {container.op, p.indices[0]}, sl);
        return p.kind == Place::Kind::Bits ? selectedBits(p, value, sl) : value;
    }
    // The variable an Element or Bits place is in: a slot, a frame
    // variable, or what a ref argument refers to (SIR_SPEC.md 5.3, 7.4, 8).
    const auto home = [&](Op slotOp, Op frameOp, Op refOp) {
        std::vector<Operand> ops{p.home == Place::Home::Slot ? Operand::slot(p.slot)
                                                             : Operand::frame(p.frameHome())};
        ops.insert(ops.end(), p.indices.begin(), p.indices.end());
        if (p.home == Place::Home::Slot) noteRead(rangeOf(p));
        const Op op = p.home == Place::Home::Slot    ? slotOp
                      : p.home == Place::Home::Frame ? frameOp
                                                     : refOp;
        return emit(op, homeType(p), std::move(ops), sl);
    };
    switch (p.kind) {
        case Place::Kind::Frame: return emit(Op::Fload, p.type, {Operand::frame(p.frame)}, sl);
        case Place::Kind::Ref: {
            std::vector<Operand> ops{Operand::frame(p.frame)};
            if (p.leaf != 0) ops.push_back(constant(sir::u2(32), p.leaf).op);
            return emit(Op::LoadRef, p.type, std::move(ops), sl);
        }
        case Place::Kind::Slot:
            noteRead(rangeOf(p));
            return emit(Op::Load, p.type, {Operand::slot(p.slot)}, sl);
        case Place::Kind::StringChar: {
            // Outside the string: 0 (table 6-9).
            const Val str = loadPlace(p.base[0], sl);
            return emit(Op::StrGet, p.type, {str.op, p.indices[0]}, sl);
        }
        case Place::Kind::Element: return home(Op::LoadElem, Op::LoadElem, Op::LoadRefElem);
        case Place::Kind::Concat: {
            std::vector<Operand> ops;
            ops.reserve(p.base.size());
            for (const Place& part : p.base)
                ops.push_back(loadPlace(part, sl).op);
            return emit(Op::Concat, p.type, std::move(ops), sl);
        }
        case Place::Kind::Bits: {
            const bool whole = p.indices.empty();
            const Val all = home(whole ? Op::Load : Op::LoadElem, whole ? Op::Fload : Op::LoadElem,
                                 whole ? Op::LoadRef : Op::LoadRefElem);
            return selectedBits(p, all, sl);
        }
    }
    unsupported(sl, "reading this target");
}

void Lowerer::storePlace(const Place& p, const Val& v, bool nonblocking, slang::SourceLocation sl,
                         std::optional<Val> delay, EventNbaCapture* event) {
    if (p.home == Place::Home::Object) {
        if (!p.receiver) throw std::logic_error("missing class receiver in lowering");
        if (nonblocking || delay || event)
            unsupported(sl, "class property nonblocking or delayed target");
        const auto field = sir::ClassFieldId{p.container.object, p.leaf};
        const auto& metadata = design_.classes[field.declaring.index()].fields[field.index];
        if (p.kind == Place::Kind::Bits) {
            // The whole property goes through a private copy, so store.bits
            // decides what an invalid position writes, as for any variable
            // (11.5.1; VALUE_SEMANTICS.md 3.11). Nothing waits in between, so
            // other processes see one write.
            if (metadata.constant) unsupported(sl, "instance const property initialization target");
            Place element = p;
            element.kind = Place::Kind::Element;
            element.type = homeType(p);
            element.pos.reset();
            element.lsb = 0;
            element.width = 0;
            element.windowLsb = 0;
            element.windowWidth = 0;
            const sir::FrameVarId copy = newFrameVar(element.type);
            emitVoid(Op::Fstore, {Operand::frame(copy), loadPlace(element, sl).op}, sl);
            Place bits = p;
            bits.home = Place::Home::Frame;
            bits.frame = copy;
            bits.indices.clear();
            bits.elem.reset();
            bits.receiver.reset();
            storePlace(bits, v, false, sl);
            storePlace(element, emit(Op::Fload, element.type, {Operand::frame(copy)}, sl), false,
                       sl);
            return;
        }
        std::vector<Operand> ops{Operand::classField(field)};
        if (metadata.constant) {
            if (!b_.isSub || design_.subroutine(b_.sub).classRole != sir::ClassRole::Constructor ||
                design_.subroutine(b_.sub).classOwner != field.declaring || p.frame == sir::kNone ||
                !p.indices.empty())
                unsupported(sl, "instance const property initialization target");
        } else {
            ops.push_back(p.receiver->op);
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
        }
        ops.push_back(coerce(v, p.type, sl).op);
        emitVoid(metadata.constant ? Op::ObjectInit : Op::ObjectWrite, std::move(ops), sl);
        return;
    }
    if (p.home == Place::Home::Slot && clockvars_.contains(p.slot))
        unsupported(sl, "write to a read-only clocking input");
    if (sir::isContainer(p.type) && !nonblocking && p.home != Place::Home::Heap) {
        const auto ref = referencePlace(p, false, sl);
        emitVoid(Op::ContainerAssign, {Operand::typeRow(p.type.detail), ref.op, v.op}, sl);
        return;
    }
    if (p.home == Place::Home::Heap) {
        if (!p.receiver) throw std::logic_error("missing heap receiver in lowering");
        if (nonblocking)
            error(sl, "nonblocking assignment to a dynamically sized array element is illegal");
        if (p.kind == Place::Kind::Bits) {
            const auto ref = referencePlace(Place{.kind = Place::Kind::Element,
                                                  .home = Place::Home::Heap,
                                                  .indices = p.indices,
                                                  .type = p.heapElement,
                                                  .receiver = p.receiver,
                                                  .container = p.container,
                                                  .heapElement = p.heapElement},
                                            false, sl);
            const auto local = newFrameVar(sir::refType());
            frameVar(local).target = p.heapElement;
            emitVoid(Op::Fstore, {Operand::frame(local), ref.op}, sl);
            auto target = Operand::frame(local);
            target.lsb = p.windowLsb;
            target.width = p.windowWidth;
            const auto pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            emitVoid(Op::StoreRefBits, {target, pos.op, coerce(v, p.type, sl).op}, sl);
        } else {
            const auto value = sir::isContainer(p.heapElement) ? v : coerce(v, p.heapElement, sl);
            emitVoid(Op::ContainerWrite,
                     {Operand::typeRow(p.container.detail), p.receiver->op, p.indices[0], value.op},
                     sl);
        }
        return;
    }
    if (event != nullptr && p.kind != Place::Kind::Concat && p.kind != Place::Kind::StringChar) {
        captureNbaWrite(*event, p, v, sl);
        return;
    }
    const auto write = [&](Op blocking, Op nba, Op delayed, std::vector<Operand> ops) {
        if (delay) ops.push_back(delay->op);
        emitVoid(delay ? delayed : nonblocking ? nba : blocking, std::move(ops), sl);
    };
    // An automatic variable takes no NBA (10.4.2): not through a ref either,
    // unless the ref names a whole static variable (nba.ref).
    const auto noNba = [&] {
        if (nonblocking) unsupported(sl, "nonblocking assignment to an automatic variable");
    };
    // A delayed continuous assignment, gate or UDP output writes a slot
    // range with drive.delay (SIR_SPEC.md 5.4).
    if (b_.delays) {
        // A whole slot or constant bits of one; the other places have no
        // slot to look up.
        const bool whole = p.kind == Place::Kind::Slot;
        const bool bits = p.kind == Place::Kind::Bits && p.home == Place::Home::Slot && !p.pos &&
                          (p.indices.empty() || p.elem.has_value());
        const bool element =
            p.kind == Place::Kind::Element && p.home == Place::Home::Slot && p.elem.has_value();
        if (!whole && !bits && !element) unsupported(sl, "a delayed write of this target");
        const sir::Slot& slot = design().slot(p.slot);
        const bool part = bits && p.lsb >= 0 && p.lsb + p.width <= slot.type.width;
        if (!whole && !part && !element) unsupported(sl, "a delayed write of this target");
        const Val dv = coerce(v, p.type, sl);
        Operand target = part ? Operand::slot(p.slot, static_cast<std::uint32_t>(p.lsb), p.width)
                              : Operand::slot(p.slot);
        if (p.elem) {
            target.firstElem = *p.elem;
            target.numElems = 1;
        }
        emitVoid(Op::DriveDelay,
                 {target, dv.op, (*b_.delays)[0].op, (*b_.delays)[1].op, (*b_.delays)[2].op}, sl);
        noteWrite(rangeOf(p));
        return;
    }
    switch (p.kind) {
        case Place::Kind::Frame: {
            noNba();
            const Val fv = coerce(v, p.type, sl);
            emitVoid(Op::Fstore, {Operand::frame(p.frame), fv.op}, sl);
            return;
        }
        case Place::Kind::Ref: {
            std::vector<Operand> ops{Operand::frame(p.frame)};
            if (p.leaf != 0) ops.push_back(constant(sir::u2(32), p.leaf).op);
            ops.push_back(coerce(v, p.type, sl).op);
            write(Op::StoreRef, Op::NbaRef, Op::NbaRefDelay, std::move(ops));
            return;
        }
        case Place::Kind::Slot: {
            const Val sv = coerce(v, p.type, sl);
            write(Op::Store, Op::Nba, Op::NbaDelay, {Operand::slot(p.slot), sv.op});
            noteWrite(rangeOf(p));
            return;
        }
        case Place::Kind::StringChar: {
            const Val c = coerce(v, sir::u2(8), sl);
            if (event != nullptr) {
                captureNbaStringChar(*event, p, c, sl);
                return;
            }
            if (nonblocking || delay) {
                // An NBA keeps the string, the character's index and the
                // byte, and writes the byte into the string held when it
                // applies, not the whole string as it is now
                // (SCHEDULER_SPEC.md 13).
                const Place& str = p.base[0];
                std::vector<Operand> ops;
                if (str.kind == Place::Kind::Slot ||
                    (str.kind == Place::Kind::Element && str.home == Place::Home::Slot)) {
                    ops.push_back(Operand::slot(str.slot));
                    ops.insert(ops.end(), str.indices.begin(), str.indices.end());
                    noteWrite(sir::SlotRange{str.slot, 0, 0, 0, 0});
                } else if ((str.kind == Place::Kind::Ref && str.leaf == 0) ||
                           (str.kind == Place::Kind::Element && str.home == Place::Home::Ref)) {
                    ops.push_back(
                        Operand::frame(str.kind == Place::Kind::Ref ? str.frame : str.frameHome()));
                    ops.insert(ops.end(), str.indices.begin(), str.indices.end());
                } else {
                    noNba();
                    unsupported(sl, "nonblocking assignment to this string character");
                }
                ops.push_back(p.indices[0]);
                ops.push_back(c.op);
                if (delay) ops.push_back(delay->op);
                emitVoid(delay ? Op::NbaStrCharDelay : Op::NbaStrChar, std::move(ops), sl);
                return;
            }
            // s[i] = c is s.putc(i, c) (6.16.2): the new string to s.
            const Type st{sir::TypeKind::String, 0};
            const Val str = loadPlace(p.base[0], sl);
            const Val n =
                emit(Op::Builtin, st, {Operand::str("putc"), str.op, p.indices[0], c.op}, sl);
            storePlace(p.base[0], n, nonblocking, sl, delay, event);
            return;
        }
        case Place::Kind::Concat: {
            // Each part gets its bits of the value, the most significant
            // part the most significant bits.
            const Val all = coerce(v, p.type, sl);
            std::uint32_t lsb = p.width;
            for (const Place& part : p.base) {
                const std::uint32_t w = part.type.width;
                lsb -= w;
                const Val bits = emit(Op::Partsel, integral(sir::isFourState(all.type), false, w),
                                      {all.op, constant(sir::u2(32), lsb).op}, sl);
                storePlace(part, bits, nonblocking, sl, delay, event);
            }
            return;
        }
        case Place::Kind::Element: {
            const Val ev = coerce(v, p.type, sl);
            if (p.home != Place::Home::Slot) {
                // Through a ref, an NBA with or without a delay goes to the
                // static variable it names (nba.ref.elem, checked at run
                // time); a frame's own element takes none (10.4.2).
                const bool refNba = p.home == Place::Home::Ref && (nonblocking || delay);
                if (!refNba) noNba();
                std::vector<Operand> ops{Operand::frame(p.frameHome())};
                ops.insert(ops.end(), p.indices.begin(), p.indices.end());
                ops.push_back(ev.op);
                if (refNba) {
                    if (delay) ops.push_back(delay->op);
                    emitVoid(delay ? Op::NbaRefElemDelay : Op::NbaRefElem, std::move(ops), sl);
                    return;
                }
                emitVoid(p.home == Place::Home::Frame ? Op::StoreElem : Op::StoreRefElem,
                         std::move(ops), sl);
                return;
            }
            std::vector<Operand> ops{Operand::slot(p.slot)};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            ops.push_back(ev.op);
            write(Op::StoreElem, Op::NbaElem, Op::NbaElemDelay, std::move(ops));
            noteWrite(rangeOf(p));
            return;
        }
        case Place::Kind::Bits: {
            const Val pv = coerce(v, p.type, sl);
            Operand target =
                p.home == Place::Home::Slot ? Operand::slot(p.slot) : Operand::frame(p.frameHome());
            target.lsb = p.windowLsb;
            target.width = p.windowWidth;
            const Val pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            if (p.home != Place::Home::Slot) {
                const bool refNba = p.home == Place::Home::Ref && (nonblocking || delay);
                if (!refNba) noNba();
                std::vector<Operand> ops{target};
                ops.insert(ops.end(), p.indices.begin(), p.indices.end());
                ops.push_back(pos.op);
                ops.push_back(pv.op);
                if (refNba) {
                    if (delay) ops.push_back(delay->op);
                    emitVoid(delay ? Op::NbaRefBitsDelay : Op::NbaRefBits, std::move(ops), sl);
                    return;
                }
                emitVoid(p.home == Place::Home::Frame ? Op::StoreBits : Op::StoreRefBits,
                         std::move(ops), sl);
                return;
            }
            const sir::Slot& slot = design().slot(p.slot);
            if (!p.pos && p.indices.empty() && p.windowWidth == 0) {
                // A constant part of a whole variable: an ordinary store to a
                // slot range. A part wholly outside writes nothing (3.11).
                if (p.lsb >= static_cast<std::int64_t>(slot.type.width) ||
                    p.lsb + static_cast<std::int64_t>(p.width) <= 0)
                    return;
                if (p.lsb >= 0 && p.lsb + p.width <= slot.type.width) {
                    write(
                        Op::Store, Op::Nba, Op::NbaDelay,
                        {Operand::slot(p.slot, static_cast<std::uint32_t>(p.lsb), p.width), pv.op});
                    noteWrite(rangeOf(p));
                    return;
                }
            }
            std::vector<Operand> ops{target};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            ops.push_back(pos.op);
            ops.push_back(pv.op);
            write(Op::StoreBits, Op::NbaBits, Op::NbaBitsDelay, std::move(ops));
            noteWrite(rangeOf(p));
            return;
        }
    }
}

}  // namespace kirimasim::front
