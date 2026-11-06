// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of streaming operators and concatenation targets to SIR.

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

Val Lowerer::lowerStream(const ast::StreamingConcatenationExpression& s, slang::SourceLocation sl) {
    // The bits of the operands from the left (11.4.14.1); four-state if any
    // operand is. Then, for <<, the blocks of the slice size from the right
    // in reverse order (11.4.14.2).
    std::vector<Operand> ops;
    std::uint32_t width = 0;
    bool four = false;
    for (const auto& se : s.streams()) {
        if (se.withExpr != nullptr) unsupported(sl, "streaming with a with clause");
        const ast::Expression& op = *se.operand;
        Val v;
        if (op.kind == ast::ExpressionKind::Streaming)
            v = lowerStream(op.as<ast::StreamingConcatenationExpression>(), sl);
        else
            v = bitstreamOf(op, sl);
        ops.push_back(v.op);
        width += v.type.width;
        four = four || sir::isFourState(v.type);
    }
    if (width == 0) unsupported(sl, "a value of no bits");
    Val stream = emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
    if (s.getSliceSize() == 0) return stream;
    return reverseBlocks(stream, s.getSliceSize(), true, sl);
}

Val Lowerer::reverseBlocks(const Val& v, std::uint64_t size, bool fromRight,
                           slang::SourceLocation sl) {
    // Blocks of `size` bits, sliced from the right end (packing) or from
    // the left end (unpacking, its inverse); the last block may be short.
    // The result has them in the reverse order.
    const std::uint32_t w = v.type.width;
    if (size >= w) return v;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> blocks;  // lsb, width
    if (fromRight) {
        for (std::uint32_t lsb = 0; lsb < w; lsb += static_cast<std::uint32_t>(size))
            blocks.emplace_back(lsb,
                                static_cast<std::uint32_t>(std::min<std::uint64_t>(size, w - lsb)));
    } else {
        for (std::uint32_t msb = w; msb > 0;) {
            const auto bw = static_cast<std::uint32_t>(std::min<std::uint64_t>(size, msb));
            msb -= bw;
            blocks.emplace_back(msb, bw);
        }
        std::reverse(blocks.begin(), blocks.end());
    }
    // From the right: the lowest block becomes the most significant; from
    // the left, the reversed list puts the last block first.
    std::vector<Operand> parts;
    parts.reserve(blocks.size());
    for (const auto& [lsb, bw] : blocks)
        parts.push_back(emit(Op::Partsel, integral(sir::isFourState(v.type), false, bw),
                             {v.op, constant(sir::u2(32), lsb).op}, sl)
                            .op);
    return emit(Op::Concat, integral(sir::isFourState(v.type), false, w), std::move(parts), sl);
}

void Lowerer::lowerUnpack(const ast::StreamingConcatenationExpression& s,
                          const ast::Expression& rhs, bool nonblocking, slang::SourceLocation sl,
                          const ast::Expression* delay, const ast::TimingControl* event) {
    // The source's leftmost bits, as many as the targets take; for << the
    // inverse reordering; then the targets from the left like a
    // concatenation (11.4.14.3). The value comes before the targets'
    // indices, as in any assignment.
    Val src = bitstreamOf(rhs, sl);
    const std::optional<Val> ticks =
        delay ? std::optional<Val>(delayValue(*delay, sl)) : std::nullopt;
    const std::uint64_t w = s.getBitstreamWidth();
    if (w == 0 || w > UINT32_MAX || src.type.width < w) unsupported(sl, "unpacking this stream");
    if (src.type.width > w)
        src = emit(Op::Partsel,
                   integral(sir::isFourState(src.type), false, static_cast<std::uint32_t>(w)),
                   {src.op, constant(sir::u2(32), src.type.width - w).op}, sl);
    else
        src = coerce(src, integral(sir::isFourState(src.type), false, src.type.width), sl);
    // <<: the inverse of packing, also with a short last block
    // (VALUE_SEMANTICS.md 8).
    if (s.getSliceSize() != 0) src = reverseBlocks(src, s.getSliceSize(), false, sl);
    const auto count = event ? std::optional{eventNbaCount(*event, sl)} : std::nullopt;
    EventNbaCapture capture;
    std::vector<Place> places;
    std::vector<Val> parts;
    unpackParts(s, src, places, parts, sl);
    if (parts.size() > 1) {
        // A stream inside the target reordered its own share: the pieces,
        // leftmost first, make the value of the targets.
        std::vector<Operand> ops;
        ops.reserve(parts.size());
        for (const Val& part : parts)
            ops.push_back(part.op);
        src = emit(Op::Concat, src.type, std::move(ops), sl);
    } else {
        src = parts.front();
    }
    Place all = concatPlace(std::move(places), src.type, sl);
    storePlace(all, src, nonblocking, sl, ticks, event ? &capture : nullptr);
    if (event != nullptr && count.has_value()) finishEventNba(capture, *event, *count, sl);
}

void Lowerer::unpackParts(const ast::StreamingConcatenationExpression& s, const Val& src,
                          std::vector<Place>& places, std::vector<Val>& parts,
                          slang::SourceLocation sl) {
    // The targets of s from the left, given src, already reordered by s.
    // A stream among them takes its share of the bits, which it reorders
    // itself and gives to its own targets: the inverse of packing, level
    // by level (11.4.14.3, VALUE_SEMANTICS.md 8).
    const bool nested = std::any_of(s.streams().begin(), s.streams().end(), [](const auto& se) {
        return se.operand->kind == ast::ExpressionKind::Streaming;
    });
    if (!nested) {
        // The whole value, to the targets as one concatenation.
        for (const auto& se : s.streams()) {
            if (se.withExpr != nullptr) unsupported(sl, "streaming with a with clause");
            bitstreamPlaces(*se.operand, places, sl);
        }
        parts.push_back(src);
        return;
    }
    std::uint32_t taken = 0;
    for (const auto& se : s.streams()) {
        if (se.withExpr != nullptr) unsupported(sl, "streaming with a with clause");
        const ast::Expression& op = *se.operand;
        const std::size_t first = places.size();
        std::uint32_t width = 0;
        if (op.kind == ast::ExpressionKind::Streaming) {
            const auto& inner = op.as<ast::StreamingConcatenationExpression>();
            width = static_cast<std::uint32_t>(inner.getBitstreamWidth());
        } else {
            bitstreamPlaces(op, places, sl);
            for (std::size_t k = first; k < places.size(); ++k)
                width += places[k].type.width;
        }
        Val share = emit(Op::Partsel, integral(sir::isFourState(src.type), false, width),
                         {src.op, constant(sir::u2(32), src.type.width - taken - width).op}, sl);
        taken += width;
        if (op.kind != ast::ExpressionKind::Streaming) {
            parts.push_back(share);
            continue;
        }
        const auto& inner = op.as<ast::StreamingConcatenationExpression>();
        if (inner.getSliceSize() != 0)
            share = reverseBlocks(share, inner.getSliceSize(), false, sl);
        unpackParts(inner, share, places, parts, sl);
    }
}

Val Lowerer::bitstreamOf(const ast::Expression& e, slang::SourceLocation sl) {
    // An integral value as its bits; an unpacked array or structure as the
    // concatenation of its single values in order (11.4.14.1,
    // VALUE_SEMANTICS.md 8).
    std::vector<Val> vals;
    if (isAggregate(*e.type))
        aggregateValues(e, *e.type, vals, sl);
    else
        vals.push_back(lowerExpr(e));
    std::vector<Operand> ops;
    std::uint32_t width = 0;
    bool four = false;
    for (Val& v : vals) {
        if (!sir::isIntegral(v.type)) unsupported(sl, "streaming of a value of this type");
        v = coerce(v, integral(sir::isFourState(v.type), false, v.type.width), sl);
        ops.push_back(v.op);
        width += v.type.width;
        four = four || sir::isFourState(v.type);
    }
    if (width == 0) unsupported(sl, "a value of no bits");
    if (ops.size() == 1) return vals[0];
    return emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
}

void Lowerer::bitstreamPlaces(const ast::Expression& e, std::vector<Place>& out,
                              slang::SourceLocation sl) {
    // The single places of a target, in the order of bitstreamOf.
    if (structElement(*e.type) != nullptr) {
        const std::optional<Storage> st = storageOf(e, sl);
        if (!st) unsupported(sl, "unpacking into this target");
        storagePlaces(*st, *e.type, out, sl);
        return;
    }
    if (e.type->getCanonicalType().isUnpackedArray()) {
        const std::optional<ArrayTarget> a = arrayTarget(e);
        if (!a) unsupported(sl, "unpacking into this array");
        arrayPlaces(baseOf(*a), a->indices, out);
        return;
    }
    if (!e.type->isIntegral()) unsupported(sl, "unpacking into this target");
    out.push_back(placeOf(e, sl));
}

void Lowerer::arrayPlaces(ArrayBase s, std::vector<std::int64_t> indices, std::vector<Place>& out) {
    const std::vector<sir::ArrayDim>& dims = dimsOf(s);
    if (indices.size() == dims.size()) {
        out.push_back(elementPlace(s, indices));
        return;
    }
    const sir::ArrayDim d = dims[indices.size()];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    for (std::int64_t i = d.left;; i += step) {
        indices.push_back(i);
        arrayPlaces(s, indices, out);
        indices.pop_back();
        if (i == d.right) break;
    }
}

Lowerer::Place Lowerer::concatPlace(std::vector<Place> parts, Type like, slang::SourceLocation sl) {
    Place all;
    all.kind = Place::Kind::Concat;
    for (Place& part : parts) {
        if (!sir::isIntegral(part.type)) unsupported(sl, "unpacking into a target of this type");
        all.width += part.type.width;
        all.base.push_back(std::move(part));
    }
    if (all.width == 0) unsupported(sl, "a value of no bits");
    all.type = integral(sir::isFourState(like), false, all.width);
    return all;
}

}  // namespace kirimasim::front
