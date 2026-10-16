// Lowering of functions and tasks, their calls and return (SIR_SPEC.md 8).
//
// A subroutine is lowered once per instance that calls it, when it is first
// called. Static subroutines keep their arguments and variables in slots,
// automatic ones in the frame of each call (SIR_SPEC.md 3.3). Arguments are
// passed by value: inputs are copied in, outputs and inouts copied back
// after the call like blocking assignments (4.9.7). An output whose actual
// is not a whole variable of the formal's type goes through a frame
// variable of the caller, then is assigned with the usual store.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lowerer.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/symbols/SubroutineSymbols.h"
#include "slang/ast/types/Type.h"

namespace kirimasim::front {

namespace {

using sir::Op;
using sir::Operand;
using sir::Type;

sir::Direction directionOf(ast::ArgumentDirection d) {
    switch (d) {
        case ast::ArgumentDirection::In: return sir::Direction::Input;
        case ast::ArgumentDirection::Out: return sir::Direction::Output;
        case ast::ArgumentDirection::InOut: return sir::Direction::Inout;
        case ast::ArgumentDirection::Ref: return sir::Direction::Ref;
    }
    return sir::Direction::Input;
}

/// The lvalue an output or inout actual argument names.
const ast::Expression* actualPlace(const ast::Expression* arg) {
    if (arg->kind == ast::ExpressionKind::Assignment) {
        const auto& a = arg->as<ast::AssignmentExpression>();
        if (a.isLValueArg()) return &a.left();
    }
    return arg;
}

}  // namespace

sir::SubroutineId Lowerer::subroutineOf(const ast::SubroutineSymbol& sym,
                                        slang::SourceLocation sl) {
    // A subroutine of a package or compilation unit is one in the design
    // and belongs to its row (SIR_SPEC.md 3.3); the others, to the
    // instance they are lowered for.
    const std::optional<sir::InstanceId> row = rowOf(sym);
    const sir::InstanceId owner = row.value_or(instance_);
    const auto key = std::make_pair(owner, static_cast<const ast::Symbol*>(&sym));
    if (auto it = subs_.find(key); it != subs_.end()) return it->second;

    sir::Subroutine s;
    s.name = design_.instances[owner].scopeName + (row ? "::" : ".") + std::string(sym.name);
    s.kind = sym.subroutineKind == ast::SubroutineKind::Function ? sir::SubroutineKind::Function
                                                                 : sir::SubroutineKind::Task;
    s.automatic = sym.defaultLifetime == ast::VariableLifetime::Automatic;
    s.instance = owner;
    s.loc = loc(sym.location);
    s.blocks.resize(1);
    const ast::Type& ret = sym.getReturnType();
    s.returnType = ret.isVoid() ? sir::voidType() : typeOf(ret, sl);
    // A task that may wait is called with call.task (SIR_SPEC.md 7.2, 8).
    s.mayWait = s.kind == sir::SubroutineKind::Task && mayWait(sym.getBody());
    const auto id = static_cast<sir::SubroutineId>(design_.subroutines.size());
    design_.subroutines.push_back(std::move(s));
    subs_.emplace(key, id);  // before the body, so that recursion finds it

    Body saved = std::move(b_);
    const ast::Symbol* savedEval = evalSymbol_;
    const sir::InstanceId savedInstance = instance_;
    instance_ = owner;
    b_ = Body{};
    b_.isSub = true;
    b_.subSymbol = &sym;
    b_.proc = id;
    b_.scale = timeScaleOf(*sym.getParentScope());
    b_.scope = sym.getHierarchicalPath();  // its own scope, also when called from elsewhere
    evalSymbol_ = &sym;
    try {
        for (const ast::FormalArgumentSymbol* a : sym.getArguments()) {
            sir::Param prm;
            prm.name = std::string(a->name);
            prm.dir = directionOf(a->direction);
            if (prm.dir == sir::Direction::Ref)
                unsupported(a->location, "ref argument '" + prm.name + "'");
            prm.type = typeOf(a->getType(), a->location);
            if (a->lifetime == ast::VariableLifetime::Automatic)
                prm.frameVar = frameVarOf(*a, a->location);
            else {
                prm.slot = slotOf(*a, a->location);
                noteWrite({prm.slot, 0, 0, 0, 0});  // written by every call (copy-in)
            }
            design_.subroutines[id].params.push_back(std::move(prm));
        }
        if (sym.returnValVar != nullptr && !ret.isVoid()) {
            // The function's own name as a variable (13.4.1).
            const ast::VariableSymbol& rv = *sym.returnValVar;
            if (rv.lifetime == ast::VariableLifetime::Automatic)
                b_.retFrame = frameVarOf(rv, rv.location);
            else
                b_.retSlot = slotOf(rv, rv.location);
        }
        lowerStatement(sym.getBody());
        if (!b_.dead) lowerReturn(nullptr, sym.location);
    } catch (const Unsupported&) {
        b_ = std::move(saved);
        evalSymbol_ = savedEval;
        instance_ = savedInstance;
        throw;
    }
    subReads_[id] = b_.reads;
    b_ = std::move(saved);
    evalSymbol_ = savedEval;
    instance_ = savedInstance;
    return id;
}

void Lowerer::lowerReturn(const ast::Expression* e, slang::SourceLocation sl) {
    if (!b_.isSub) unsupported(sl, "return outside a subroutine");
    const Type rt = design_.subroutines[b_.proc].returnType;
    if (rt.kind == sir::TypeKind::Void) {
        terminate(Op::Ret, {}, sl);
        return;
    }
    Val v;
    if (e != nullptr) {
        v = coerce(lowerExpr(*e), rt, sl);
    } else if (b_.retFrame) {
        v = emit(Op::Fload, frameVars()[*b_.retFrame].type, {Operand::frame(*b_.retFrame)}, sl);
    } else if (b_.retSlot) {
        v = emit(Op::Load, design_.slots[*b_.retSlot].type, {Operand::slot(*b_.retSlot)}, sl);
    } else {
        unsupported(sl, "function without a return value");
    }
    terminate(Op::Ret, {coerce(v, rt, sl).op}, sl);
}

Val Lowerer::lowerCall(const ast::CallExpression& call) {
    const slang::SourceLocation sl = call.sourceRange.start();
    const auto& sym = *std::get<0>(call.subroutine);
    const sir::SubroutineId id = subroutineOf(sym, sl);
    callees_[{b_.isSub, b_.proc}].push_back(id);
    const std::vector<sir::Param> params = design_.subroutines[id].params;
    const Type rt = design_.subroutines[id].returnType;
    const bool waits = design_.subroutines[id].mayWait;
    const auto args = call.arguments();
    if (args.size() != params.size()) unsupported(sl, "call with this argument list");

    std::vector<Operand> ops{Operand::subroutine(id)};
    struct CopyBack {
        const ast::Expression* place;
        sir::FrameVarId fv;
        Type type;
    };
    std::vector<CopyBack> copies;
    for (std::size_t i = 0; i < params.size(); ++i) {
        const sir::Param& prm = params[i];
        if (prm.dir == sir::Direction::Input) {
            ops.push_back(coerce(lowerExpr(*args[i]), prm.type, sl).op);
            continue;
        }
        // An output or inout: a whole variable of the formal's type is
        // passed as it is; anything else through a frame variable.
        const ast::Expression& place = *actualPlace(args[i]);
        if (place.kind == ast::ExpressionKind::NamedValue) {
            const ast::Symbol& v = place.as<ast::NamedValueExpression>().symbol;
            if (const std::optional<sir::FrameVarId> f = findFrameVar(v)) {
                if (frameVars()[*f].type == prm.type) {
                    ops.push_back(Operand::frame(*f));
                    continue;
                }
            } else if ((v.kind == ast::SymbolKind::Variable ||
                        v.kind == ast::SymbolKind::FormalArgument) &&
                       v.as<ast::VariableSymbol>().lifetime == ast::VariableLifetime::Static) {
                const sir::SlotId s = slotOf(v.as<ast::VariableSymbol>(), sl);
                if (design_.slots[s].dims.empty() && design_.slots[s].type == prm.type) {
                    ops.push_back(Operand::slot(s));
                    noteWrite({s, 0, 0, 0, 0});
                    continue;
                }
            }
        }
        const sir::FrameVarId fv = newFrameVar(prm.type);
        if (prm.dir == sir::Direction::Inout) {
            const Val cur = coerce(lowerExpr(place), prm.type, sl);
            emitVoid(Op::Fstore, {Operand::frame(fv), cur.op}, sl);
        }
        ops.push_back(Operand::frame(fv));
        copies.push_back({&place, fv, prm.type});
    }
    Val result{Operand::temp(0), sir::voidType()};
    if (waits) {
        // call.task ends the block; the caller goes on in a new one.
        const sir::BlockId cont = newBlock();
        ops.push_back(Operand::block(cont));
        terminate(Op::CallTask, std::move(ops), sl);
        setBlock(cont);
    } else if (rt.kind == sir::TypeKind::Void) {
        emitVoid(Op::Call, std::move(ops), sl);
    } else {
        result = emit(Op::Call, rt, std::move(ops), sl);
    }
    for (const CopyBack& c : copies) {
        const Val v = emit(Op::Fload, c.type, {Operand::frame(c.fv)}, sl);
        lowerStore(*c.place, coerce(v, typeOf(*c.place->type, sl), sl), false, sl);
    }
    return result;
}

}  // namespace kirimasim::front
