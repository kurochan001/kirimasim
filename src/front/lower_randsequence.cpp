// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// IEEE 1800-2023 18.17, SYSTEM_TASKS.md 13.3.
#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"

namespace kirimasim::front {

namespace {
using Grammar = ast::RandSequenceStatement;
using Production = ast::RandSeqProductionSymbol;

// Each statement owns its bound rules. A nested grammar can refer to a
// production in an enclosing statement, so resolve by symbol identity.
class ProductionRules {
public:
    explicit ProductionRules(const Grammar& sequence) { remember(sequence); }

    std::span<const Grammar::Rule> get(const Production& production) {
        if (auto it = rules.find(&production); it != rules.end()) return it->second;
        const auto& owner = production.getParentScope()->asSymbol().as<ast::StatementBlockSymbol>();
        const auto& block = owner.tryGetStatement()->as<ast::BlockStatement>();
        remember(block.body.as<Grammar>());
        return rules.at(&production);
    }

private:
    std::map<const Production*, std::span<const Grammar::Rule>> rules;
    void remember(const Grammar& sequence) {
        for (const auto& production : sequence.productions)
            rules.emplace(production.symbol.get(), production.rules);
    }
};

using sir::Op;
using sir::Operand;
using slang::SourceLocation;
// The optional trailing code belongs to the selected rule, after its items.
const ast::StatementBlockSymbol* trailingBlock(const Grammar::Rule& rule) {
    const auto& trailer = rule.codeBlock;
    return trailer.has_value() ? trailer->block.get() : nullptr;
}
}  // namespace

std::vector<const ast::Statement*> randSequenceBlocks(const ast::RandSequenceStatement& sequence) {
    std::vector<const ast::Statement*> blocks;
    if (!sequence.firstProduction) return blocks;
    std::vector<const Production*> pending{sequence.firstProduction};
    ProductionRules rules(sequence);
    std::set<const Production*> seen;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (!seen.insert(pending[i]).second) continue;
        for (const auto& rule : rules.get(*pending[i])) {
            for (const auto* item : rule.prods) {
                if (auto* call = item->as_if<Grammar::ProdItem>(); call && call->target)
                    pending.push_back(call->target);
                if (auto* code = item->as_if<Grammar::CodeBlockProd>())
                    blocks.push_back(code->block->tryGetStatement());
            }
            if (const auto* trailer = trailingBlock(rule))
                blocks.push_back(trailer->tryGetStatement());
        }
    }
    return blocks;
}

namespace {

// Only a transfer whose target is inside this code block can be lowered as
// an ordinary statement in its enclosing process or subroutine.
template <typename CheckNested>
void checkCode(Lowerer& lower, const ast::StatementBlockSymbol& code, CheckNested checkNested) {
    unsigned loopDepth = 0;
    auto visitor = ast::makeVisitor([&](auto& v, const auto& node) {
        using T = std::remove_cvref_t<decltype(node)>;
        if constexpr (std::is_base_of_v<ast::Statement, T>)
            lower.checkStack(node.sourceRange.start());
        if constexpr (std::is_same_v<T, ast::RandSequenceStatement>) {
            if (node.firstProduction) checkNested(*node.firstProduction);
        } else if constexpr (std::is_same_v<T, ast::BlockStatement>) {
            if (node.blockKind != ast::StatementBlockKind::Sequential &&
                node.blockKind != ast::StatementBlockKind::JoinNone)
                lower.unsupported(code.location, "a waiting randsequence code block");
            if (node.blockSymbol) {
                for (const auto& member : node.blockSymbol->members())
                    if (member.kind != ast::SymbolKind::StatementBlock &&
                        member.kind != ast::SymbolKind::RandSeqProduction)
                        lower.unsupported(member.location, "a randsequence code block declaration");
            }
        } else if constexpr (std::is_same_v<T, ast::VariableDeclStatement>)
            lower.unsupported(node.sourceRange.start(), "a randsequence code block declaration");
        else if constexpr (std::is_same_v<T, ast::TimedStatement> ||
                           std::is_same_v<T, ast::WaitStatement> ||
                           std::is_same_v<T, ast::WaitForkStatement> ||
                           std::is_same_v<T, ast::WaitOrderStatement>)
            lower.unsupported(node.sourceRange.start(), "a waiting randsequence code block");
        else if constexpr (std::is_same_v<T, ast::AssignmentExpression>) {
            if (node.timingControl)
                lower.unsupported(node.sourceRange.start(), "a delayed randsequence assignment");
        } else if constexpr (std::is_same_v<T, ast::EventTriggerStatement>) {
            if (node.timing)
                lower.unsupported(node.sourceRange.start(), "a delayed randsequence event");
        } else if constexpr (std::is_same_v<T, ast::CallExpression>) {
            if (!node.isSystemCall()) {
                const auto& sub = *std::get<0>(node.subroutine);
                if (sub.subroutineKind == ast::SubroutineKind::Task &&
                    (sub.isVirtual() || mayWait(sub.getBody())))
                    lower.unsupported(node.sourceRange.start(), "a waiting randsequence task call");
            }
        } else if constexpr (std::is_same_v<T, ast::ReturnStatement> ||
                             std::is_same_v<T, ast::BreakStatement> ||
                             std::is_same_v<T, ast::ContinueStatement>) {
            if (std::is_same_v<T, ast::ReturnStatement> || loopDepth == 0)
                lower.unsupported(node.sourceRange.start(),
                                  "a transfer out of a randsequence code block");
        } else if constexpr (std::is_same_v<T, ast::DisableStatement>) {
            auto* target = node.target.template as_if<ast::ArbitrarySymbolExpression>();
            const ast::Symbol* symbol = target ? target->symbol.get() : nullptr;
            while (symbol && symbol != &code) {
                auto* scope = symbol->getParentScope();
                symbol = scope ? &scope->asSymbol() : nullptr;
            }
            if (!symbol)
                lower.unsupported(node.sourceRange.start(),
                                  "a transfer out of a randsequence code block");
        }
        constexpr bool loop = std::is_same_v<T, ast::ForLoopStatement> ||
                              std::is_same_v<T, ast::ForeachLoopStatement> ||
                              std::is_same_v<T, ast::RepeatLoopStatement> ||
                              std::is_same_v<T, ast::WhileLoopStatement> ||
                              std::is_same_v<T, ast::DoWhileLoopStatement> ||
                              std::is_same_v<T, ast::ForeverLoopStatement>;
        if constexpr (loop) ++loopDepth;
        v.visitDefault(node);
        if constexpr (loop) --loopDepth;
    });
    code.tryGetStatement()->visit(visitor);
}

struct ProductionBody {
    std::vector<std::uint32_t> limits;
    std::size_t callers = 0;
    bool ready = false;
    sir::BlockId entry = sir::kNone;
    sir::BlockId exit = sir::kNone;
    sir::FrameVarId selector = sir::kNone;
    std::vector<sir::BlockId> continuations;
};

class Sequence {
public:
    Sequence(Lowerer& lower, const Grammar& sequence) : lower(lower), rulesByProduction(sequence) {}

    void run(const Production& start) {
        inspect(start);
        call(start);
        const auto end = lower.body().block;
        const bool dead = lower.body().dead;
        // Use discovery order: pointer order must never affect the SIR.
        for (const Production* p : shared) {
            auto& body = bodies.at(p);
            if (body.exit == sir::kNone) continue;
            lower.setBlock(body.exit);
            const auto selector =
                lower.emit(Op::Fload, u32, {Operand::frame(body.selector)}, p->location);
            for (std::size_t i = 0; i + 1 < body.continuations.size(); ++i)
                branch(Op::Eq, selector, i, body.continuations[i], p->location);
            lower.jump(body.continuations.back(), p->location);
        }
        lower.setBlock(end);
        lower.body().dead = dead;
    }

private:
    Lowerer& lower;
    ProductionRules rulesByProduction;
    std::map<const Production*, ProductionBody> bodies;
    std::vector<const Production*> shared;
    const sir::Type u32 = sir::u2(32);

    void inspect(const Production& prod) {
        lower.checkStack(prod.location);
        auto [it, fresh] = bodies.try_emplace(&prod);
        auto& body = it->second;
        ++body.callers;
        if (!fresh) {
            if (!body.ready) lower.unsupported(prod.location, "recursive randsequence productions");
            return;
        }
        if (!prod.arguments.empty() || !prod.getReturnType().isVoid())
            lower.unsupported(prod.location, "randsequence production arguments or return values");
        const auto inspectNested = [&](const Production& child) { inspect(child); };
        std::uint32_t sum = 0;
        for (const auto& rule : rulesByProduction.get(prod)) {
            const auto sl = rule.ruleBlock->location;
            if (rule.isRandJoin) lower.unsupported(sl, "randsequence rand join");
            const auto* expr = rule.weightExpr;
            const auto weight = expr ? lower.constInt(*expr) : std::optional<std::int64_t>(1);
            const auto weightLoc = expr ? expr->sourceRange.start() : sl;
            if (!weight || *weight <= 0)
                lower.unsupported(weightLoc,
                                  "a randsequence weight other than a positive integral constant");
            if (static_cast<std::uint64_t>(*weight) > UINT32_MAX - sum)
                lower.unsupported(weightLoc, "a randsequence weight sum wider than 32 bits");
            sum += static_cast<std::uint32_t>(*weight);
            body.limits.push_back(sum);
            for (const auto* item : rule.prods) {
                if (const auto* call = item->as_if<Grammar::ProdItem>()) {
                    if (!call->target || !call->args.empty())
                        lower.unsupported(sl, "randsequence production arguments");
                    inspect(*call->target);
                } else if (const auto* code = item->as_if<Grammar::CodeBlockProd>())
                    checkCode(lower, *code->block, inspectNested);
                else
                    lower.unsupported(sl, "a randsequence production control item");
            }
            if (const auto* trailer = trailingBlock(rule))
                checkCode(lower, *trailer, inspectNested);
        }
        body.ready = true;
    }

    // A shared production is compiled once. Its return address belongs to
    // this activation; acyclicity ensures a call cannot overwrite its own.
    void call(const Production& prod) {
        lower.checkStack(prod.location);
        auto& body = bodies.at(&prod);
        if (body.callers == 1) {
            emitRules(prod, body);
            return;
        }
        const bool first = body.entry == sir::kNone;
        if (first) {
            body.entry = lower.newBlock();
            body.selector = lower.newFrameVar(u32);
            shared.push_back(&prod);
        }
        lower.emitVoid(
            Op::Fstore,
            {Operand::frame(body.selector), lower.constant(u32, body.continuations.size()).op},
            prod.location);
        lower.jump(body.entry, prod.location);
        if (first) {
            lower.setBlock(body.entry);
            emitRules(prod, body);
            if (!lower.body().dead) body.exit = lower.body().block;
        }
        if (body.exit != sir::kNone) {
            body.continuations.push_back(lower.newBlock());
            lower.setBlock(body.continuations.back());
        }
    }

    void branch(Op comparison, const Val& value, std::uint32_t limit, sir::BlockId target,
                SourceLocation sl) {
        const auto next = lower.newBlock();
        const auto cond =
            lower.emit(comparison, sir::u2(1), {value.op, lower.constant(u32, limit).op}, sl);
        lower.terminate(Op::BrIf, {cond.op, Operand::block(target), Operand::block(next)}, sl);
        lower.setBlock(next);
    }

    void emitRules(const Production& prod, const ProductionBody& body) {
        const auto rules = rulesByProduction.get(prod);
        const auto sl = prod.location;
        std::vector<sir::BlockId> alternatives;
        if (rules.size() > 1) {
            const auto draw = lower.emit(Op::Syscall, u32,
                                         {Operand::str("$urandom_range"),
                                          lower.constant(u32, body.limits.back() - 1).op,
                                          lower.constant(u32, 0).op},
                                         sl);
            for (std::size_t i = 0; i + 1 < rules.size(); ++i) {
                alternatives.push_back(lower.newBlock());
                branch(Op::Lt, draw, body.limits[i], alternatives.back(), sl);
            }
        }
        alternatives.push_back(lower.body().block);
        std::vector<sir::BlockId> exits;
        for (std::size_t i = 0; i < rules.size(); ++i) {
            lower.setBlock(alternatives[i]);
            for (const auto* item : rules[i].prods) {
                if (lower.body().dead) break;
                if (const auto* target = item->as_if<Grammar::ProdItem>())
                    call(*target->target);
                else
                    lower.lowerStatement(
                        *item->as<Grammar::CodeBlockProd>().block->tryGetStatement());
            }
            if (const auto* trailer = trailingBlock(rules[i]))
                lower.lowerStatement(*trailer->tryGetStatement());
            if (!lower.body().dead) exits.push_back(lower.body().block);
        }
        if (exits.size() == 1) lower.setBlock(exits.front());
        if (exits.size() <= 1) return;
        const auto join = lower.newBlock();
        for (auto exit : exits)
            lower.emitInto(exit, Op::Br, {Operand::block(join)}, sl);
        lower.setBlock(join);
    }
};
}  // namespace

void Lowerer::lowerRandSequence(const ast::RandSequenceStatement& s) {
    if (s.firstProduction) Sequence(*this, s).run(*s.firstProduction);
}

}  // namespace kirimasim::front
