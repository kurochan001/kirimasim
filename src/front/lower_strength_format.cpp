// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/SelectExpressions.h"

namespace kirimasim::front {
Lowerer::StrengthArgument Lowerer::captureStrengthArgument(const ast::Expression& expression) {
    const auto sl = expression.sourceRange.start();
    const ast::Expression* input = &expression;
    while (input->kind == ast::ExpressionKind::Conversion) {
        const auto& cast = input->as<ast::ConversionExpression>();
        const auto& from = *cast.operand().type;
        const auto& to = *cast.type;
        if (!cast.isImplicit() || !from.isIntegral() || !to.isIntegral() ||
            from.getBitWidth() != to.getBitWidth() || from.isSigned() != to.isSigned() ||
            from.isFourState() != to.isFourState())
            break;
        input = &cast.operand();
    }
    const auto* root = input;
    for (;;) {
        if (root->kind == ast::ExpressionKind::ElementSelect)
            root = &root->as<ast::ElementSelectExpression>().value();
        else if (root->kind == ast::ExpressionKind::RangeSelect)
            root = &root->as<ast::RangeSelectExpression>().value();
        else if (root->kind == ast::ExpressionKind::MemberAccess)
            root = &root->as<ast::MemberAccessExpression>().value();
        else
            break;
    }
    const auto* symbol = namedSymbol(*root);
    if (!symbol || symbol->kind != ast::SymbolKind::Net || !input->type->isIntegral())
        return {lowerExpr(expression), {}};
    const auto place = placeOf(*input, sl);
    if (place.home != Place::Home::Slot || !sir::isFourState(place.type))
        unsupported(sl, "strength formatting of this net domain");
    const auto value = loadPlace(place, sl);
    std::vector<sir::Operand> operands{
        sir::Operand::slot(place.slot, place.windowLsb, place.windowWidth)};
    operands.insert(operands.end(), place.indices.begin(), place.indices.end());
    const auto position =
        place.kind == Place::Kind::Bits
            ? (place.pos ? *place.pos
                         : constant(sir::s2(64), static_cast<std::uint64_t>(place.lsb)))
            : constant(sir::s2(64), 0);
    operands.push_back(position.op);
    const auto physical = sir::physicalSlot(design_, place.slot);
    strengthNets_.try_emplace(physical, loc(sl));
    return {value, emit(sir::Op::StrengthLoad, sir::strengthType(value.type.width),
                        std::move(operands), sl)};
}
Val Lowerer::strengthPresentation(const StrengthArgument& argument, slang::SourceLocation sl) {
    Val strength;
    if (argument.strength)
        strength = *argument.strength;
    else {
        auto value = argument.value;
        if (value.type.kind == sir::TypeKind::Real || value.type.kind == sir::TypeKind::ShortReal)
            value = coerce(value, sir::s4(64), sl);
        if (!sir::isIntegral(value.type)) unsupported(sl, "strength format of this type");
        // strength.make takes a 4-state value; a 2-state one has the same
        // bits (SYSTEM_TASKS.md 3.10: any integral value is shown strong).
        if (!sir::isFourState(value.type))
            value = coerce(value,
                           sir::isSigned(value.type) ? sir::s4(value.type.width)
                                                     : sir::u4(value.type.width),
                           sl);
        strength = emit(sir::Op::StrengthMake, sir::strengthType(value.type.width),
                        {value.op, constant(sir::u2(3), 6).op, constant(sir::u2(3), 6).op}, sl);
    }
    return emit(sir::Op::Syscall, sir::Type{sir::TypeKind::String, 0},
                {sir::Operand::str("$strength.format"), strength.op}, sl);
}
}  // namespace kirimasim::front
