// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of $sscanf, $fscanf and $fread to SIR.

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

namespace {

/// The conversions of a $sscanf format that assign, in order, as letters
/// (%x is h; %e and %g are f); false with the specifier in `bad` for one
/// that is not supported (SIR_SPEC.md 8, SYSTEM_TASKS.md 6.3).
bool scanConversions(std::string_view f, std::vector<char>& convs, std::string& bad,
                     bool allowBinary = true) {
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%') continue;
        bool assigns = true;
        ++i;
        if (i < f.size() && f[i] == '*') {
            assigns = false;
            ++i;
        }
        while (i < f.size() && f[i] >= '0' && f[i] <= '9')
            ++i;
        if (i >= f.size()) {
            bad = "%";
            return false;
        }
        auto c = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
        if (c == '%') continue;
        if (c == 'x') c = 'h';
        if (c == 'e' || c == 'g') c = 'f';
        if ((allowBinary ? std::string_view("bohdcsfuz") : std::string_view("bohdcsf")).find(c) ==
            std::string_view::npos) {
            bad = std::string("%") + f[i];
            return false;
        }
        if (assigns) convs.push_back(c);
    }
    return true;
}

}  // namespace

Val Lowerer::lowerScanCall(const ast::CallExpression& call, Type rt, slang::SourceLocation sl) {
    // $sscanf(input, format, targets...), $fscanf(fd, format, targets...)
    // and $value$plusargs(format, target): each target that a conversion
    // assigns gets a frame variable
    // of its type, which the syscall writes; the targets are assigned
    // after it, as many as it reports (SIR_SPEC.md 8).
    const std::string name(call.getSubroutineName());
    const bool plus = name == "$value$plusargs";
    const auto args = call.arguments();
    const std::size_t fmtArg = plus ? 0 : 1;
    if (args.size() <= fmtArg) unsupported(sl, name + " without a format");
    const std::optional<slang::ConstantValue> fc = constantValue(*args[fmtArg]);
    std::optional<std::string> fmt;
    if (fc && fc->isString()) fmt = fc->str();
    if (fc && fc->isInteger() && (plus || !fc->integer().hasUnknown()))
        fmt = stringBytes(fc->integer());
    std::vector<char> convs;
    std::string bad;
    if (plus && fmt) {
        // name%conv: the one conversion after the name (21.6).
        const std::size_t pct = fmt->find('%');
        if (pct == std::string::npos || !scanConversions(fmt->substr(pct), convs, bad, false) ||
            convs.size() != 1)
            unsupported(sl, name + " with this format");
    } else if (fmt && !scanConversions(*fmt, convs, bad)) {
        unsupported(sl, "format specifier " + bad + " in " + name);
    }
    const std::span<const ast::Expression* const> targets = args.subspan(fmtArg + 1);
    // Fewer targets than conversions: an error (SYSTEM_TASKS.md 6.3); more
    // are ignored (21.3.4.3).
    if (targets.size() < convs.size())
        error(sl, name + " has fewer arguments than its format has conversions");
    std::vector<Operand> ops{Operand::str(name)};
    if (name == "$fscanf") {
        ops.push_back(lowerDescriptor(*args[0], sl).op);
    } else if (!plus) {
        // A literal input of any length as a string (VALUE_SEMANTICS.md 7.6).
        const ast::Expression& ie = args[0]->unwrapImplicitConversions();
        std::string text;
        if (ie.kind == ast::ExpressionKind::StringLiteral) {
            text = ie.as<ast::StringLiteral>().getValue();
            std::erase(text, '\0');
        }
        Val in = ie.kind == ast::ExpressionKind::StringLiteral ? stringConstant(text)
                                                               : lowerExpr(*args[0]);
        if (!isString(in.type) && !sir::isIntegral(in.type))
            unsupported(sl, name + " of an input of this type");
        ops.push_back(in.op);
    }
    if (fmt) {
        observeRuntimeConstant(*args[fmtArg]);
        ops.push_back(Operand::str(*fmt));
    } else {
        const Val format = lowerExpr(*args[fmtArg]);
        if (!isString(format.type) && !sir::isIntegral(format.type))
            unsupported(sl, name + " of a format of this type");
        ops.push_back(format.op);
    }
    std::vector<std::pair<const ast::Expression*, sir::FrameVarId>> outs;
    for (std::size_t i = 0; i < (fmt ? convs.size() : targets.size()); ++i) {
        const ast::Expression* t = targets[i];
        if (t->kind == ast::ExpressionKind::Assignment &&
            t->as<ast::AssignmentExpression>().isLValueArg())
            t = &t->as<ast::AssignmentExpression>().left();
        const ast::Type& tt = t->type->getCanonicalType();
        if (fmt) {
            const char c = convs[i];
            const bool ok = c == 'f'   ? tt.isFloating()
                            : c == 's' ? tt.isString() || tt.isIntegral()
                                       : tt.isIntegral();
            if (!ok) unsupported(sl, std::string("%") + c + " into a target of this type");
        }
        const sir::FrameVarId f = newFrameVar(typeOf(tt, sl));
        ops.push_back(Operand::frame(f));
        outs.emplace_back(t, f);
    }
    const Val n = emit(Op::Syscall, sir::s2(32), std::move(ops), sl);
    // Target i is assigned when the count is above i.
    for (std::size_t i = 0; i < outs.size(); ++i) {
        const Val above = emit(Op::Gt, sir::u2(1),
                               {n.op, constant(sir::s2(32), static_cast<std::uint64_t>(i)).op}, sl);
        const sir::BlockId store = newBlock();
        const sir::BlockId next = newBlock();
        terminate(Op::BrIf, {above.op, Operand::block(store), Operand::block(next)}, sl);
        setBlock(store);
        const Val v =
            emit(Op::Fload, frameVar(outs[i].second).type, {Operand::frame(outs[i].second)}, sl);
        lowerStore(*outs[i].first, v, false, sl);
        terminate(Op::Br, {Operand::block(next)}, sl);
        setBlock(next);
    }
    return coerce(n, rt, sl);
}

Val Lowerer::lowerFreadMemory(const ast::CallExpression& call, Type rt, slang::SourceLocation sl) {
    // $fread(mem, fd[, start[, count]]): from start (the lowest address by
    // default) toward the highest, at most count elements, each read as
    // $fread of an integral variable reads it, until one is not read whole
    // (SYSTEM_TASKS.md 6.3, 21.3.4.4). The count of bytes read is returned.
    // A memory of unpacked structures reads each element's members in
    // declaration order (21.3.4.4).
    const auto args = call.arguments();
    const ast::Expression* m = args[0];
    if (m->kind == ast::ExpressionKind::Assignment &&
        m->as<ast::AssignmentExpression>().isLValueArg())
        m = &m->as<ast::AssignmentExpression>().left();
    const ast::Type& mt = m->type->getCanonicalType();
    const bool structs = structElement(mt) != nullptr;
    std::optional<Storage> base;
    std::vector<sir::ArrayDim> remaining;
    sir::ArrayDim d;
    if (structs) {
        // One dimension of structures, whose leaves are slots or frame
        // variables with that dimension (SIR_SPEC.md 4.2).
        const auto* at = mt.kind == ast::SymbolKind::FixedSizeUnpackedArrayType
                             ? &mt.as<ast::FixedSizeUnpackedArrayType>()
                             : nullptr;
        if (at == nullptr || unpackedStruct(at->elementType) == nullptr)
            unsupported(sl, "$fread into a memory of this kind");
        base = storageOf(*m, sl);
        if (!base || !base->elem.empty()) unsupported(sl, "$fread into a memory of this kind");
        d = {at->range.left, at->range.right};
    } else {
        // storageOf captures a selected prefix before descriptor/start/count,
        // including runtime indices, using the ordinary slot/frame/ref path.
        base = storageOf(*m, sl);
        if (!base) unsupported(sl, "$fread into a memory of this kind");
        const auto memory = storageBase(*base);
        const auto dims = dimsOf(memory);
        if (dims.size() <= base->elem.size()) unsupported(sl, "$fread into a memory of this kind");
        if (!sir::isIntegral(elemTypeOf(memory)))
            unsupported(sl, "$fread into a memory of this element type");
        remaining.assign(dims.begin() + static_cast<std::ptrdiff_t>(base->elem.size()), dims.end());
        d = remaining.back();
        if (remaining.size() > 1) {
            std::uint64_t elements = 1;
            for (const auto dim : remaining) {
                const auto size = static_cast<std::uint64_t>(std::max(dim.left, dim.right) -
                                                             std::min(dim.left, dim.right)) +
                                  1;
                if (size > std::uint64_t{INT64_MAX} / elements)
                    unsupported(sl, "$fread memory exceeds the storage size limit");
                elements *= size;
            }
            d = {0, static_cast<std::int64_t>(elements - 1)};
        }
    }
    const std::int64_t lo = std::min(d.left, d.right);
    const std::int64_t hi = std::max(d.left, d.right);
    const Type i64 = sir::s2(64);
    const auto given = [&](std::size_t i) {
        return i < args.size() && args[i]->kind != ast::ExpressionKind::EmptyArgument;
    };
    const Val desc = lowerDescriptor(*args[1], sl);
    const Val start = given(2) ? coerce(lowerExpr(*args[2]), i64, sl)
                               : constant(i64, static_cast<std::uint64_t>(lo));
    const Val count = given(3) ? coerce(lowerExpr(*args[3]), i64, sl)
                               : constant(i64, static_cast<std::uint64_t>(hi - lo + 1));
    const sir::FrameVarId addr = newFrameVar(i64);
    const sir::FrameVarId left = newFrameVar(i64);
    const sir::FrameVarId total = newFrameVar(sir::s2(32));
    emitVoid(Op::Fstore, {Operand::frame(addr), start.op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(left), count.op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(total), constant(sir::s2(32), 0).op}, sl);
    const sir::BlockId head = newBlock();
    const sir::BlockId body = newBlock();
    const sir::BlockId done = newBlock();
    terminate(Op::Br, {Operand::block(head)}, sl);
    // While the address is in the memory and the count is not used up.
    setBlock(head);
    const Val at = emit(Op::Fload, i64, {Operand::frame(addr)}, sl);
    const Val inMem = emit(
        Op::And, sir::u2(1),
        {emit(Op::Ge, sir::u2(1), {at.op, constant(i64, static_cast<std::uint64_t>(lo)).op}, sl).op,
         emit(Op::Le, sir::u2(1), {at.op, constant(i64, static_cast<std::uint64_t>(hi)).op}, sl)
             .op},
        sl);
    const Val more =
        emit(Op::And, sir::u2(1),
             {inMem.op,
              emit(Op::Gt, sir::u2(1),
                   {emit(Op::Fload, i64, {Operand::frame(left)}, sl).op, constant(i64, 0).op}, sl)
                  .op},
             sl);
    terminate(Op::BrIf, {more.op, Operand::block(body), Operand::block(done)}, sl);
    // The element at the address: its one place, or its members' places.
    setBlock(body);
    const Val at2 = emit(Op::Fload, i64, {Operand::frame(addr)}, sl);
    std::vector<Place> places;
    if (structs) {
        Storage e = *base;
        e.elem.push_back(at2.op);
        e.dynamic = true;
        freadPlaces(e, mt.as<ast::FixedSizeUnpackedArrayType>().elementType, places, sl);
    } else {
        const auto memory = storageBase(*base);
        Place p;
        p.kind = Place::Kind::Element;
        p.home = memory.home;
        p.slot = memory.slot;
        p.frame = memory.frame;
        p.type = elemTypeOf(memory);
        p.indices = base->elem;
        if (remaining.size() == 1) {
            p.indices.push_back(at2.op);
        } else {
            std::vector<Operand> indices(remaining.size());
            Val address = at2;
            for (std::size_t i = remaining.size(); i-- > 0;) {
                const auto dim = remaining[i];
                const auto low = std::min(dim.left, dim.right);
                const auto width = constant(
                    i64, static_cast<std::uint64_t>(std::max(dim.left, dim.right) - low + 1));
                const auto offset = emit(Op::Mod, i64, {address.op, width.op}, sl);
                indices[i] =
                    emit(Op::Add, i64,
                         {offset.op, constant(i64, static_cast<std::uint64_t>(low)).op}, sl)
                        .op;
                address = emit(Op::Div, i64, {address.op, width.op}, sl);
            }
            p.indices.insert(p.indices.end(), indices.begin(), indices.end());
        }
        p.elem.reset();
        places.push_back(std::move(p));
    }
    freadLeaves(places, desc, total, done, sl);
    emitVoid(Op::Fstore,
             {Operand::frame(addr),
              emit(Op::Add, i64,
                   {emit(Op::Fload, i64, {Operand::frame(addr)}, sl).op, constant(i64, 1).op}, sl)
                  .op},
             sl);
    emitVoid(Op::Fstore,
             {Operand::frame(left),
              emit(Op::Sub, i64,
                   {emit(Op::Fload, i64, {Operand::frame(left)}, sl).op, constant(i64, 1).op}, sl)
                  .op},
             sl);
    terminate(Op::Br, {Operand::block(head)}, sl);
    setBlock(done);
    return coerce(emit(Op::Fload, sir::s2(32), {Operand::frame(total)}, sl), rt, sl);
}

void Lowerer::freadPlaces(const Storage& s, const ast::Type& t, std::vector<Place>& out,
                          slang::SourceLocation sl) {
    // Members that are integral, each read as an integral variable; a
    // member array or real is not done yet.
    std::vector<Leaf> leaves;
    structLeaves(t, "", leaves);
    for (const Leaf& l : leaves)
        if (!l.dims.empty() || !l.type->getCanonicalType().isIntegral())
            unsupported(sl, "$fread into a structure with a member of type '" + l.type->toString() +
                                "'");
    storagePlaces(s, t, out, sl);
}

void Lowerer::freadLeaves(const std::vector<Place>& places, const Val& desc, sir::FrameVarId total,
                          sir::BlockId stop, slang::SourceLocation sl) {
    // Each place as $fread of an integral variable reads it: it starts with
    // its value, which the bytes not read keep, and one not read whole ends
    // the reading (SYSTEM_TASKS.md 6.3). The bytes read add up in total.
    for (const Place& p : places) {
        const sir::FrameVarId f = newFrameVar(p.type);
        emitVoid(Op::Fstore, {Operand::frame(f), loadPlace(p, sl).op}, sl);
        const Val n = emit(Op::Syscall, sir::s2(32),
                           {Operand::str("$fread"), Operand::frame(f), desc.op}, sl);
        emitVoid(Op::Fstore,
                 {Operand::frame(total),
                  emit(Op::Add, sir::s2(32),
                       {emit(Op::Fload, sir::s2(32), {Operand::frame(total)}, sl).op, n.op}, sl)
                      .op},
                 sl);
        const sir::BlockId store = newBlock();
        const sir::BlockId next = newBlock();
        terminate(Op::BrIf,
                  {emit(Op::Gt, sir::u2(1), {n.op, constant(sir::s2(32), 0).op}, sl).op,
                   Operand::block(store), Operand::block(stop)},
                  sl);
        setBlock(store);
        storePlace(p, emit(Op::Fload, p.type, {Operand::frame(f)}, sl), false, sl);
        const Val whole =
            emit(Op::Eq, sir::u2(1),
                 {n.op, constant(sir::s2(32), sir::integralBytes(p.type.width)).op}, sl);
        terminate(Op::BrIf, {whole.op, Operand::block(next), Operand::block(stop)}, sl);
        setBlock(next);
    }
}

}  // namespace kirimasim::front
