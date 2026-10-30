// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lexical storage ownership and immutable fork imports (SIR_SPEC 7/8;
// approved automatic-capture contract 1-4/6-8, IEEE 1800-2023 6.21/9.3.2).
#include "lowerer.h"

#include <algorithm>
#include <set>
#include <utility>

#include "slang/ast/ASTVisitor.h"

namespace kirimasim::front {

sir::ActivationInfo& Lowerer::activationInfo() {
    return b_.isSub ? design_.subroutine(b_.sub).activation : proc().activation;
}
void Lowerer::initLexical(bool automatic) {
    b_.lexical.push_back({0, automatic, automatic});
    b_.lexicalStack.push_back(0);
}
void Lowerer::ensureLexical() {
    if (!b_.lexical.empty()) return;
    if (b_.isSub && sir::readsWaiterFrame(design_.subroutine(b_.sub).kind)) return;
    initLexical(b_.isSub && design_.subroutine(b_.sub).automatic);
}
void Lowerer::pushLexical(bool automatic, std::optional<std::uint32_t> control,
                          slang::SourceLocation sl) {
    ensureLexical();
    if (b_.lexicalStack.empty()) return;
    const auto index = b_.lexical.size();
    b_.lexical.push_back({b_.lexicalStack.back(), automatic, automatic, sir::kNone, control});
    b_.lexicalStack.push_back(index);
    b_.lexicalEdits.push_back({b_.block, block(b_.block).instrs.size(), index, true, loc(sl)});
}
void Lowerer::popLexical(slang::SourceLocation sl) {
    if (b_.lexicalStack.size() <= 1) return;
    if (!b_.dead)
        b_.lexicalEdits.push_back(
            {b_.block, block(b_.block).instrs.size(), b_.lexicalStack.back(), false, loc(sl)});
    b_.lexicalStack.pop_back();
}
void Lowerer::ownFrame(const ast::VariableSymbol& symbol, sir::FrameVarId first,
                       std::uint32_t count) {
    ensureLexical();
    if (b_.lexicalStack.empty()) return;
    auto& lexical = b_.lexical[b_.lexicalStack.back()];
    lexical.automatic = lexical.automatic || symbol.lifetime == ast::VariableLifetime::Automatic;
    for (std::uint32_t i = 0; i < count; ++i) {
        const sir::FrameVarId field{first.index() + i};
        lexical.fields.push_back(field);
        auto& row = frameVar(field);
        if (lexical.scope != sir::kNone && row.type.kind != sir::TypeKind::Ref) {
            row.home = sir::FrameHome::Scoped;
            row.activationScope = lexical.scope;
            activationInfo().scopes[lexical.scope.index()].fields.push_back(field);
        }
    }
}
void Lowerer::promoteLexical() {
    ensureLexical();
    sir::ActivationScopeId parent = sir::kNone;
    for (const auto index : b_.lexicalStack) {
        auto& lexical = b_.lexical[index];
        if (!lexical.automatic) continue;
        if (lexical.scope == sir::kNone) {
            lexical.scope =
                sir::ActivationScopeId{static_cast<std::uint32_t>(activationInfo().scopes.size())};
            activationInfo().scopes.push_back({parent, {}});
            if (index == 0) activationInfo().root = lexical.scope;
            for (const auto field : lexical.fields) {
                auto& row = frameVar(field);
                if (row.type.kind == sir::TypeKind::Ref) continue;
                row.home = sir::FrameHome::Scoped;
                row.activationScope = lexical.scope;
                activationInfo().scopes[lexical.scope.index()].fields.push_back(field);
            }
        }
        parent = lexical.scope;
    }
    qualifyReferenceBindings();
}
void Lowerer::qualifyReferenceBindings() {
    // Formal qualifiers belong to the binding, not to inferred actual lifetime.
    // Restrict annotation here so bodies without an extended environment keep
    // their existing text form while source capture is introduced.
    const auto qualify = [&](const ast::Symbol& symbol, sir::FrameVarId field) {
        auto& row = frameVar(field);
        if (row.type.kind != sir::TypeKind::Ref || row.home == sir::FrameHome::Capture ||
            symbol.kind != ast::SymbolKind::FormalArgument)
            return;
        const auto& formal = symbol.as<ast::FormalArgumentSymbol>();
        row.readonly = formal.flags.has(ast::VariableFlags::Const);
        row.refStatic = formal.flags.has(ast::VariableFlags::RefStatic);
    };
    for (const auto& [symbol, field] : b_.frameVars)
        qualify(*symbol, field);
    // A ref structure's binding is one per leaf (SIR_SPEC.md 8).
    for (const auto& [symbol, first] : b_.structFrames) {
        if (symbol->kind != ast::SymbolKind::FormalArgument) continue;
        std::vector<Leaf> leaves;
        structLeaves(symbol->as<ast::FormalArgumentSymbol>().getType(), "", leaves);
        for (std::uint32_t k = 0; k < leaves.size(); ++k)
            qualify(*symbol, sir::FrameVarId{first.index() + k});
    }
}
void Lowerer::unwindLexical(std::size_t depth, slang::SourceLocation sl) {
    for (std::size_t i = b_.lexicalStack.size(); i-- > depth;) {
        const auto index = b_.lexicalStack[i];
        if (index != 0)
            b_.lexicalEdits.push_back(
                {b_.block, block(b_.block).instrs.size(), index, false, loc(sl)});
        if (const auto control = b_.lexical[index].control)
            emitVoid(sir::Op::ScopeLeave, {sir::Operand::scope(*control)}, sl);
    }
}
void Lowerer::finishLexical() {
    if (activationInfo().scopes.empty()) return;
    // Edits refer to original instruction positions. Stable order preserves
    // empty scopes and interleaves automatic exits with named control exits.
    std::stable_sort(
        b_.lexicalEdits.begin(), b_.lexicalEdits.end(), [](const auto& a, const auto& b) {
            return std::pair{a.block.index(), a.position} < std::pair{b.block.index(), b.position};
        });
    std::size_t next = 0;
    for (std::size_t i = 0; i < blocks().size(); ++i) {
        auto& original = blocks()[i].instrs;
        std::vector<sir::Instr> result;
        result.reserve(original.size());
        for (std::size_t at = 0; at <= original.size(); ++at) {
            while (next < b_.lexicalEdits.size() && b_.lexicalEdits[next].block.index() == i &&
                   b_.lexicalEdits[next].position == at) {
                const auto& edit = b_.lexicalEdits[next++];
                const auto scope = b_.lexical[edit.lexical].scope;
                if (scope == sir::kNone) continue;
                result.push_back({edit.enter ? sir::Op::ActivationEnter : sir::Op::ActivationLeave,
                                  sir::voidType(),
                                  sir::kNone,
                                  {sir::Operand::activation(scope)},
                                  edit.loc});
            }
            if (at < original.size()) result.push_back(std::move(original[at]));
        }
        original = std::move(result);
    }
}
void Lowerer::captureForkEnvironment(const ast::Statement& stmt, sir::Process& child, Body& next) {
    std::set<std::uint32_t> active;
    for (const auto index : b_.lexicalStack) {
        const auto scope = b_.lexical[index].scope;
        if (scope == sir::kNone) continue;
        active.insert(scope.index());
        child.activation.extents.push_back(scope);
    }
    child.activation.inheritCaptures =
        !activationInfo().extents.empty() || activationInfo().inheritCaptures;
    std::set<sir::FrameVarId> usedRefs;
    const auto reference = [&](const ast::Symbol& symbol) {
        if (const auto found = b_.frameVars.find(&symbol);
            found != b_.frameVars.end() && frameVar(found->second).type.kind == sir::TypeKind::Ref)
            usedRefs.insert(found->second);
    };
    auto visitor = ast::makeVisitor(
        [&](auto&, const ast::NamedValueExpression& e) { reference(e.symbol); },
        [&](auto&, const ast::HierarchicalValueExpression& e) { reference(e.symbol); });
    stmt.visit(visitor);
    std::vector<sir::FrameVarId> mapping(frameVars().size(), sir::FrameVarId{sir::kNone});
    for (std::uint32_t i = 0; i < frameVars().size(); ++i) {
        const auto& row = frameVars()[i];
        const sir::FrameVarId field{i};
        const bool eligible = row.type.kind == sir::TypeKind::Ref
                                  ? usedRefs.contains(field)
                                  : row.home == sir::FrameHome::Capture ||
                                        (row.home == sir::FrameHome::Scoped &&
                                         active.contains(row.activationScope.index()));
        if (!eligible) continue;
        auto capture = row;
        capture.defaultInit = false;
        capture.home = sir::FrameHome::Capture;
        capture.activationScope = sir::kNone;
        capture.captureSource = field;
        mapping[i] = sir::FrameVarId{static_cast<std::uint32_t>(child.frame.size())};
        child.frame.push_back(std::move(capture));
    }
    for (const auto& [symbol, field] : b_.frameVars)
        if (mapping[field.index()] != sir::kNone)
            next.frameVars.emplace(symbol, mapping[field.index()]);
    for (const auto& [symbol, first] : b_.structFrames)
        if (mapping[first.index()] != sir::kNone)
            next.structFrames.emplace(symbol, mapping[first.index()]);
}

}  // namespace kirimasim::front
