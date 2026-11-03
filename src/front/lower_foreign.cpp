// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include <algorithm>
namespace kirimasim::front {
void Lowerer::buildForeignReflection() {
    std::map<std::uint32_t, const ast::Symbol*> scopes;
    for (const auto& [symbol, row] : dumpScopeRows_)
        if (!symbol->isValue()) scopes.emplace(row, symbol);
    for (std::uint32_t i = 0; i < design_.dumpScopes.size(); ++i) {
        const auto found = scopes.find(i);
        if (found == scopes.end()) continue;
        const auto& symbol = *found->second;
        const auto& scope = design_.dumpScopes[i];
        sir::ForeignObject row;
        row.name = scope.path;
        row.leafName = symbol.name;
        row.location = loc(symbol.location);
        if (scope.parent != sir::kNone) row.scope = design_.dumpScopes[scope.parent].path;
        if (symbol.kind == ast::SymbolKind::Instance)
            row.scale = timeScaleOf(symbol.as<ast::InstanceSymbol>().body);
        else if (symbol.isScope())
            row.scale = timeScaleOf(symbol.as<ast::Scope>());
        switch (scope.kind) {
            case sir::DumpScopeKind::Module: row.scopeKind = sir::ForeignScopeKind::Module; break;
            case sir::DumpScopeKind::Begin:
                row.scopeKind = symbol.kind == ast::SymbolKind::GenerateBlock
                                    ? sir::ForeignScopeKind::Generate
                                    : sir::ForeignScopeKind::Begin;
                break;
            case sir::DumpScopeKind::Fork: row.scopeKind = sir::ForeignScopeKind::Fork; break;
            case sir::DumpScopeKind::Task: row.scopeKind = sir::ForeignScopeKind::Task; break;
            case sir::DumpScopeKind::Function:
                row.scopeKind = sir::ForeignScopeKind::Function;
                break;
        }
        if (std::none_of(design_.foreignObjects.begin(), design_.foreignObjects.end(),
                         [&](const auto& existing) { return existing.name == row.name; }))
            design_.foreignObjects.push_back(std::move(row));
    }

    std::map<std::string, const ast::Symbol*> symbols;
    for (const auto& [symbol, slot] : slots_) {
        (void)slot;
        if (symbol->isValue()) symbols.emplace(symbol->getHierarchicalPath(), symbol);
    }
    for (const auto& name : design_.names) {
        const auto found = symbols.find(name.path);
        if (found == symbols.end()) continue;
        const auto& symbol = found->second->as<ast::ValueSymbol>();
        const auto& slot = design_.slot(name.slot);
        if (!sir::isIntegral(slot.type) && slot.type.kind != sir::TypeKind::Real) continue;
        sir::ForeignObject row;
        row.name = name.path;
        row.leafName = symbol.name;
        row.scope = symbol.getParentScope()->asSymbol().getHierarchicalPath();
        row.slot = name.slot;
        row.location = loc(symbol.location);
        row.scale = timeScaleOf(*symbol.getParentScope());
        row.left = slot.type.kind == sir::TypeKind::Real ? 63 : slot.type.width - 1;
        const ast::Type* type = &symbol.getType().getCanonicalType();
        while (type->kind == ast::SymbolKind::FixedSizeUnpackedArrayType)
            type = &type->as<ast::FixedSizeUnpackedArrayType>().elementType.getCanonicalType();
        row.scalar = type->isScalar();
        if (type->kind == ast::SymbolKind::PackedArrayType) {
            const auto& array = type->as<ast::PackedArrayType>();
            row.left = array.range.left;
            row.right = array.range.right;
        }
        if (type->kind == ast::SymbolKind::PredefinedIntegerType) {
            const auto kind = type->as<ast::PredefinedIntegerType>().integerKind;
            row.integer = kind == ast::PredefinedIntegerType::Int ||
                          kind == ast::PredefinedIntegerType::Integer;
        }
        design_.foreignObjects.push_back(std::move(row));
    }
}
std::optional<Val> Lowerer::lowerForeignCall(const ast::CallExpression& call,
                                             slang::SourceLocation sl) {
    const auto name = call.getSubroutineName();
    const auto found = std::find_if(foreignRoutines_.begin(), foreignRoutines_.end(),
                                    [&](const auto& routine) { return routine.name == name; });
    if (found == foreignRoutines_.end()) return std::nullopt;
    if (b_.isSub && b_.subSymbol &&
        std::none_of(design_.foreignObjects.begin(), design_.foreignObjects.end(),
                     [&](const auto& object) { return object.name == b_.scope; })) {
        sir::ForeignObject scope;
        scope.name = b_.scope;
        scope.leafName = b_.subSymbol->name;
        scope.scope = b_.subSymbol->getParentScope()->asSymbol().getHierarchicalPath();
        scope.location = loc(b_.subSymbol->location);
        scope.scale = timeScale();
        scope.scopeKind = design_.subroutine(b_.sub).kind == sir::SubroutineKind::Task
                              ? sir::ForeignScopeKind::Task
                              : sir::ForeignScopeKind::Function;
        design_.foreignObjects.push_back(std::move(scope));
    }
    const auto id = static_cast<std::uint32_t>(found - foreignRoutines_.begin());
    std::vector<sir::Operand> operands{
        sir::Operand::str("$vpi.call"),
        constant(sir::u2(32), id).op,
        sir::Operand::str(std::string(name)),
        constant(sir::u2(32), foreignSites_++).op,
        sir::Operand::str(b_.scope),
        constant(sir::s2(32), static_cast<std::uint64_t>(timeScale().unit)).op,
        constant(sir::s2(32), static_cast<std::uint64_t>(timeScale().precision)).op};
    for (const auto* actual : call.arguments()) {
        const ast::Expression* expression = actual;
        while (expression->kind == ast::ExpressionKind::Conversion &&
               expression->as<ast::ConversionExpression>().isImplicit())
            expression = &expression->as<ast::ConversionExpression>().operand();
        const ast::Expression* root = expression;
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
        sir::Operand reference = sir::Operand::str("");
        Val offset = constant(sir::u4(64), 0);
        Val value;
        std::string spelling, kind = "value";
        if (symbol && (isVariable(*symbol) || symbol->kind == ast::SymbolKind::Net) &&
            !isAggregate(*expression->type) && !isDynamicContainer(*expression->type)) {
            const auto place = placeOf(*expression, sl);
            if (place.home == Place::Home::Heap || place.home == Place::Home::Object)
                unsupported(sl, "VPI binding to this storage");
            value = loadPlace(place, sl);
            const bool frame = place.kind == Place::Kind::Frame || place.kind == Place::Kind::Ref ||
                               place.home == Place::Home::Frame || place.home == Place::Home::Ref;
            if (place.leaf != 0 && frame)
                unsupported(sl, "VPI binding to a non-first aggregate ref leaf");
            std::vector<sir::Operand> target{frame ? sir::Operand::frame(place.frame)
                                                   : sir::Operand::slot(place.slot)};
            target.insert(target.end(), place.indices.begin(), place.indices.end());
            target.push_back(sir::Operand::str("foreign"));
            reference = emit(sir::Op::RefMake, sir::refType(), std::move(target), sl).op;
            if (place.kind == Place::Kind::Bits) {
                offset = constant(sir::u4(64), static_cast<std::uint64_t>(place.lsb));
                if (place.pos)
                    offset = emit(sir::Op::Add, sir::u4(64),
                                  {offset.op, coerce(*place.pos, sir::u4(64), sl).op}, sl);
            }
            if (!frame) {
                spelling = design_.slot(place.slot).name;
                kind = design_.slot(place.slot).kind == sir::SlotKind::Var ? "variable" : "net";
                noteRead(rangeOf(place));
                if (kind == "variable") noteWrite(rangeOf(place));
            } else {
                spelling = std::string(symbol->name);
                kind = "variable";
            }
        } else
            value = lowerExpr(*actual);
        if (!sir::isIntegral(value.type) && value.type.kind != sir::TypeKind::Real)
            unsupported(sl, "VPI argument of this type");
        operands.push_back(value.op);
        operands.push_back(std::move(reference));
        operands.push_back(offset.op);
        operands.push_back(sir::Operand::str(std::move(spelling)));
        operands.push_back(sir::Operand::str(std::move(kind)));
    }
    if (found->result.kind == sir::TypeKind::Void) {
        emitVoid(sir::Op::Syscall, std::move(operands), sl);
        return Val{{}, found->result};
    }
    return emit(sir::Op::Syscall, found->result, std::move(operands), sl);
}
}  // namespace kirimasim::front
