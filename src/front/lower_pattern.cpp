// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Source-selected presentation metadata and argument-time snapshots. The
// shared renderer owns all output spelling (SYSTEM_TASKS 3.9).
#include "lowerer.h"
#include "slang/syntax/SyntaxNode.h"
#include <cctype>
#include <limits>
#include <utility>

namespace kirimasim::front {
namespace {
bool escapedName(const ast::Symbol& symbol) {
    if (const auto* syntax = symbol.getSyntax()) {
        const auto token = syntax->getFirstToken().rawText();
        if (!token.empty() && token.front() == '\\') return true;
    }
    const auto name = symbol.name;
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name.front())) && name.front() != '_') return true;
    for (const unsigned char c : name)
        if (!std::isalnum(c) && c != '_' && c != '$') return true;
    return false;
}
}  // namespace

sir::PatternId Lowerer::patternLayout(const ast::Type& type, slang::SourceLocation sl) {
    checkStack(sl);
    const auto& t = type.getCanonicalType();
    if (const auto found = patternLayouts_.find(&t); found != patternLayouts_.end())
        return found->second;
    using Kind = sir::PatternLayout::Kind;
    sir::PatternLayout row;
    if (t.kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& array = t.as<ast::FixedSizeUnpackedArrayType>();
        row.kind = Kind::Array;
        row.count = array.range.width();
        row.element = patternLayout(array.elementType, sl);
    } else if (t.kind == ast::SymbolKind::UnpackedStructType) {
        row.kind = Kind::Record;
        for (const auto* field : t.as<ast::UnpackedStructType>().fields)
            row.members.push_back({std::string(field->name), patternLayout(field->getType(), sl),
                                   escapedName(*field)});
    } else if (t.kind == ast::SymbolKind::PackedStructType || t.isPackedUnion()) {
        const bool isUnion = t.isPackedUnion();
        if (isUnion && t.as<ast::PackedUnionType>().isTagged)
            unsupported(sl, "assignment-pattern formatting of a tagged union");
        row.kind = isUnion ? Kind::PackedUnion : Kind::PackedRecord;
        row.value = typeOf(t, sl);
        const auto fields = [&](const auto& scope) {
            for (const auto& field : scope.template membersOfType<ast::FieldSymbol>()) {
                if (field.bitOffset > std::numeric_limits<std::uint32_t>::max())
                    unsupported(sl, "assignment-pattern field offset of this size");
                row.members.push_back({std::string(field.name), patternLayout(field.getType(), sl),
                                       escapedName(field),
                                       isUnion ? 0 : static_cast<std::uint32_t>(field.bitOffset)});
                if (isUnion) break;
            }
        };
        if (isUnion)
            fields(t.as<ast::PackedUnionType>());
        else
            fields(t.as<ast::PackedStructType>());
    } else if (isDynamicContainer(t)) {
        row.kind = Kind::Container;
        row.value = typeOf(t, sl);
        const auto& element = t.isQueue() ? t.as<ast::QueueType>().elementType
                                          : t.as<ast::DynamicArrayType>().elementType;
        row.element = patternLayout(element, sl);
    } else {
        row.value = typeOf(t, sl);
        if (t.isEnum()) {
            row.kind = Kind::Enumeration;
            for (const auto& item : t.as<ast::EnumType>().values())
                row.enumerators.push_back(
                    {std::string(item.name),
                     constantFrom(item.getValue(), row.value, sl).op.asConst(), escapedName(item)});
        } else if (t.isString()) {
            row.kind = Kind::String;
        } else if (t.isFloating()) {
            row.kind = Kind::Real;
        } else if (t.isIntegral()) {
            row.kind = Kind::Integral;
        } else {
            unsupported(sl, "assignment-pattern formatting of this type");
        }
    }
    const sir::PatternId id{static_cast<std::uint32_t>(design_.patterns.size())};
    design_.patterns.push_back(std::move(row));
    patternLayouts_.emplace(&t, id);
    return id;
}

Lowerer::PatternArgument Lowerer::capturePatternArgument(const ast::Expression& expression,
                                                         bool strength) {
    const auto sl = expression.sourceRange.start();
    PatternArgument argument;
    if (isAggregate(*expression.type)) {
        argument.layout = patternLayout(*expression.type, sl);
        aggregateValues(expression, *expression.type, argument.values, sl);
    } else {
        const auto captured = strength ? captureStrengthArgument(expression)
                                       : StrengthArgument{lowerExpr(expression), {}};
        Val value = captured.value;
        argument.strength = captured.strength;
        if (sir::isIntegral(value.type) || value.type.kind == sir::TypeKind::String ||
            value.type.kind == sir::TypeKind::Real || value.type.kind == sir::TypeKind::ShortReal)
            argument.scalar = value;
        if (sir::isIntegral(value.type) && expression.isImplicitString()) {
            // Literal-derived expressions have a string presentation for
            // %p. Keep their original packed value for other conversions
            // and monitor comparisons; perform the existing SV string cast.
            value = coerce(value, sir::Type{sir::TypeKind::String, 0}, sl);
            sir::PatternLayout row;
            row.kind = sir::PatternLayout::Kind::String;
            row.value = value.type;
            argument.layout = sir::PatternId{static_cast<std::uint32_t>(design_.patterns.size())};
            design_.patterns.push_back(std::move(row));
        } else {
            argument.layout = patternLayout(*expression.type, sl);
        }
        argument.values.push_back(value);
    }
    return argument;
}

Val Lowerer::formatPatternArgument(const PatternArgument& argument, bool positional,
                                   slang::SourceLocation sl) {
    std::vector<sir::Operand> operands{sir::Operand::pattern(argument.layout),
                                       sir::Operand::str(positional ? "positional" : "named")};
    for (const auto& value : argument.values)
        operands.push_back(value.op);
    return emit(sir::Op::FormatPattern, sir::Type{sir::TypeKind::String, 0}, std::move(operands),
                sl);
}
void Lowerer::lowerMonitor(std::span<const ast::Expression* const> args, char radix,
                           slang::SourceLocation sl) {
    using sir::Op;
    using sir::Operand;
    std::vector<Operand> operands{
        Operand::str(radix == 'd' ? "$display" : "$display" + std::string(1, radix))};
    std::vector<Val> observed;
    lowerFormatArgs(args, operands, sl, radix, {}, &observed);
    std::optional<Val> changed;
    std::vector<std::pair<sir::FrameVarId, Val>> shown;
    for (const auto& value : observed) {
        const auto previous = newFrameVar(value.type);
        const auto last = emit(Op::Fload, value.type, {Operand::frame(previous)}, sl);
        const auto same = definitelyEqualValues(value, last, sl, true);
        const auto different = emit(Op::Lnot, sir::u2(1), {same.op}, sl);
        changed = changed ? bitOp(Op::Or, *changed, different, sl) : different;
        shown.emplace_back(previous, value);
    }
    const auto seen = newFrameVar(sir::u2(1));
    auto show = emit(Op::Lnot, sir::u2(1),
                     {emit(Op::Fload, sir::u2(1), {Operand::frame(seen)}, sl).op}, sl);
    if (changed) show = bitOp(Op::Or, show, *changed, sl);
    const auto yes = newBlock(), done = newBlock();
    terminate(Op::BrIf, {show.op, Operand::block(yes), Operand::block(done)}, sl);
    setBlock(yes);
    for (const auto& [previous, value] : shown)
        emitVoid(Op::Fstore, {Operand::frame(previous), value.op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(seen), constant(sir::u2(1), 1).op}, sl);
    emitVoid(Op::Syscall, std::move(operands), sl);
    terminate(Op::Br, {Operand::block(done)}, sl);
    setBlock(done);
}
}  // namespace kirimasim::front
