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
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/statements/ConditionalStatements.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/types/Type.h"

namespace haremasim::front {

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
            lowerStatement(b.body);
            return;
        }
        case ast::StatementKind::VariableDeclaration: {
            const auto& v = s.as<ast::VariableDeclStatement>().symbol;
            if (v.lifetime == ast::VariableLifetime::Static) {
                // Static: a slot, set once at time 0 (6.21), which needs the
                // static initialization that comes later.
                if (v.getInitializer() != nullptr)
                    unsupported(sl, "initial value of a static variable");
                slotOf(v, sl);
                return;
            }
            // Automatic: a frame variable, set each time the scope is
            // entered (SIR_SPEC.md 7.4).
            const sir::FrameVarId f = frameVarOf(v, sl);
            const Type t = proc().frame[f].type;
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
            if (e.kind == ast::ExpressionKind::Call && e.as<ast::CallExpression>().isSystemCall()) {
                lowerSystemTask(e.as<ast::CallExpression>());
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
        case ast::StatementKind::ForeverLoop:
            // Needs the default zero-time limits first (CLI.md 6), or a
            // forever loop that never waits runs without end.
            unsupported(sl, "forever loop");
        case ast::StatementKind::DisableFork: emitVoid(Op::DisableFork, {}, sl); return;
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
    const Type t = proc().frame[fv].type;
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

void Lowerer::lowerIf(const ast::ConditionalStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    if (s.check != ast::UniquePriorityCheck::None) unsupported(sl, "unique or priority if");
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

void Lowerer::lowerCase(const ast::CaseStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    if (s.check != ast::UniquePriorityCheck::None) unsupported(sl, "unique or priority case");
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
    for (std::size_t g = 0; g < s.items.size(); ++g) {
        for (const ast::Expression* e : s.items[g].expressions) {
            const slang::SourceLocation isl = e->sourceRange.start();
            const Val v = coerce(lowerExpr(*e), ct, isl);
            Val m;
            if (s.condition == ast::CaseStatementCondition::Normal) {
                m = emit(Op::Ceq, sir::u2(1), {x.op, v.op}, isl);
            } else {
                const bool casex = s.condition == ast::CaseStatementCondition::WildcardXOrZ;
                m = emit(Op::MatchMask, sir::u2(1),
                         {x.op, v.op, Operand::str(casex ? "casex" : "casez")}, isl);
            }
            const sir::BlockId next = newBlock();
            terminate(Op::BrIf, {m.op, Operand::block(bodies[g]), Operand::block(next)}, isl);
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

}  // namespace haremasim::front
