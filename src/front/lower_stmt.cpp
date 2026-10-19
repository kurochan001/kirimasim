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
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/statements/ConditionalStatements.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/types/Type.h"
#include "slang/syntax/AllSyntax.h"

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

slang::SourceLocation Lowerer::startOf(const ast::Statement& s) {
    if (s.kind == ast::StatementKind::Block && s.syntax != nullptr)
        return s.syntax->getFirstToken().location();
    return s.sourceRange.start();
}

void Lowerer::lowerStatement(const ast::Statement& s) {
    if (b_.dead) return;  // unreachable code is dropped
    const slang::SourceLocation sl = startOf(s);
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
            const bool named = b.blockSymbol != nullptr && !b.blockSymbol->name.empty();
            if (named) {
                b_.scope += "." + std::string(b.blockSymbol->name);
                Body::Named n{b.blockSymbol, std::nullopt, false, std::nullopt};
                // A block that disable reaches from another activation, or
                // that starts processes, is a scope: disable ends all its
                // activations at once (SIR_SPEC.md 7.2). One in a task or a
                // fork branch may be active in several processes.
                const bool inTask =
                    b_.isSub && design_.subroutines[b_.proc].kind == sir::SubroutineKind::Task;
                const bool inFork = !b_.isSub && proc().parent.kind != sir::Owner::Kind::None;
                if (disableTargets_.contains(b.blockSymbol) &&
                    (inTask || inFork || disabledFromOutside_.contains(b.blockSymbol) ||
                     blocksThatSpawn_.contains(b.blockSymbol))) {
                    n.scope = scopeOf(*b.blockSymbol);
                    n.exit = newBlock();
                    emitVoid(Op::ScopeEnter, {Operand::scope(*n.scope), Operand::block(*n.exit)},
                             sl);
                }
                b_.named.push_back(n);
            }
            lowerStatement(b.body);
            b_.scope = outer;
            if (named) {
                const Body::Named n = b_.named.back();
                b_.named.pop_back();
                if (n.scope.has_value() && n.exit.has_value()) {
                    if (!b_.dead) {
                        emitVoid(Op::ScopeLeave, {Operand::scope(*n.scope)}, sl);
                        terminate(Op::Br, {Operand::block(*n.exit)}, sl);
                    }
                    setBlock(*n.exit);
                } else if (n.exit) {
                    // A disable also ends what the block started (9.6.2).
                    if (n.spawns)
                        unsupported(sl, "disable of a block that starts processes it does not "
                                        "wait for");
                    if (!b_.dead) terminate(Op::Br, {Operand::block(*n.exit)}, sl);
                    setBlock(*n.exit);
                }
            }
            return;
        }
        case ast::StatementKind::VariableDeclaration: {
            const auto& v = s.as<ast::VariableDeclStatement>().symbol;
            // The default initial value (6.8, table 6-7): x, 0, 0.0 or "".
            const auto initial = [&](Type t) {
                if (t.kind == sir::TypeKind::String) return stringConstant("");
                return sir::isFourState(t) ? allXConstant(t) : constant(t, 0);
            };
            if (structElement(v.getType()) != nullptr) {
                // One slot or frame variable per member (SIR_SPEC.md 4.2),
                // set from the initializer or else the members' defaults
                // (7.2.2).
                const ast::Expression* init = v.getInitializer();
                if (v.lifetime == ast::VariableLifetime::Static) {
                    const sir::SlotId first = structSlots(v, sl);
                    if (init != nullptr || hasMemberDefaults(v.getType()))
                        lowerStaticInit(v, first, sl);
                    return;
                }
                const sir::FrameVarId first = structFrameVars(v, sl);
                const std::uint32_t n = leafCount(v.getType());
                for (std::uint32_t k = 0; k < n; ++k)
                    emitVoid(Op::Fstore,
                             {Operand::frame(first + k), initial(frameVars()[first + k].type).op},
                             sl);
                Stores stores;
                Storage dst;
                dst.frame = true;
                dst.id = first;
                if (init != nullptr)
                    assignAggregate(dst, v.getType(), *init, stores, sl);
                else
                    memberDefaults(dst, v.getType(), stores, sl);
                flushStores(stores, false, sl);
                return;
            }
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
            if (!frameVars()[f].dims.empty()) {
                // An array: its initializer element by element, or else
                // every element's default.
                const ArrayBase a{Place::Home::Frame, f};
                Stores stores;
                if (init != nullptr) {
                    assignArray(a, {}, *init, stores, sl);
                } else {
                    std::vector<Place> places;
                    arrayPlaces(a, {}, places);
                    for (Place& e : places)
                        stores.emplace_back(std::move(e), initial(t));
                }
                flushStores(stores, false, sl);
                return;
            }
            const Val value = init != nullptr ? coerce(lowerExpr(*init), t, sl) : initial(t);
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
            b_.loops.push_back(Loop{.named = b_.named.size()});
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
            b_.loops.push_back(Loop{.named = b_.named.size()});
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
            b_.loops.push_back(Loop{.named = b_.named.size()});
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
            b_.loops.push_back(Loop{.named = b_.named.size()});
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
            b_.loops.push_back(Loop{.named = b_.named.size()});
            b_.loops[loop].continueTo = body;
            lowerStatement(s.as<ast::ForeverLoopStatement>().body);
            jump(body, sl);
            endLoop(loop);
            return;
        }
        case ast::StatementKind::DisableFork: emitVoid(Op::DisableFork, {}, sl); return;
        case ast::StatementKind::Return: lowerReturn(s.as<ast::ReturnStatement>().expr, sl); return;
        case ast::StatementKind::EventTrigger: {
            // -> wakes the processes waiting now; ->> triggers in the NBA
            // region after its delay (SCHEDULER_SPEC.md 9.4).
            const auto& et = s.as<ast::EventTriggerStatement>();
            std::vector<Operand> ops{eventOperand(et.target, sl)};
            if (!et.isNonBlocking) {
                emitVoid(Op::Trigger, std::move(ops), sl);
                return;
            }
            if (et.timing != nullptr) {
                if (et.timing->kind != ast::TimingControlKind::Delay)
                    unsupported(sl, "->> with an event control");
                ops.push_back(delayValue(et.timing->as<ast::DelayControl>().expr, sl).op);
            }
            emitVoid(Op::TriggerNb, std::move(ops), sl);
            return;
        }
        case ast::StatementKind::Disable: lowerDisable(s.as<ast::DisableStatement>()); return;
        case ast::StatementKind::Wait: lowerWait(s.as<ast::WaitStatement>()); return;
        case ast::StatementKind::WaitFork: {
            const sir::BlockId cont = newBlock();
            terminate(Op::WaitFork, {Operand::block(cont)}, sl);
            setBlock(cont);
            return;
        }
        case ast::StatementKind::ProceduralAssign:
            lowerOverride(s.as<ast::ProceduralAssignStatement>());
            return;
        case ast::StatementKind::ProceduralDeassign: {
            // deassign and release end the holds of their level on the
            // targets (SCHEDULER_SPEC.md 14, SIR_SPEC.md 5.6).
            const auto& d = s.as<ast::ProceduralDeassignStatement>();
            std::vector<Operand> ops{Operand::str(d.isRelease ? "force" : "assign")};
            for (const HeldTarget& h : heldTargets(d.lvalue, sl))
                ops.push_back(h.range);
            emitVoid(Op::OverrideStop, std::move(ops), sl);
            return;
        }
        case ast::StatementKind::Break:
            if (b_.loops.empty()) unsupported(sl, "break outside a loop");
            leaveScopes(b_.loops.back().named, sl);
            terminate(Op::Br, {Operand::block(breakTarget(b_.loops.size() - 1))}, sl);
            return;
        case ast::StatementKind::Continue:
            if (b_.loops.empty()) unsupported(sl, "continue outside a loop");
            leaveScopes(b_.loops.back().named, sl);
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
    b_.loops.push_back(Loop{.named = b_.named.size()});
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
    const slang::SourceLocation sl = startOf(b);
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
    // Children not waited for may outlive the named blocks around the fork.
    if (b.blockKind != ast::StatementBlockKind::JoinAll)
        for (Body::Named& n : b_.named)
            n.spawns = true;
    std::string scope = b_.scope;  // a named fork is a scope (9.3.5)
    if (b.blockSymbol != nullptr && !b.blockSymbol->name.empty())
        scope += "." + std::string(b.blockSymbol->name);
    // The parent is the process, or the task, whose body holds the fork.
    const std::string owner = b_.isSub ? design().subroutines[b_.proc].name : proc().name;
    const sir::InstanceId instance =
        b_.isSub ? design().subroutines[b_.proc].instance : proc().instance;
    const sir::Owner::Kind ownerKind =
        b_.isSub ? sir::Owner::Kind::Subroutine : sir::Owner::Kind::Process;
    std::vector<Operand> ops;
    for (std::size_t i = 0; i < stmts.size(); ++i) {
        if (stmts[i]->kind == ast::StatementKind::VariableDeclaration)
            unsupported(stmts[i]->sourceRange.start(), "declarations in a fork block");
        if (stmts[i]->kind == ast::StatementKind::Empty) continue;
        sir::Process child;
        child.name = owner + ".fork" + std::to_string(forkNumber) + "." + std::to_string(i);
        child.kind = sir::ProcessKind::General;
        child.instance = instance;
        child.parent = {ownerKind, parent};
        child.loc = loc(startOf(*stmts[i]));
        child.blocks.resize(1);
        const auto id = static_cast<std::uint32_t>(design().processes.size());
        design().processes.push_back(std::move(child));
        Body saved = std::move(b_);
        b_ = Body{};
        b_.proc = id;
        b_.scale = saved.scale;
        b_.scope = scope;
        b_.outermostDisabled = disabledBlock(*stmts[i]);
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

void Lowerer::lowerDisable(const ast::DisableStatement& d) {
    const slang::SourceLocation sl = d.sourceRange.start();
    const ast::Expression& t = d.target;
    const ast::Symbol* sym = t.kind == ast::ExpressionKind::ArbitrarySymbol
                                 ? t.as<ast::ArbitrarySymbolExpression>().symbol.get()
                                 : nullptr;
    if (sym == nullptr) unsupported(sl, "disable of this target");
    if (sym->kind == ast::SymbolKind::Subroutine) {
        // Every call of the task ends (9.6.2).
        const sir::SubroutineId id = subroutineOf(sym->as<ast::SubroutineSymbol>(), sl);
        emitVoid(Op::DisableTask, {Operand::subroutine(id)}, sl);
        return;
    }
    if (sym->kind != ast::SymbolKind::StatementBlock ||
        sym->as<ast::StatementBlockSymbol>().blockKind != ast::StatementBlockKind::Sequential)
        unsupported(sl, "disable of this block");
    std::size_t at = b_.named.size();
    for (std::size_t i = b_.named.size(); i-- > 0 && at == b_.named.size();)
        if (b_.named[i].sym == sym) at = i;
    if (at != b_.named.size() && !b_.named[at].scope) {
        // A block around it in this body that nothing else reaches: go to
        // its end, leaving the scopes in between (SIR_SPEC.md 7.2).
        Body::Named& n = b_.named[at];
        leaveScopes(at + 1, sl);
        if (!n.exit) n.exit = newBlock();
        terminate(Op::Br, {Operand::block(*n.exit)}, sl);
        return;
    }
    emitVoid(Op::DisableScope, {Operand::scope(scopeOf(*sym))}, sl);
    // A block around it ends here; the kernel goes on at its exit.
    if (at == b_.named.size()) return;
    if (const std::optional<sir::BlockId> exit = b_.named[at].exit)
        terminate(Op::Br, {Operand::block(*exit)}, sl);
}

std::uint32_t Lowerer::scopeOf(const ast::Symbol& block) {
    if (const auto it = scopeIds_.find(&block); it != scopeIds_.end()) return it->second;
    const std::uint32_t id = design_.numScopes++;
    scopeIds_.emplace(&block, id);
    return id;
}

void Lowerer::leaveScopes(std::size_t from, slang::SourceLocation sl) {
    for (std::size_t i = b_.named.size(); i-- > from;)
        if (const std::optional<std::uint32_t> scope = b_.named[i].scope)
            emitVoid(Op::ScopeLeave, {Operand::scope(*scope)}, sl);
}

void Lowerer::scanDisables(const ast::RootSymbol& root) {
    // The named blocks around the statement being visited, in its body: a
    // procedure, a subroutine or a fork branch. A disable of a block ends
    // every process started in it, with their descendants (9.6.2): a block
    // where a process may start is one that spawns, and so is not left
    // with a plain jump.
    std::vector<std::vector<const ast::Symbol*>> bodies(1);
    std::size_t spawns = 0;
    const auto spawn = [&] {
        ++spawns;
        for (const ast::Symbol* b : bodies.back())
            blocksThatSpawn_.insert(b);
    };
    auto visitor = ast::makeVisitor(
        [&](auto& v, const ast::ProceduralBlockSymbol& pb) {
            bodies.emplace_back();
            v.visitDefault(pb);
            bodies.pop_back();
        },
        [&](auto& v, const ast::SubroutineSymbol& sub) {
            bodies.emplace_back();
            v.visitDefault(sub);
            bodies.pop_back();
        },
        [&](auto& v, const ast::BlockStatement& b) {
            if (b.blockKind == ast::StatementBlockKind::Sequential) {
                const bool named = b.blockSymbol != nullptr && !b.blockSymbol->name.empty();
                if (named) bodies.back().push_back(b.blockSymbol);
                v.visitDefault(b);
                if (named) bodies.back().pop_back();
                return;
            }
            // Each statement of a fork is a process of its own (9.3.2).
            const std::size_t before = spawns;
            const auto branch = [&](const ast::Statement& st) {
                bodies.emplace_back();
                st.visit(v);
                bodies.pop_back();
            };
            if (b.body.kind == ast::StatementKind::List) {
                for (const ast::Statement* item : b.body.as<ast::StatementList>().list)
                    branch(*item);
            } else {
                branch(b.body);
            }
            // join_any and join_none leave children running; a join leaves
            // what its branches started without waiting (9.6.1).
            if (b.blockKind != ast::StatementBlockKind::JoinAll || spawns != before) spawn();
        },
        [&](auto& v, const ast::DisableStatement& d) {
            if (d.target.kind == ast::ExpressionKind::ArbitrarySymbol) {
                const ast::Symbol* sym = d.target.as<ast::ArbitrarySymbolExpression>().symbol.get();
                disableTargets_.insert(sym);
                const auto& around = bodies.back();
                if (std::find(around.begin(), around.end(), sym) == around.end())
                    disabledFromOutside_.insert(sym);
            }
            v.visitDefault(d);
        },
        [&](auto& v, const ast::CallExpression& c) {
            // A task may start processes it does not wait for, and so may a
            // function with a fork-join_none (13.4.4).
            if (!c.isSystemCall()) {
                const ast::SubroutineSymbol& sub = *std::get<0>(c.subroutine);
                if (sub.subroutineKind == ast::SubroutineKind::Task || mayFork(sub)) spawn();
            }
            v.visitDefault(c);
        });
    root.visit(visitor);
}

bool Lowerer::mayFork(const ast::SubroutineSymbol& sub) {
    if (const auto it = mayFork_.find(&sub); it != mayFork_.end()) return it->second;
    mayFork_[&sub] = false;  // a recursive call adds nothing
    bool forks = false;
    auto visitor = ast::makeVisitor(
        [&](auto& v, const ast::BlockStatement& b) {
            if (b.blockKind != ast::StatementBlockKind::Sequential) forks = true;
            v.visitDefault(b);
        },
        [&](auto& v, const ast::CallExpression& c) {
            if (!c.isSystemCall() && mayFork(*std::get<0>(c.subroutine))) forks = true;
            v.visitDefault(c);
        });
    sub.visit(visitor);
    mayFork_[&sub] = forks;
    return forks;
}

void Lowerer::lowerWait(const ast::WaitStatement& w) {
    // wait.cond goes on at once when the guard is true, and else when it
    // becomes true (9.4.3, SIR_SPEC.md 7.2).
    const slang::SourceLocation sl = w.sourceRange.start();
    const sir::SubroutineId id = guardOf(w.cond, "wait", sl);
    const sir::BlockId cont = newBlock();
    terminate(Op::WaitCond, {Operand::subroutine(id), Operand::block(cont)}, sl);
    setBlock(cont);
    lowerStatement(w.stmt);
}

sir::SubroutineId Lowerer::guardOf(const ast::Expression& cond, const char* what,
                                   slang::SourceLocation sl) {
    return waiterOf(sir::SubroutineKind::Guard, cond, what, sl);
}

sir::SubroutineId Lowerer::watchOf(const ast::Expression& e, slang::SourceLocation sl) {
    return waiterOf(sir::SubroutineKind::Watch, e, "event", sl);
}

sir::SubroutineId Lowerer::waiterOf(sir::SubroutineKind kind, const ast::Expression& e,
                                    const char* what, slang::SourceLocation sl) {
    const bool guard = kind == sir::SubroutineKind::Guard;
    const sir::Owner owner{b_.isSub ? sir::Owner::Kind::Subroutine : sir::Owner::Kind::Process,
                           b_.proc};
    sir::Subroutine g;
    g.name = (b_.isSub ? design_.subroutines[b_.proc].name : proc().name) + "." + what +
             std::to_string(b_.guards++);
    g.kind = kind;
    g.instance = b_.isSub ? design_.subroutines[b_.proc].instance : proc().instance;
    g.frameOf = owner;
    g.returnType = guard ? sir::u4(1) : typeOf(*e.type, sl);
    g.loc = loc(sl);
    g.blocks.resize(1);
    const auto id = static_cast<sir::SubroutineId>(design_.subroutines.size());
    design_.subroutines.push_back(std::move(g));
    Body saved = std::move(b_);
    b_ = Body{};
    b_.isSub = true;
    b_.proc = id;
    b_.subSymbol = saved.subSymbol;  // the variables of a waiting task are its own
    b_.frameVars = saved.frameVars;
    b_.structFrames = saved.structFrames;
    b_.scale = saved.scale;
    b_.scope = saved.scope;
    try {
        const sir::Type rt = design_.subroutines[id].returnType;
        const Val v = guard ? lowerCondition(e) : lowerExpr(e);
        terminate(Op::Ret, {coerce(v, rt, sl).op}, sl);
    } catch (const Unsupported&) {
        b_ = std::move(saved);
        throw;
    }
    b_ = std::move(saved);
    // What the guard's calls write counts as the waiting body's.
    callees_[{b_.isSub, b_.proc}].push_back(id);
    // The kernel evaluates the condition in the middle of the write that
    // changed what it reads (SCHEDULER_SPEC.md 9.5) or that made the
    // event, so it may not write a variable or a net itself.
    std::set<sir::SubroutineId> seen;
    if (writesOutside(id, seen))
        unsupported(sl, guard ? std::string(what) + " condition that writes a variable or net"
                              : std::string("event expression that writes a variable or net"));
    return id;
}

bool Lowerer::writesOutside(sir::SubroutineId id, std::set<sir::SubroutineId>& seen) const {
    if (!seen.insert(id).second) return false;
    if (const auto it = subWrites_.find(id); it != subWrites_.end())
        for (const sir::SlotRange& r : it->second)
            if (design_.slots[r.slot].subroutine == sir::kNone) return true;
    if (const auto it = callees_.find({true, id}); it != callees_.end())
        for (const sir::SubroutineId c : it->second)
            if (writesOutside(c, seen)) return true;
    return false;
}

std::vector<Lowerer::HeldTarget> Lowerer::heldTargets(const ast::Expression& lhs,
                                                      slang::SourceLocation sl) {
    // The places of a target of assign, force, deassign and release: a
    // variable, a net, a constant part of a net, or a concatenation of
    // them (10.6.1, 10.6.2), most significant first. slang rejects the
    // others. Each slot is marked as a force target (SCHEDULER_SPEC.md
    // 14.4).
    std::vector<HeldTarget> out;
    const auto add = [&](const Place& p, auto& self) -> void {
        if (p.kind == Place::Kind::Concat) {
            for (const Place& part : p.base)
                self(part, self);
            return;
        }
        if (p.kind == Place::Kind::Frame)
            unsupported(sl, "an automatic variable as the target of assign or force");
        // A whole slot, or an element or bits of one: the other places have
        // no slot to look up.
        const bool ofSlot = p.kind == Place::Kind::Slot ||
                            ((p.kind == Place::Kind::Element || p.kind == Place::Kind::Bits) &&
                             p.home == Place::Home::Slot);
        if (!ofSlot) unsupported(sl, "this target of assign or force");
        sir::Slot& slot = design_.slots[p.slot];
        if (!sir::isIntegral(slot.type))
            unsupported(sl, "assign or force of a value that is not integral");
        // A net merged with another through a port would be held on both
        // sides (SCHEDULER_SPEC.md 14.4).
        if (mergedSlots_.contains(p.slot))
            unsupported(sl, "assign or force of a net connected through a port");
        Operand o = Operand::slot(p.slot);
        std::uint32_t w = slot.type.width;
        sir::Type type = slot.type;
        if ((p.kind == Place::Kind::Element || p.kind == Place::Kind::Bits) && !p.indices.empty()) {
            if (!p.elem) unsupported(sl, "an element selected at run time here");
            o.firstElem = *p.elem;
            o.numElems = 1;
        }
        if (p.kind == Place::Kind::Bits) {
            if (p.pos || p.lsb < 0 || p.lsb + p.width > w)
                unsupported(sl, "a part selected at run time or outside the net here");
            o.lsb = static_cast<std::uint32_t>(p.lsb);
            o.width = p.width;
            w = p.width;
            type = sir::isFourState(slot.type) ? sir::u4(w) : sir::u2(w);
        } else if (!slot.dims.empty() && o.numElems != 1) {
            unsupported(sl, "assign or force of a whole unpacked array");
        }
        slot.flags |= sir::kForceTarget;
        out.push_back({o, type});
    };
    add(placeOf(lhs, sl), add);
    return out;
}

void Lowerer::lowerOverride(const ast::ProceduralAssignStatement& s) {
    // assign and force (10.6.1, 10.6.2): an override process computes the
    // right side and writes the targets it holds, now and whenever what it
    // reads changes (SIR_SPEC.md 5.6, 7.1).
    const slang::SourceLocation sl = s.sourceRange.start();
    if (s.assignment.kind != ast::ExpressionKind::Assignment)
        unsupported(sl, "this procedural continuous assignment");
    const auto& a = s.assignment.as<ast::AssignmentExpression>();
    const std::string level = s.isForce ? "force" : "assign";
    const std::vector<HeldTarget> targets = heldTargets(a.left(), sl);
    const sir::InstanceId inst = b_.isSub ? design_.subroutines[b_.proc].instance : proc().instance;
    sir::Process p;
    p.name = processName(design_.instances[inst].scopeName, "$" + level);
    p.kind = sir::ProcessKind::Override;
    p.instance = inst;
    p.loc = loc(sl);
    p.blocks.resize(1);
    const auto id = static_cast<std::uint32_t>(design_.processes.size());
    design_.processes.push_back(std::move(p));
    Body saved = std::move(b_);
    b_ = Body{};
    b_.proc = id;
    b_.scale = saved.scale;
    b_.scope = saved.scope;
    try {
        std::uint32_t total = 0;
        bool four = false;
        for (const HeldTarget& h : targets) {
            total += h.type.width;
            four = four || sir::isFourState(h.type);
        }
        const Val all = coerce(lowerExpr(a.right()), four ? sir::u4(total) : sir::u2(total), sl);
        std::uint32_t lsb = total;
        for (const HeldTarget& h : targets) {
            lsb -= h.type.width;
            Val part = all;
            if (targets.size() > 1)
                part = emit(Op::Partsel, four ? sir::u4(h.type.width) : sir::u2(h.type.width),
                            {all.op, constant(sir::u2(32), lsb).op}, sl);
            part = coerce(part, h.type, sl);
            emitVoid(Op::StoreOverride, {Operand::str(level), h.range, part.op}, sl);
            noteWrite(
                {h.range.id, h.range.firstElem, h.range.numElems, h.range.lsb, h.range.width});
        }
        terminate(Op::Ret, {}, sl);
        triggerOnReads();
    } catch (const Unsupported&) {
        b_ = std::move(saved);
        throw;
    }
    b_ = std::move(saved);
    std::vector<Operand> ops{Operand::str(level), Operand::process(id)};
    for (const HeldTarget& h : targets)
        ops.push_back(h.range);
    emitVoid(Op::OverrideStart, std::move(ops), sl);
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
        emitVoid(Op::ReportDefer,
                 {Operand::str(construct), Operand::str(content), Operand::str(b_.scope)}, sl);
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
    if (s.isDeferred) {
        // The expression is evaluated now; the action goes on the process's
        // queue of pending reports, with the values it passes (16.4,
        // SCHEDULER_SPEC.md 15.2, SIR_SPEC.md 5.7).
        if (b_.fromAssertion) unsupported(sl, "deferred assertion written as a module item");
        const Val c = lowerCondition(s.cond);
        const sir::BlockId pass = newBlock();
        const sir::BlockId fail = newBlock();
        const sir::BlockId join = newBlock();
        terminate(Op::BrIf, {c.op, Operand::block(pass), Operand::block(fail)}, sl);
        // An empty action does nothing; a missing else of assert and
        // assume calls $error.
        const auto acts = [](const ast::Statement* a) {
            return a != nullptr && a->kind != ast::StatementKind::Empty;
        };
        setBlock(pass);
        if (acts(s.ifTrue)) deferAction(s.ifTrue, s.isFinal, sl);
        jump(join, sl);
        setBlock(fail);
        if (acts(s.ifFalse) ||
            (s.ifFalse == nullptr && (s.assertionKind == ast::AssertionKind::Assert ||
                                      s.assertionKind == ast::AssertionKind::Assume)))
            deferAction(s.ifFalse, s.isFinal, sl);
        jump(join, sl);
        setBlock(join);
        return;
    }
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

bool Lowerer::disabledBlock(const ast::Statement& s) const {
    if (s.kind != ast::StatementKind::Block) return false;
    const auto& b = s.as<ast::BlockStatement>();
    return b.blockSymbol != nullptr && disableTargets_.contains(b.blockSymbol);
}

void Lowerer::deferAction(const ast::Statement* action, bool final, slang::SourceLocation sl) {
    // The queue would also be dropped when the process's outermost block is
    // disabled (SCHEDULER_SPEC.md 15.3).
    if (b_.outermostDisabled)
        unsupported(sl, "deferred assertion in a process whose outermost block is disabled");
    // The action is one subroutine call (16.4): a function S runs it later
    // with the values passed by value, evaluated now. Outputs, inouts and
    // refs are written and read when S runs.
    const ast::CallExpression* call = nullptr;
    if (action != nullptr) {
        if (action->kind == ast::StatementKind::ExpressionStatement &&
            action->as<ast::ExpressionStatement>().expr.kind == ast::ExpressionKind::Call)
            call = &action->as<ast::ExpressionStatement>().expr.as<ast::CallExpression>();
        if (call == nullptr) unsupported(sl, "this action of a deferred assertion");
        if (!call->isSystemCall() &&
            design_.subroutines[subroutineOf(*std::get<0>(call->subroutine), sl)].mayWait)
            unsupported(sl, "deferred assertion whose action is a task that may wait");
    }
    std::vector<const ast::Expression*> passed;
    std::vector<Val> values;
    if (call != nullptr) {
        const auto args = call->arguments();
        for (std::size_t i = 0; i < args.size(); ++i) {
            const ast::Expression* a = args[i];
            bool byValue = true;
            if (!call->isSystemCall()) {
                const auto formals = std::get<0>(call->subroutine)->getArguments();
                byValue = i < formals.size() && formals[i]->direction == ast::ArgumentDirection::In;
            }
            if (!byValue || a->kind == ast::ExpressionKind::StringLiteral || constantValue(*a)) {
                if (readsAutomatic(*a))
                    unsupported(sl, "deferred assertion action with an automatic variable as its "
                                    "output, inout or ref argument");
                continue;
            }
            passed.push_back(a);
            values.push_back(lowerExpr(*a));
        }
    }
    const std::string owner = b_.isSub ? design_.subroutines[b_.proc].name : proc().name;
    sir::Subroutine fn;
    fn.name = owner + ".$deferred" + std::to_string(b_.deferred++);
    fn.kind = sir::SubroutineKind::Function;
    fn.automatic = true;
    fn.instance = b_.isSub ? design_.subroutines[b_.proc].instance : proc().instance;
    fn.returnType = sir::voidType();
    fn.loc = loc(sl);
    fn.blocks.resize(1);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto fv = static_cast<sir::FrameVarId>(i);
        fn.frame.push_back({"", values[i].type, sir::kNone, {}, {}});
        fn.params.push_back(
            {"$" + std::to_string(i), sir::Direction::Input, values[i].type, sir::kNone, fv});
    }
    const auto id = static_cast<sir::SubroutineId>(design_.subroutines.size());
    design_.subroutines.push_back(std::move(fn));
    callees_[{b_.isSub, b_.proc}].push_back(id);
    Body saved = std::move(b_);
    b_ = Body{};
    b_.isSub = true;
    b_.proc = id;
    b_.scale = saved.scale;
    b_.scope = saved.scope;
    try {
        for (std::size_t i = 0; i < passed.size(); ++i)
            substitutions_[passed[i]] = emit(Op::Fload, values[i].type,
                                             {Operand::frame(static_cast<sir::FrameVarId>(i))}, sl);
        if (call == nullptr)
            emitVoid(Op::Syscall, {Operand::str("$error"), Operand::str(b_.scope)}, sl);
        else if (call->isSystemCall())
            lowerSystemTask(*call);
        else
            lowerCall(*call);
        terminate(Op::Ret, {}, sl);
    } catch (const Unsupported&) {
        substitutions_.clear();
        b_ = std::move(saved);
        throw;
    }
    substitutions_.clear();
    b_ = std::move(saved);
    std::vector<Operand> ops{Operand::str(final ? "final" : "#0"), Operand::subroutine(id)};
    for (const Val& v : values)
        ops.push_back(v.op);
    emitVoid(Op::ReportCall, std::move(ops), sl);
}

void Lowerer::lowerCase(const ast::CaseStatement& s) {
    const slang::SourceLocation sl = s.sourceRange.start();
    // The expression is evaluated once, before the items; the items in
    // order until one matches (12.5). All take the width of the widest, and
    // are unsigned unless all are signed (VALUE_SEMANTICS.md 4.2). An item
    // of case inside matches as the set of an inside operator does, and
    // only when that gives 1 (12.5.4). slang has given the expression and
    // the items their common type; a real, shortreal or string one is
    // compared with == in that type, not bit by bit (VALUE_SEMANTICS.md
    // 4.2).
    const bool inside = s.condition == ast::CaseStatementCondition::Inside;
    const ast::Type& common = s.expr.type->getCanonicalType();
    const bool byValue = !inside && (common.isFloating() || common.isString());
    std::uint32_t width = s.expr.type->getBitWidth();
    bool allSigned = s.expr.type->isSigned();
    bool four = s.expr.type->isFourState();
    for (const auto& g : s.items) {
        for (const ast::Expression* e : g.expressions) {
            if (inside) break;
            width = std::max(width, e->type->getBitWidth());
            allSigned = allSigned && e->type->isSigned();
            four = four || e->type->isFourState();
        }
    }
    if (!inside && !byValue && width == 0) unsupported(sl, "a value of no bits");
    const Type ct = four ? (allSigned ? sir::s4(width) : sir::u4(width))
                         : (allSigned ? sir::s2(width) : sir::u2(width));
    const Val x = inside || byValue ? lowerExpr(s.expr) : coerce(lowerExpr(s.expr), ct, sl);
    std::vector<sir::BlockId> bodies;
    bodies.reserve(s.items.size());
    for (std::size_t g = 0; g < s.items.size(); ++g)
        bodies.push_back(newBlock());
    const auto match = [&](const ast::Expression& e) {
        const slang::SourceLocation isl = e.sourceRange.start();
        if (inside) {
            const ast::Expression* const item[] = {&e};
            return coerce(lowerInside(x, item, isl), sir::u2(1), isl);
        }
        if (byValue) return compareValues(Op::Eq, x, lowerExpr(e), isl);
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
            const Val ticks = delayValue(t.as<ast::DelayControl>().expr, sl);
            const sir::BlockId cont = newBlock();
            terminate(Op::WaitDelay, {ticks.op, Operand::block(cont)}, sl);
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
                if (se.expr.type->isEvent()) {
                    // @ev: wait for the next trigger of a named event
                    // (SCHEDULER_SPEC.md 9.4).
                    if (items.size() != 1) unsupported(sl, "event list with a named event");
                    if (se.iffCondition != nullptr) unsupported(sl, "named event with iff");
                    const sir::BlockId cont = newBlock();
                    terminate(Op::WaitNamed, {eventOperand(se.expr, sl), Operand::block(cont)}, sl);
                    setBlock(cont);
                    return;
                }
                Operand o;
                if (const std::optional<sir::SlotRange> r = eventSlot(se.expr)) {
                    o = Operand::slot(r->slot, r->lsb, r->width);
                    o.firstElem = r->firstElem;
                    o.numElems = r->numElems;
                } else {
                    // An expression: a watch, which the layers make a
                    // monitor record (SIR_SPEC.md 7.2).
                    if (!se.expr.type->isIntegral())
                        unsupported(sl, "event control on an expression of this type");
                    o = Operand::subroutine(watchOf(se.expr, se.expr.sourceRange.start()));
                }
                switch (se.edge) {
                    case ast::EdgeKind::PosEdge: o.edge = sir::Edge::Pos; break;
                    case ast::EdgeKind::NegEdge: o.edge = sir::Edge::Neg; break;
                    case ast::EdgeKind::BothEdges: o.edge = sir::Edge::Both; break;
                    case ast::EdgeKind::None: o.edge = sir::Edge::Any; break;
                }
                ops.push_back(o);
                // iff: a guard after its item, evaluated when that item's
                // event occurs and only then (9.4.2.3, SIR_SPEC.md 7.2).
                if (const ast::Expression* c = se.iffCondition; c != nullptr)
                    ops.push_back(Operand::subroutine(guardOf(*c, "iff", c->sourceRange.start())));
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
