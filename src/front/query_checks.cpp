// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "query_checks.h"

#include <concepts>
#include <set>
#include <string>
#include <variant>

#include "slang/ast/ASTVisitor.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/ast/types/DeclaredType.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxVisitor.h"
#include "slang/text/SourceManager.h"

namespace kirimasim::front {

namespace {

namespace ast = slang::ast;
namespace syntax = slang::syntax;

ast::ASTContext declarationContext(const ast::Symbol& symbol, const ast::DeclaredType& declared,
                                   bool initializer) {
    const auto* scope = symbol.getParentScope();
    auto lookup =
        initializer && !declared.getFlags().has(ast::DeclaredTypeFlags::InitializerCantSeeParent)
            ? ast::LookupLocation::after(symbol)
            : ast::LookupLocation::before(symbol);
    slang::bitmask<ast::ASTFlags> flags;
    const auto override = initializer ? ast::DeclaredTypeFlags::InitializerOverridden
                                      : ast::DeclaredTypeFlags::TypeOverridden;
    if (declared.getFlags().has(override) &&
        scope->asSymbol().kind == ast::SymbolKind::InstanceBody) {
        const auto& body = scope->asSymbol().as<ast::InstanceBodySymbol>();
        if (const auto* instance = body.parentInstance;
            instance != nullptr && instance->getParentScope() != nullptr) {
            scope = instance->getParentScope();
            lookup = ast::LookupLocation::before(*instance);
            if (body.flags.has(ast::InstanceFlags::FromBind))
                flags |= ast::ASTFlags::BindInstantiation;
        }
    }
    return ast::ASTContext(*scope, lookup, flags);
}

bool dynamicallySized(const ast::Type& type, std::set<const ast::Type*>& path) {
    const auto& canonical = type.getCanonicalType();
    if (canonical.isString() || canonical.isDynamicallySizedArray()) return true;
    if (canonical.kind == ast::SymbolKind::FixedSizeUnpackedArrayType)
        return dynamicallySized(canonical.as<ast::FixedSizeUnpackedArrayType>().elementType, path);
    if (canonical.kind == ast::SymbolKind::UnpackedStructType) {
        for (const auto* field : canonical.as<ast::UnpackedStructType>().fields)
            if (dynamicallySized(field->getType(), path)) return true;
    } else if (canonical.kind == ast::SymbolKind::UnpackedUnionType) {
        for (const auto* field : canonical.as<ast::UnpackedUnionType>().fields)
            if (dynamicallySized(field->getType(), path)) return true;
    } else if (canonical.kind == ast::SymbolKind::ClassType) {
        const auto& classType = canonical.as<ast::ClassType>();
        if (classType.isInterface || !path.insert(&canonical).second) return false;
        bool dynamic = false;
        for (const auto& property : classType.properties()) {
            if (dynamicallySized(property.getType(), path)) {
                dynamic = true;
                break;
            }
        }
        path.erase(&canonical);
        return dynamic;
    }
    return false;
}

bool dynamicallySized(const ast::Type& type) {
    std::set<const ast::Type*> path;
    return dynamicallySized(type, path);
}

class DynamicReturnFinder
    : public ast::ASTVisitor<DynamicReturnFinder, ast::VisitFlags::Expressions> {
public:
    bool found = false;

    void handle(const ast::CallExpression& call) {
        if (call.getSubroutineKind() == ast::SubroutineKind::Function &&
            dynamicallySized(*call.type)) {
            found = true;
            return;
        }
        visitDefault(call);
    }
};

// Resolved types keep their dimension values, but not the expressions that
// produced them. Bind only a query's argument to recover its types; binding
// the full query could ask the pinned front end to evaluate that argument.
class DimensionQueryVisitor : public syntax::SyntaxVisitor<DimensionQueryVisitor> {
public:
    DimensionQueryVisitor(const ast::ASTContext& context, std::set<slang::SourceLocation>& errors)
        : context_(context), errors_(errors) {}

    void handle(const syntax::InvocationExpressionSyntax& call) {
        if (call.left->kind == syntax::SyntaxKind::SystemName &&
            call.left->as<syntax::SystemNameSyntax>().systemIdentifier.valueText() == "$bits" &&
            call.arguments != nullptr && call.arguments->parameters.size() == 1 &&
            call.arguments->parameters[0]->kind == syntax::SyntaxKind::OrderedArgument) {
            const auto& argument =
                call.arguments->parameters[0]->as<syntax::OrderedArgumentSyntax>();
            if (const auto* expression = context_.requireSimpleExpr(*argument.expr)) {
                const auto& bound =
                    ast::Expression::bind(*expression, context_, ast::ASTFlags::AllowDataType);
                DynamicReturnFinder finder;
                bound.visit(finder);
                if (finder.found) errors_.insert(call.sourceRange().start());
            }
        }
        visitDefault(call);
    }

private:
    const ast::ASTContext& context_;
    std::set<slang::SourceLocation>& errors_;
};

class QueryVisitor : public ast::ASTVisitor<QueryVisitor, ast::VisitFlags::AllGood> {
public:
    std::set<slang::SourceLocation> errors;

    template <std::derived_from<ast::Symbol> Symbol> void handle(const Symbol& symbol) {
        const auto& base = static_cast<const ast::Symbol&>(symbol);
        const auto* declared = base.getDeclaredType();
        if (declared != nullptr && symbol.getParentScope() != nullptr) {
            const auto context = declarationContext(base, *declared, false);
            DimensionQueryVisitor dimensions(context, errors);
            if (const auto* type = declared->getTypeSyntax()) type->visit(dimensions);
            if (const auto* unpacked = declared->getDimensionSyntax())
                for (const auto* dimension : *unpacked)
                    dimension->visit(dimensions);
        }
        const auto* scope = base.isScope() ? &base.as<ast::Scope>() : base.getParentScope();
        if (scope != nullptr) {
            const auto context = declared != nullptr && !base.isScope()
                                     ? declarationContext(base, *declared, true)
                                     : ast::ASTContext(*scope, ast::LookupLocation::max);
            visitInContext(symbol, context);
        } else {
            visitDefault(symbol);
        }
    }

    void handle(const ast::VariableDeclStatement& statement) { statement.symbol.visit(*this); }

    void handle(const ast::BlockStatement& statement) {
        if (statement.blockSymbol != nullptr) {
            const ast::ASTContext context(*statement.blockSymbol, ast::LookupLocation::max);
            visitInContext(statement, context);
        } else {
            visitDefault(statement);
        }
    }

    void handle(const ast::ForLoopStatement& statement) {
        for (const auto* variable : statement.loopVars)
            variable->visit(*this);
        if (!statement.loopVars.empty() &&
            statement.loopVars.front()->getParentScope() != nullptr) {
            const ast::ASTContext context(*statement.loopVars.front()->getParentScope(),
                                          ast::LookupLocation::max);
            visitInContext(statement, context);
        } else {
            visitDefault(statement);
        }
    }

    void handle(const ast::DefParamSymbol& parameter) { parameter.getInitializer().visit(*this); }

    void handle(const ast::GenerateBlockSymbol& block) {
        if (const auto* condition = block.getConditionExpression()) condition->visit(*this);
        for (const auto* item : block.caseItemExpressions)
            item->visit(*this);
        if (!block.isUninstantiated) {
            const ast::ASTContext context(block, ast::LookupLocation::max);
            visitInContext(block, context);
        }
    }

    void handle(const ast::GenerateBlockArraySymbol& block) {
        if (block.initialExpression != nullptr) block.initialExpression->visit(*this);
        if (block.loopVariable != nullptr && block.loopVariable->getParentScope() != nullptr) {
            const ast::ASTContext context(*block.loopVariable->getParentScope(),
                                          ast::LookupLocation::max);
            visitExpression(block.stopExpression, context);
            visitExpression(block.iterExpression, context);
        } else {
            if (block.stopExpression != nullptr) block.stopExpression->visit(*this);
            if (block.iterExpression != nullptr) block.iterExpression->visit(*this);
        }
        visitDefault(block);
    }

    void handle(const ast::CallExpression& call) {
        if (call.isSystemCall() && call.getSubroutineName() == "$bits") {
            DynamicReturnFinder finder;
            for (const auto* argument : call.arguments())
                argument->visit(finder);
            if (finder.found) errors.insert(call.sourceRange.start());
        }
        if (call.isSystemCall()) {
            const auto [expression, variable] = std::get<1>(call.subroutine).getIteratorInfo();
            if (expression != nullptr && variable != nullptr &&
                variable->getParentScope() != nullptr) {
                if (call.thisClass() != nullptr) call.thisClass()->visit(*this);
                auto context = context_ != nullptr ? *context_
                                                   : ast::ASTContext(*variable->getParentScope(),
                                                                     ast::LookupLocation::max);
                context.firstTempVar = &variable->as<ast::TempVarSymbol>();
                context.flags &= ~ast::ASTFlags::StaticInitializer;
                visitExpression(expression, context);
                for (const auto* argument : call.arguments())
                    argument->visit(*this);
                return;
            }
        }
        // The declaration's original AST is retained even when its value
        // was folded. Also check nested queries and unused subroutine bodies.
        visitDefault(call);
    }

    void handle(const ast::ConversionExpression& expression) {
        if (context_ != nullptr && expression.syntax != nullptr &&
            expression.syntax->kind == syntax::SyntaxKind::CastExpression) {
            DimensionQueryVisitor dimensions(*context_, errors);
            expression.syntax->as<syntax::CastExpressionSyntax>().left->visit(dimensions);
        }
        visitDefault(expression);
    }

private:
    const ast::ASTContext* context_ = nullptr;

    void visitExpression(const ast::Expression* expression, const ast::ASTContext& context) {
        if (expression == nullptr) return;
        const auto* saved = context_;
        context_ = &context;
        expression->visit(*this);
        context_ = saved;
    }

    template <typename Node> void visitInContext(const Node& node, const ast::ASTContext& context) {
        const auto* saved = context_;
        context_ = &context;
        visitDefault(node);
        context_ = saved;
    }
};

}  // namespace

QueryCheckResult checkQueryExpressions(ast::Compilation& compilation) {
    QueryVisitor visitor;
    compilation.getRoot().visit(visitor);
    QueryCheckResult result;
    result.numErrors = visitor.errors.size();
    const auto& sourceManager = *compilation.getSourceManager();
    for (const auto at : visitor.errors) {
        result.diagnostics += std::string(sourceManager.getFileName(at)) + ":" +
                              std::to_string(sourceManager.getLineNumber(at)) + ":" +
                              std::to_string(sourceManager.getColumnNumber(at)) +
                              ": error: $bits cannot use a function call with a dynamically sized "
                              "return type (IEEE 1800-2023 20.6.2)\n";
    }
    return result;
}

}  // namespace kirimasim::front
