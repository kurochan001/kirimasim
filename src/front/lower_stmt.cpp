// Lowering of procedural statements to SIR basic blocks.
//
// Code after a statement that never falls through (break, continue, a loop
// without an exit) is unreachable and is dropped, since the verifier
// rejects unreachable blocks (SIR_SPEC.md 10, rule 3). Loop exits and
// continue targets are created only when something jumps to them, for the
// same reason.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/statements/ConditionalStatements.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/types/Type.h"

namespace kirimasim::front {

namespace {

using sir::Op;
using sir::Operand;
using sir::Type;

}  // namespace

sir::BlockId Lowerer::breakTarget(std::size_t loop) {
    if (b_.loops[loop].breakTo == sir::kNone) b_.loops[loop].breakTo = newBlock();
    return b_.loops[loop].breakTo;
}

sir::BlockId Lowerer::continueTarget(std::size_t loop) {
    if (b_.loops[loop].continueTo == sir::kNone) b_.loops[loop].continueTo = newBlock();
    return b_.loops[loop].continueTo;
}

void Lowerer::jump(sir::BlockId to, slang::SourceLocation sl) {
    if (!b_.dead) terminate(Op::Br, {Operand::block(to)}, sl);
}

void Lowerer::endLoop(std::size_t loop) {
    const sir::BlockId exit = b_.loops[loop].breakTo;
    b_.loops.pop_back();
    if (exit == sir::kNone) {
        b_.dead = true;  // nothing leaves the loop
        return;
    }
    setBlock(exit);
}

void Lowerer::lowerStatement(const ast::Statement& s) {
    if (b_.dead) return;  // unreachable code is dropped
    const slang::SourceLocation sl = s.sourceRange.start();
    switch (s.kind) {
        case ast::StatementKind::Empty: return;
        case ast::StatementKind::List:
            for (const ast::Statement* item : s.as<ast::StatementList>().list)
                lowerStatement(*item);
            return;
        case ast::StatementKind::Block: {
            const auto& b = s.as<ast::BlockStatement>();
            if (b.blockKind != ast::StatementBlockKind::Sequential) {
                lowerFork(b);
                return;
            }
            // A named block is a scope of the hierarchy (9.3.5).
            const std::string outer = b_.scope;
            if (b.blockSymbol != nullptr && !b.blockSymbol->name.empty())
                b_.scope += "." + std::string(b.blockSymbol->name);
            lowerStatement(b.body);
            b_.scope = outer;
            return;
        }
        case ast::StatementKind::VariableDeclaration: {
            const auto& v = s.as<ast::VariableDeclStatement>().symbol;
            if (v.lifetime == ast::VariableLifetime::Static) {
                // Static: a slot, set once before time 0 starts (6.21) by
                // the static-init process, not when the block runs.
                const sir::SlotId slot = slotOf(v, sl);
                if (v.getInitializer() != nullptr) lowerStaticInit(v, slot, sl);
                return;
            }
            // Automatic: a frame variable, set each time the scope is
            // entered (SIR_SPEC.md 7.4).
            const sir::FrameVarId f = frameVarOf(v, sl);
            const Type t = frameVars()[f].type;
            const ast::Expression* init = v.getInitializer();
            const Val value =
                init != nullptr
                    ? coerce(lowerExpr(*init), t, sl)
                    : (sir::isFourState(t) ? constant(t, ~std::uint64_t{0}, ~std::uint64_t{0})
                                           : constant(t, 0));
            emitVoid(Op::Fstore, {Operand::frame(f), value.op}, sl);
            return;
        }
        case ast::StatementKind::ExpressionStatement: {
            const ast::Expression& e = s.as<ast::ExpressionStatement>().expr;
            if (e.kind == ast::ExpressionKind::Assignment) {
                lowerAssignment(e.as<ast::AssignmentExpression>());
                return;
            }
            if (e.kind == ast::ExpressionKind::Call) {
                const auto& call = e.as<ast::CallExpression>();
                if (call.isSystemCall())
                    lowerSystemTask(call);
                else
                    lowerCall(call);
                return;
            }
            if (e.kind == ast::ExpressionKind::Conversion && e.type->isVoid()) {
                lowerExpr(e.as<ast::ConversionExpression>().operand());  // void'(f())
                return;
            }
            if (e.kind == ast::ExpressionKind::UnaryOp) {
                switch (e.as<ast::UnaryExpression>().op) {
                    case ast::UnaryOperator::Preincrement:
                    case ast::UnaryOperator::Predecrement:
                    case ast::UnaryOperator::Postincrement:
                    case ast::UnaryOperator::Postdecrement: lowerExpr(e); return;
                    default: break;
                }
            }
            unsupported(sl, "expression statement");
        }
        case ast::StatementKind::Timed: {
            const auto& ts = s.as<ast::TimedStatement>();
            lowerTiming(ts.timing);
            lowerStatement(ts.stmt);
            return;
        }
        case ast::StatementKind::Conditional: lowerIf(s.as<ast::ConditionalStatement>()); return;
        case ast::StatementKind::ImmediateAssertion:
            lowerImmediateAssertion(s.as<ast::ImmediateAssertionStatement>());
            return;
        case ast::StatementKind::Case: lowerCase(s.as<ast::CaseStatement>()); return;
        case ast::StatementKind::ForLoop: {
            const auto& f = s.as<ast::ForLoopStatement>();
            // Loop variables (automatic, 12.7.1) come with declaration
            // statements of their own just before the loop, which set them.
            for (const ast::VariableSymbol* v : f.loopVars)
                frameVarOf(*v, sl);
            for (const ast::Expression* init : f.initializers)
                lowerExpr(*init);
            if (f.stopExpr == nullptr) unsupported(sl, "for loop without a condition");
            const sir::BlockId cond = newBlock();
            jump(cond, sl);
            setBlock(cond);
            const std::size_t loop = b_.loops.size();
            b_.loops.push_back({});
            const Val c = lowerCondition(*f.stopExpr);
            const sir::BlockId body = newBlock();
            terminate(Op::BrIf, {c.op, Operand::block(body), Operand::block(breakTarget(loop))},
                      sl);
            setBlock(body);
            lowerStatement(f.body);
            if (!b_.dead) jump(continueTarget(loop), sl);
            if (b_.loops[loop].continueTo != sir::kNone) {
                setBlock(b_.loops[loop].continueTo);
                for (const ast::Expression* step : f.steps)
                    lowerExpr(*step);
                jump(cond, sl);
            }
            endLoop(loop);
            return;
        }
        case ast::StatementKind::ForeachLoop: {
            const auto& f = s.as<ast::ForeachLoopStatement>();
            std::vector<const ast::ForeachLoopStatement::LoopDim*> dims;
            for (const auto& d : f.loopDims) {
                if (d.loopVar == nullptr) continue;
                if (!d.range) unsupported(sl, "foreach over a dynamically sized dimension");
                dims.push_back(&d);
            }
            lowerForeach(dims, 0, f.body, sl);
            return;
        }
        case ast::StatementKind::RepeatLoop: {
            const auto& r = s.as<ast::RepeatLoopStatement>();
            // The count is evaluated once (12.7.2) and kept in the frame,
            // since the body may wait.
            const Val n = lowerExpr(r.count);
            if (!sir::isIntegral(n.type)) unsupported(sl, "repeat count of this type");
            const sir::FrameVarId fv = newFrameVar(n.type);
            emitVoid(Op::Fstore, {Operand::frame(fv), n.op}, sl);
            const sir::BlockId cond = newBlock();
            jump(cond, sl);
            setBlock(cond);
            const std::size_t loop = b_.loops.size();
            b_.loops.push_back({});
            // A negative, x or z count runs the body no times: gt gives 0
            // or x, and br.if takes x as false.
            const Val left = emit(Op::Fload, n.type, {Operand::frame(fv)}, sl);
            const Type bt = sir::isFourState(n.type) ? sir::u4(1) : sir::u2(1);
            const Val more = emit(Op::Gt, bt, {left.op, constant(n.type, 0).op}, sl);
            const sir::BlockId body = newBlock();
            terminate(Op::BrIf, {more.op, Operand::block(body), Operand::block(breakTarget(loop))},
                      sl);
            setBlock(body);
            const Val left2 = emit(Op::Fload, n.type, {Operand::frame(fv)}, sl);
            const Val dec = emit(Op::Sub, n.type, {left2.op, constant(n.type, 1).op}, sl);
            emitVoid(Op::Fstore, {Operand::frame(fv), dec.op}, sl);
            lowerStatement(r.body);
            if (!b_.dead) jump(continueTarget(loop), sl);
            if (b_.loops[loop].continueTo != sir::kNone) {
                setBlock(b_.loops[loop].continueTo);
                jump(cond, sl);
            }
            endLoop(loop);
            return;
        }
        case ast::StatementKind::WhileLoop: {
            const auto& w = s.as<ast::WhileLoopStatement>();
            const sir::BlockId cond = newBlock();
            jump(cond, sl);
            setBlock(cond);
            const std::size_t loop = b_.loops.size();
            b_.loops.push_back({});
            b_.loops[loop].continueTo = cond;
            const Val c = lowerCondition(w.cond);
            const sir::BlockId body = newBlock();
            terminate(Op::BrIf, {c.op, Operand::block(body), Operand::block(breakTarget(loop))},
                      sl);
            setBlock(body);
            lowerStatement(w.body);
            jump(cond, sl);
            endLoop(loop);
            return;
        }
        case ast::StatementKind::DoWhileLoop: {
            const auto& w = s.as<ast::DoWhileLoopStatement>();
            const sir::BlockId body = newBlock();
            jump(body, sl);
            setBlock(body);
            const std::size_t loop = b_.loops.size();
            b_.loops.push_back({});
            lowerStatement(w.body);
            if (!b_.dead) jump(continueTarget(loop), sl);
            if (b_.loops[loop].continueTo != sir::kNone) {
                setBlock(b_.loops[loop].continueTo);
                const Val c = lowerCondition(w.cond);
                terminate(Op::BrIf, {c.op, Operand::block(body), Operand::block(breakTarget(loop))},
                          sl);
            }
            endLoop(loop);
            return;
        }
        case ast::StatementKind::ForeverLoop: {
            // A loop that never waits is stopped by the zero-time limits
            // (CLI.md 2.6).
            const sir::BlockId body = newBlock();
            jump(body, sl);
            setBlock(body);
            const std::size_t loop = b_.loops.size();
            b_.loops.push_back({});
            b_.loops[loop].continueTo = body;
            lowerStatement(s.as<ast::ForeverLoopStatement>().body);
            jump(body, sl);
            endLoop(loop);
            return;
        }
        case ast::StatementKind::DisableFork: emitVoid(Op::DisableFork, {}, sl); return;
        case ast::StatementKind::Return: lowerReturn(s.as<ast::ReturnStatement>().expr, sl); return;
        case ast::StatementKind::WaitFork: {
            const sir::BlockId cont = newBlock();
            terminate(Op::WaitFork, {Operand::block(cont)}, sl);
            setBlock(cont);
            return;
        }
        case ast::StatementKind::Break:
            if (b_.loops.empty()) unsupported(sl, "break outside a loop");
            terminate(Op::Br, {Operand::block(breakTarget(b_.loops.size() - 1))}, sl);
            return;
        case ast::StatementKind::Continue:
            if (b_.loops.empty()) unsupported(sl, "continue outside a loop");
            terminate(Op::Br, {Operand::block(continueTarget(b_.loops.size() - 1))}, sl);
            return;
        default: unsupported(sl, "statement '" + std::string(ast::toString(s.kind)) + "'");
    }
}

void Lowerer::lowerForeach(const std::vector<const ast::ForeachLoopStatement::LoopDim*>& dims,
                           std::size_t k, const ast::Statement& body, slang::SourceLocation sl) {
    if (k == dims.size()) {
        lowerStatement(body);
        return;
    }
    // Each index runs from the left bound to the right bound (12.7.3).
    const ast::ForeachLoopStatement::LoopDim& d = *dims[k];
    const sir::FrameVarId fv = frameVarOf(*d.loopVar, sl);
    const Type t = frameVars()[fv].type;
    const slang::ConstantRange range = d.range.value_or(slang::ConstantRange{});
    const std::int64_t left = range.left;
    const std::int64_t right = range.right;
    emitVoid(Op::Fstore, {Operand::frame(fv), constant(t, static_cast<std::uint64_t>(left)).op},
             sl);
    const sir::BlockId cond = newBlock();
    jump(cond, sl);
    setBlock(cond);
    const std::size_t loop = b_.loops.size();
    b_.loops.push_back({});
    const Val i = emit(Op::Fload, t, {Operand::frame(fv)}, sl);
    const Type bt = sir::isFourState(t) ? sir::u4(1) : sir::u2(1);
    const Val c = emit(left <= right ? Op::Le : Op::Ge, bt,
                       {i.op, constant(t, static_cast<std::uint64_t>(right)).op}, sl);
    const sir::BlockId inner = newBlock();
    terminate(Op::BrIf, {c.op, Operand::block(inner), Operand::block(breakTarget(loop))}, sl);
    setBlock(inner);
    lowerForeach(dims, k + 1, body, sl);
    if (!b_.dead) jump(continueTarget(loop), sl);
    if (b_.loops[loop].continueTo != sir::kNone) {
        setBlock(b_.loops[loop].continueTo);
        const Val i2 = emit(Op::Fload, t, {Operand::frame(fv)}, sl);
        const Val n = emit(left <= right ? Op::Add : Op::Sub, t, {i2.op, constant(t, 1).op}, sl);
        emitVoid(Op::Fstore, {Operand::frame(fv), n.op}, sl);
        jump(cond, sl);
    }
    endLoop(loop);
}

void Lowerer::lowerFork(const ast::BlockStatement& b) {
    const slang::SourceLocation sl = b.sourceRange.start();
    // Each statement of the fork is a child process (SIR_SPEC.md 7.5).
    std::vector<const ast::Statement*> stmts;
    if (b.body.kind == ast::StatementKind::List) {
        for (const ast::Statement* item : b.body.as<ast::StatementList>().list)
            stmts.push_back(item);
    } else {
        stmts.push_back(&b.body);
    }
    const std::uint32_t parent = b_.proc;
    const std::uint32_t forkNumber = b_.forks++;
    std::string scope = b_.scope;  // a named fork is a scope (9.3.5)
    if (b.blockSymbol != nullptr && !b.blockSymbol->name.empty())
        scope += "." + std::string(b.blockSymbol->name);
    std::vector<Operand> ops;
    for (std::size_t i = 0; i < stmts.size(); ++i) {
        if (stmts[i]->kind == ast::StatementKind::VariableDeclaration)
            unsupported(stmts[i]->sourceRange.start(), "declarations in a fork block");
        if (stmts[i]->kind == ast::StatementKind::Empty) continue;
        sir::Process child;
        child.name = proc().name + ".fork" + std::to_string(forkNumber) + "." + std::to_string(i);
        child.kind = sir::ProcessKind::General;
        child.instance = proc().instance;
        child.parent = {sir::Owner::Kind::Process, parent};
        child.loc = loc(stmts[i]->sourceRange.start());
        child.blocks.resize(1);
        const auto id = static_cast<std::uint32_t>(design().processes.size());
        design().processes.push_back(std::move(child));
        Body saved = std::move(b_);
        b_ = Body{};
        b_.proc = id;
        b_.scale = saved.scale;
        b_.scope = scope;
        lowerStatement(*stmts[i]);
        if (!b_.dead) terminate(Op::Ret, {}, sl);
        b_ = std::move(saved);
        ops.push_back(Operand::process(id));
    }
    const char* join = b.blockKind == ast::StatementBlockKind::JoinAll   ? "join"
                       : b.blockKind == ast::StatementBlockKind::JoinAny ? "join_any"
                                                                         : "join_none";
    ops.push_back(Operand::str(join));
    const sir::BlockId cont = newBlock();
    ops.push_back(Operand::block(cont));
    terminate(Op::Fork, std::move(ops), sl);
    setBlock(cont);
}

void Lowerer::reportViolations(const std::vector<Val>& matches, ast::UniquePriorityCheck check,
                               bool hasElse, bool isCase, slang::SourceLocation sl) {
    std::string construct = check == ast::UniquePriorityCheck::Unique    ? "unique"
                            : check == ast::UniquePriorityCheck::Unique0 ? "unique0"
                                                                         : "priority";
    construct += isCase ? " case" : " if";
    Val any = matches.empty() ? constant(sir::u2(1), 0) : matches[0];
    Val multi = constant(sir::u2(1), 0);
    for (std::size_t i = 1; i < matches.size(); ++i) {
        const Val both = emit(Op::And, sir::u2(1), {any.op, matches[i].op}, sl);
        multi = emit(Op::Or, sir::u2(1), {multi.op, both.op}, sl);
        any = emit(Op::Or, sir::u2(1), {any.op, matches[i].op}, sl);
    }
    const auto report = [&](const Val& cond, bool when, const char* content) {
        const sir::BlockId rep = newBlock();
        const sir::BlockId next = newBlock();
        terminate(Op::BrIf,
                  {cond.op, Operand::block(when ? rep : next), Operand::block(when ? next : rep)},
                  sl);
        setBlock(rep);
        emitVoid(Op::ReportDefer, {Operand::str(construct), Operand::str(content)}, sl);
        jump(next, sl);
        setBlock(next);
    };
    // unique and unique0: more than one; unique and priority: none, unless
    // there is an else or default (12.4.2, 12.5.3).
    if (check != ast::UniquePriorityCheck::Priority && matches.size() >= 2)
        report(multi, true,
               isCase ? "more than one item matches" : "more than one condition is true");
    if (check != ast::UniquePriorityCheck::Unique0 && !hasElse)
        report(any, false, isCase ? "no item matches" : "no condition is true");
}

void Lowerer::lowerUniqueIf(const ast::ConditionalStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    // The if-else-if chain: every condition in order, then the checks, then
    // the first true one (SCHEDULER_SPEC.md 15.1).
    std::vector<const ast::Statement*> bodies;
    std::vector<Val> trues;
    const ast::Statement* elseStmt = nullptr;
    const ast::ConditionalStatement* cur = &s;
    for (;;) {
        if (cur->conditions.size() != 1 || cur->conditions[0].pattern != nullptr)
            unsupported(sl, "if with a pattern");
        const Val c = lowerCondition(*cur->conditions[0].expr);
        // Only a known 1 is true; x counts as false here too.
        trues.push_back(emit(Op::Ceq, sir::u2(1), {c.op, constant(c.type, 1).op}, sl));
        bodies.push_back(&cur->ifTrue);
        if (cur->ifFalse != nullptr && cur->ifFalse->kind == ast::StatementKind::Conditional) {
            cur = &cur->ifFalse->as<ast::ConditionalStatement>();
            continue;
        }
        elseStmt = cur->ifFalse;
        break;
    }
    reportViolations(trues, s.check, elseStmt != nullptr, false, sl);
    std::vector<sir::BlockId> ends;
    std::vector<sir::BlockId> blocks;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        blocks.push_back(newBlock());
        const sir::BlockId next = newBlock();
        terminate(Op::BrIf, {trues[i].op, Operand::block(blocks[i]), Operand::block(next)}, sl);
        setBlock(next);
    }
    if (elseStmt != nullptr) lowerStatement(*elseStmt);
    if (!b_.dead) ends.push_back(b_.block);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        setBlock(blocks[i]);
        lowerStatement(*bodies[i]);
        if (!b_.dead) ends.push_back(b_.block);
    }
    if (ends.empty()) {
        b_.dead = true;
        return;
    }
    const sir::BlockId join = newBlock();
    for (const sir::BlockId e : ends)
        emitInto(e, Op::Br, {Operand::block(join)}, sl);
    setBlock(join);
}

void Lowerer::lowerIf(const ast::ConditionalStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    if (s.check != ast::UniquePriorityCheck::None) {
        lowerUniqueIf(s);
        return;
    }
    if (s.conditions.size() != 1 || s.conditions[0].pattern != nullptr)
        unsupported(sl, "if with a pattern");
    const Val c = lowerCondition(*s.conditions[0].expr);
    const sir::BlockId tb = newBlock();
    const sir::BlockId fb = newBlock();
    terminate(Op::BrIf, {c.op, Operand::block(tb), Operand::block(fb)}, sl);
    setBlock(tb);
    lowerStatement(s.ifTrue);
    const bool thenLive = !b_.dead;
    const sir::BlockId thenEnd = b_.block;
    setBlock(fb);
    if (s.ifFalse != nullptr) lowerStatement(*s.ifFalse);
    const bool elseLive = !b_.dead;
    const sir::BlockId elseEnd = b_.block;
    if (!thenLive && !elseLive) {
        b_.dead = true;
        return;
    }
    const sir::BlockId join = newBlock();
    if (thenLive) emitInto(thenEnd, Op::Br, {Operand::block(join)}, sl);
    if (elseLive) emitInto(elseEnd, Op::Br, {Operand::block(join)}, sl);
    setBlock(join);
}

void Lowerer::lowerImmediateAssertion(const ast::ImmediateAssertionStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    if (s.isDeferred) unsupported(sl, "deferred immediate assertion");
    // The expression is a condition as in if: x, z and 0 fail (16.3). The
    // pass statement runs on success; the fail statement on failure, or
    // $error when an assert or assume has none. A cover has no failure.
    const Val c = lowerCondition(s.cond);
    const sir::BlockId pass = newBlock();
    const sir::BlockId fail = newBlock();
    const sir::BlockId join = newBlock();
    terminate(Op::BrIf, {c.op, Operand::block(pass), Operand::block(fail)}, sl);
    setBlock(pass);
    if (s.ifTrue != nullptr) lowerStatement(*s.ifTrue);
    jump(join, sl);
    setBlock(fail);
    if (s.ifFalse != nullptr) {
        lowerStatement(*s.ifFalse);
    } else if (s.assertionKind == ast::AssertionKind::Assert ||
               s.assertionKind == ast::AssertionKind::Assume) {
        emitVoid(Op::Syscall, {Operand::str("$error"), Operand::str(b_.scope)}, sl);
    }
    jump(join, sl);
    setBlock(join);
}

void Lowerer::lowerCase(const ast::CaseStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    if (s.condition == ast::CaseStatementCondition::Inside) unsupported(sl, "case inside");
    // The expression is evaluated once, before the items; the items in
    // order until one matches (12.5). All take the width of the widest, and
    // are unsigned unless all are signed (VALUE_SEMANTICS.md 4.2).
    std::uint32_t width = s.expr.type->getBitWidth();
    bool allSigned = s.expr.type->isSigned();
    bool four = s.expr.type->isFourState();
    for (const auto& g : s.items) {
        for (const ast::Expression* e : g.expressions) {
            width = std::max(width, e->type->getBitWidth());
            allSigned = allSigned && e->type->isSigned();
            four = four || e->type->isFourState();
        }
    }
    if (width == 0 || width > 64) unsupported(sl, "values wider than 64 bits");
    const Type ct = four ? (allSigned ? sir::s4(width) : sir::u4(width))
                         : (allSigned ? sir::s2(width) : sir::u2(width));
    const Val x = coerce(lowerExpr(s.expr), ct, sl);
    std::vector<sir::BlockId> bodies;
    bodies.reserve(s.items.size());
    for (std::size_t g = 0; g < s.items.size(); ++g)
        bodies.push_back(newBlock());
    const auto match = [&](const ast::Expression& e) {
        const slang::SourceLocation isl = e.sourceRange.start();
        const Val v = coerce(lowerExpr(e), ct, isl);
        if (s.condition == ast::CaseStatementCondition::Normal)
            return emit(Op::Ceq, sir::u2(1), {x.op, v.op}, isl);
        const bool casex = s.condition == ast::CaseStatementCondition::WildcardXOrZ;
        return emit(Op::MatchMask, sir::u2(1),
                    {x.op, v.op, Operand::str(casex ? "casex" : "casez")}, isl);
    };
    if (s.check == ast::UniquePriorityCheck::None) {
        // The items in order until one matches.
        for (std::size_t g = 0; g < s.items.size(); ++g) {
            for (const ast::Expression* e : s.items[g].expressions) {
                const Val m = match(*e);
                const sir::BlockId next = newBlock();
                terminate(Op::BrIf, {m.op, Operand::block(bodies[g]), Operand::block(next)},
                          e->sourceRange.start());
                setBlock(next);
            }
        }
    } else {
        // unique, unique0, priority: every item and expression in order,
        // then the violation checks, then the first matching item
        // (SCHEDULER_SPEC.md 15.1).
        std::vector<Val> matches;
        for (const auto& g : s.items) {
            std::optional<Val> any;
            for (const ast::Expression* e : g.expressions) {
                const Val m = match(*e);
                any = any ? emit(Op::Or, sir::u2(1), {any->op, m.op}, sl) : m;
            }
            matches.push_back(any ? *any : constant(sir::u2(1), 0));
        }
        reportViolations(matches, s.check, s.defaultCase != nullptr, true, sl);
        for (std::size_t g = 0; g < s.items.size(); ++g) {
            const sir::BlockId next = newBlock();
            terminate(Op::BrIf, {matches[g].op, Operand::block(bodies[g]), Operand::block(next)},
                      sl);
            setBlock(next);
        }
    }
    std::vector<sir::BlockId> ends;
    if (s.defaultCase != nullptr) lowerStatement(*s.defaultCase);
    if (!b_.dead) ends.push_back(b_.block);
    for (std::size_t g = 0; g < s.items.size(); ++g) {
        setBlock(bodies[g]);
        lowerStatement(*s.items[g].stmt);
        if (!b_.dead) ends.push_back(b_.block);
    }
    if (ends.empty()) {
        b_.dead = true;
        return;
    }
    const sir::BlockId join = newBlock();
    for (const sir::BlockId e : ends)
        emitInto(e, Op::Br, {Operand::block(join)}, sl);
    setBlock(join);
}

void Lowerer::lowerTiming(const ast::TimingControl& t) {
    const slang::SourceLocation sl = t.sourceRange.start();
    switch (t.kind) {
        case ast::TimingControlKind::Delay: {
            const ast::Expression& e = t.as<ast::DelayControl>().expr;
            const std::optional<slang::ConstantValue> cv = constantValue(e);
            if (!cv) unsupported(sl, "delay that is not a constant");
            std::optional<std::uint64_t> ticks;
            if (cv->isInteger()) {
                // x or z: 0. Negative: read as an unsigned 64-bit time (9.4.1).
                const slang::SVInt& v = cv->integer();
                if (v.hasUnknown()) {
                    ticks = 0;
                } else {
                    const std::optional<std::int64_t> n = v.as<std::int64_t>();
                    if (n)
                        ticks = sir::delayTicks(static_cast<std::uint64_t>(*n), timeScale(),
                                                precision());
                }
            } else if (cv->isReal()) {
                ticks = sir::delayTicksReal(cv->real(), timeScale(), precision());
            }
            if (!ticks) unsupported(sl, "this delay value");
            const sir::BlockId cont = newBlock();
            terminate(Op::WaitDelay, {constant(sir::u2(64), *ticks).op, Operand::block(cont)}, sl);
            setBlock(cont);
            return;
        }
        case ast::TimingControlKind::SignalEvent:
        case ast::TimingControlKind::EventList: {
            std::vector<const ast::TimingControl*> items;
            if (t.kind == ast::TimingControlKind::SignalEvent)
                items.push_back(&t);
            else
                for (const ast::TimingControl* i : t.as<ast::EventListControl>().events)
                    items.push_back(i);
            std::vector<Operand> ops;
            for (const ast::TimingControl* i : items) {
                if (i->kind != ast::TimingControlKind::SignalEvent)
                    unsupported(sl, "this event control");
                const auto& se = i->as<ast::SignalEventControl>();
                if (se.iffCondition != nullptr) unsupported(sl, "event control with iff");
                const std::optional<sir::SlotRange> r = eventSlot(se.expr);
                if (!r) unsupported(sl, "event control on an expression");
                Operand o = Operand::slot(r->slot, r->lsb, r->width);
                o.firstElem = r->firstElem;
                o.numElems = r->numElems;
                switch (se.edge) {
                    case ast::EdgeKind::PosEdge: o.edge = sir::Edge::Pos; break;
                    case ast::EdgeKind::NegEdge: o.edge = sir::Edge::Neg; break;
                    case ast::EdgeKind::BothEdges: o.edge = sir::Edge::Both; break;
                    case ast::EdgeKind::None: o.edge = sir::Edge::Any; break;
                }
                ops.push_back(o);
            }
            const sir::BlockId cont = newBlock();
            ops.push_back(Operand::block(cont));
            terminate(Op::WaitEvent, std::move(ops), sl);
            setBlock(cont);
            return;
        }
        default: unsupported(sl, "timing control");
    }
}

}  // namespace kirimasim::front
