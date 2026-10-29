// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "lowerer.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "slang/ast/types/AllTypes.h"

namespace kirimasim::front {

Val Lowerer::lowerArrayQuery(const ast::Expression& queried, const ast::Expression* dimension,
                             std::string_view name, sir::Type result, slang::SourceLocation sl) {
    using sir::Op;
    using sir::Operand;

    // Count every declared array dimension, including a packed integral
    // leaf. Dynamic dimensions have no fixed range (20.7).
    std::vector<std::optional<slang::ConstantRange>> ranges;
    const ast::Type* type = &queried.type->getCanonicalType();
    std::uint32_t unpacked = 0;
    while (type->isArray()) {
        if (type->isUnpackedArray()) ++unpacked;
        ranges.push_back(type->hasFixedRange() ? std::optional{type->getFixedRange()}
                                               : std::nullopt);
        type = &type->getArrayElementType()->getCanonicalType();
    }
    if (type->isIntegral() && !type->isScalar())
        ranges.push_back(type->getFixedRange());
    else if (type->isString())
        ranges.push_back(std::nullopt);
    if (name == "$dimensions") return constant(result, ranges.size());
    if (name == "$unpacked_dimensions") return constant(result, unpacked);

    const auto fixed = [&](const slang::ConstantRange& range) {
        const std::int64_t value = name == "$left"             ? range.left
                                   : name == "$right"          ? range.right
                                   : name == "$low"            ? range.lower()
                                   : name == "$high"           ? range.upper()
                                   : name == "$size"           ? range.width()
                                   : range.left >= range.right ? 1
                                                               : -1;
        return constant(result, static_cast<std::uint64_t>(value));
    };
    const auto dynamic = [&] {
        // Match the front end's query evaluation: the dimension is evaluated
        // first. Only selecting dynamic dimension one evaluates the queried
        // expression. A fixed dimension or invalid index never does so.
        const auto value = lowerExpr(queried);
        const auto size = [&] {
            if (sir::isContainer(value.type))
                return emit(Op::ContainerSize, sir::u2(64), {value.op}, sl);
            if (value.type.kind == sir::TypeKind::String)
                return coerce(emit(Op::Builtin, sir::s2(32), {Operand::str("len"), value.op}, sl),
                              sir::u2(64), sl);
            unsupported(sl, "system function " + std::string(name) + " on this argument");
        }();
        if (name == "$size") return coerce(size, result, sl);
        if (name == "$left" || name == "$low") return constant(result, 0);
        if (name == "$increment") return constant(result, static_cast<std::uint64_t>(-1));
        const auto count = coerce(size, sir::s2(64), sl);
        return coerce(emit(Op::Sub, sir::s2(64), {count.op, constant(sir::s2(64), 1).op}, sl),
                      result, sl);
    };

    const auto known = dimension == nullptr ? std::optional<std::int64_t>{1} : constInt(*dimension);
    if (known) {
        if (dimension != nullptr) observeRuntimeConstant(*dimension);
        if (*known < 1 || static_cast<std::uint64_t>(*known) > ranges.size())
            return allXConstant(result);
        const auto& range = ranges[static_cast<std::size_t>(*known - 1)];
        if (range) return fixed(*range);
        if (*known != 1) unsupported(sl, "nested dynamic array query dimension");
        return dynamic();
    }

    // A dynamic subdimension needs a selected subarray rather than an
    // unspecified row. Preserve this existing rejection for runtime indices
    // too; constant invalid references are diagnosed by slang (20.7.1).
    if (ranges.size() > 1 &&
        std::any_of(ranges.begin() + 1, ranges.end(), [](const auto& range) { return !range; }))
        unsupported(sl, "runtime or nested dynamic array query dimension");

    auto index = lowerExpr(*dimension);
    // Preserve all selector bits and its signedness. Widen narrow selectors
    // so a declared dimension number cannot wrap into a different dimension.
    auto indexType = index.type;
    indexType.width = std::max(indexType.width, std::uint32_t{32});
    index = coerce(index, indexType, sl);
    const auto matches = [&](std::size_t i) {
        return emit(Op::Ceq, sir::u2(1), {index.op, constant(indexType, i + 1).op}, sl);
    };
    const auto fixedResult = [&] {
        auto selected = allXConstant(result);
        for (std::size_t i = 0; i < ranges.size(); ++i) {
            if (!ranges[i]) continue;
            const auto test = matches(i);
            selected = emit(Op::Select, result, {test.op, fixed(*ranges[i]).op, selected.op}, sl);
        }
        return selected;
    };
    if (ranges.empty() || ranges[0]) return fixedResult();

    const auto first = newBlock();
    const auto other = newBlock();
    const auto join = newBlock();
    const auto joined = blockParam(join, result);
    terminate(Op::BrIf, {matches(0).op, Operand::block(first), Operand::block(other)}, sl);
    setBlock(first);
    const auto current = dynamic();
    terminate(Op::Br, {Operand::block(join, 1), current.op}, sl);
    setBlock(other);
    const auto selected = fixedResult();
    terminate(Op::Br, {Operand::block(join, 1), selected.op}, sl);
    setBlock(join);
    return joined;
}

}  // namespace kirimasim::front
