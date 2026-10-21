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
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/symbols/SubroutineSymbols.h"
#include "slang/ast/types/Type.h"
#include "slang/syntax/AllSyntax.h"

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

}  // namespace

sir::SubroutineId Lowerer::subroutineOf(const ast::SubroutineSymbol& sym,
                                        slang::SourceLocation sl) {
    // A subroutine of a package or compilation unit is one in the design
    // and belongs to its row (SIR_SPEC.md 3.3); the others, to the instance
    // that declares them, whichever scope calls them, so that one called
    // by a hierarchical name is the same subroutine with the same static
    // variables (6.21).
    const std::optional<sir::InstanceId> row = rowOf(sym);
    const sir::InstanceId owner = declaringInstance(sym).value_or(instance_);
    const auto key = std::make_pair(owner, static_cast<const ast::Symbol*>(&sym));
    if (auto it = subs_.find(key); it != subs_.end()) return it->second;
    // Its body is not SystemVerilog: a call stops here, the import reported.
    if (reportDpiImport(sym)) throw Unsupported{};

    sir::Subroutine s;
    s.name = design_.instance(owner).scopeName + (row ? "::" : ".") + std::string(sym.name);
    s.kind = sym.subroutineKind == ast::SubroutineKind::Function ? sir::SubroutineKind::Function
                                                                 : sir::SubroutineKind::Task;
    s.automatic = sym.defaultLifetime == ast::VariableLifetime::Automatic;
    s.instance = owner;
    s.loc = loc(sym.location);
    s.blocks.resize(1);
    const ast::Type& ret = sym.getReturnType();
    // An unpacked structure or array comes back through $return
    // (SIR_SPEC.md 8).
    const bool aggregateRet =
        unpackedStruct(ret) != nullptr || ret.getCanonicalType().isUnpackedArray();
    s.returnType = ret.isVoid() || aggregateRet ? sir::voidType() : typeOf(ret, sl);
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
    b_.proc = id.index();
    b_.scale = timeScaleOf(*sym.getParentScope());
    b_.scope = sym.getHierarchicalPath();  // its own scope, also when called from elsewhere
    evalSymbol_ = &sym;
    try {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> shape;
        // The arguments of an unpacked structure: one per leaf, named
        // <argument>.<member> (SIR_SPEC.md 8).
        const auto leafParams = [&](const ast::VariableSymbol& v, const std::string& name,
                                    sir::Direction dir) {
            std::vector<Leaf> leaves;
            structLeaves(v.getType(), "", leaves);
            const bool automatic = v.lifetime == ast::VariableLifetime::Automatic;
            const std::uint32_t first =
                automatic ? structFrameVars(v, v.location) : structSlots(v, v.location);
            const auto at = static_cast<std::uint32_t>(design_.subroutine(id).params.size());
            for (std::size_t k = 0; k < leaves.size(); ++k) {
                sir::Param prm;
                prm.name = name + leaves[k].path;
                prm.dir = dir;
                const auto leaf = static_cast<std::uint32_t>(first + k);
                if (automatic) {
                    prm.frameVar = leaf;
                    prm.type = frameVars()[leaf].type;
                } else {
                    prm.slot = leaf;
                    prm.type = design_.slots[leaf].type;
                    noteWrite({leaf, 0, 0, 0, 0});
                }
                design_.subroutine(id).params.push_back(std::move(prm));
            }
            return std::make_pair(at, static_cast<std::uint32_t>(leaves.size()));
        };
        for (const ast::FormalArgumentSymbol* a : sym.getArguments()) {
            sir::Param prm;
            prm.name = std::string(a->name);
            prm.dir = directionOf(a->direction);
            // A fixed-size unpacked array is passed element by element; its
            // type here is its element's (SIR_SPEC.md 8).
            std::vector<sir::ArrayDim> dims;
            const ast::Type& et = arrayElement(a->getType(), dims);
            if (unpackedStruct(et) != nullptr) {
                if (!dims.empty() || prm.dir == sir::Direction::Ref)
                    unsupported(a->location, "argument '" + prm.name +
                                                 "' of an array of structures or a ref structure");
                shape.push_back(leafParams(*a, prm.name, prm.dir));
                continue;
            }
            shape.emplace_back(static_cast<std::uint32_t>(design_.subroutine(id).params.size()),
                               1U);
            prm.type = typeOf(et, a->location);
            if (prm.dir == sir::Direction::Ref) {
                // A ref argument: a frame variable that holds a ref to the
                // actual, with the type and shape of what it refers to.
                if (a->flags.has(ast::VariableFlags::Const)) prm.dir = sir::Direction::ConstRef;
                if (a->lifetime != ast::VariableLifetime::Automatic)
                    unsupported(a->location,
                                "ref argument '" + prm.name + "' of a static subroutine");
                prm.frameVar = static_cast<sir::FrameVarId>(frameVars().size());
                frameVars().push_back({prm.name, sir::refType(), sir::kNone, dims, prm.type});
                b_.frameVars.emplace(a, prm.frameVar);
            } else if (a->lifetime == ast::VariableLifetime::Automatic) {
                prm.frameVar = frameVarOf(*a, a->location);
            } else {
                prm.slot = slotOf(*a, a->location);
                noteWrite({prm.slot, 0, 0, 0, 0});  // written by every call (copy-in)
            }
            design_.subroutine(id).params.push_back(std::move(prm));
        }
        if (sym.returnValVar != nullptr && aggregateRet) {
            // The function's own name as a variable (13.4.1), which slang
            // makes automatic, holds the value; $return, a hidden output,
            // gives it back: the same frame variables in an automatic
            // function, slots of their own in a static one, which every
            // return fills from it (SIR_SPEC.md 8).
            const ast::VariableSymbol& rv = *sym.returnValVar;
            const bool automatic = design_.subroutine(id).automatic;
            const bool isStruct = unpackedStruct(ret) != nullptr;
            std::vector<sir::ArrayDim> dims;
            const ast::Type& et = arrayElement(ret, dims);
            if (!isStruct && unpackedStruct(et) != nullptr)
                unsupported(rv.location, "function returning an array of structures");
            const std::uint32_t held =
                isStruct ? structFrameVars(rv, rv.location) : frameVarOf(rv, rv.location);
            std::vector<Leaf> leaves;
            if (isStruct)
                structLeaves(ret, "", leaves);
            else
                leaves.push_back({"", &ret, {}});
            const auto at = static_cast<std::uint32_t>(design_.subroutine(id).params.size());
            for (std::size_t k = 0; k < leaves.size(); ++k) {
                sir::Param prm;
                prm.name = "$return" + leaves[k].path;
                prm.dir = sir::Direction::Output;
                const auto leaf = static_cast<std::uint32_t>(held + k);
                prm.type = frameVars()[leaf].type;
                if (automatic) {
                    prm.frameVar = leaf;
                } else {
                    prm.slot = newSlot(rv, "$return" + leaves[k].path, *leaves[k].type,
                                       leaves[k].dims, rv.location);
                    if (k == 0) b_.retSlots = prm.slot;
                }
                design_.subroutine(id).params.push_back(std::move(prm));
            }
            shape.emplace_back(at, static_cast<std::uint32_t>(leaves.size()));
            b_.retAggregate = {held, true};
            b_.retAggregateType = &ret;
        } else if (sym.returnValVar != nullptr && !ret.isVoid()) {
            // The function's own name as a variable (13.4.1).
            const ast::VariableSymbol& rv = *sym.returnValVar;
            if (rv.lifetime == ast::VariableLifetime::Automatic)
                b_.retFrame = frameVarOf(rv, rv.location);
            else
                b_.retSlot = slotOf(rv, rv.location);
        }
        argParams_[id] = std::move(shape);
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
    leaveScopes(0, sl);
    const Type rt = design_.subroutine(b_.sub()).returnType;
    if (b_.retAggregate && b_.retAggregateType != nullptr) {
        // The value goes to $return like an aggregate assignment
        // (SIR_SPEC.md 8, VALUE_SEMANTICS.md 3.13).
        Storage held;
        held.id = b_.retAggregate->first;
        held.frame = b_.retAggregate->second;
        if (e != nullptr) {
            Stores stores;
            assignAggregate(held, *b_.retAggregateType, *e, stores, sl);
            flushStores(stores, false, sl);
        }
        if (b_.retSlots) {
            // A static function's $return slots.
            Storage out;
            out.id = *b_.retSlots;
            Stores stores;
            copyAggregate(out, held, *b_.retAggregateType, stores, sl);
            flushStores(stores, false, sl);
        }
        terminate(Op::Ret, {}, sl);
        return;
    }
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

Val Lowerer::refOf(const ast::Expression& actual, bool isConst, slang::SourceLocation sl) {
    // The variable a ref argument refers to: a variable, an element of an
    // unpacked array, or a member of an unpacked structure (13.5.2), with
    // the indices evaluated now (SIR_SPEC.md 8). The call writes what it
    // refers to, unless the argument is const.
    std::vector<Operand> ops;
    std::vector<const ast::Expression*> indices;
    const ast::Expression* base = &actual;
    if (const std::optional<ArrayAccess> acc = arrayAccess(actual)) {
        base = acc->base;
        indices = acc->indices;
    }
    if (unpackedStruct(*base->type) != nullptr && indices.empty())
        unsupported(sl, "a ref argument of an unpacked structure type");
    const std::optional<Storage> st = storageOf(*base, sl);
    if (!st || st->dynamic) unsupported(sl, "a ref argument of this actual");
    if (st->frame) {
        // A ref into a frame steps from leaf to leaf one word at a time.
        const ast::Type* elem = base->type;
        while (elem->isUnpackedArray())
            elem = elem->getArrayElementType();
        if (const ast::UnpackedStructType* us = unpackedStruct(*elem)) {
            for (const ast::FieldSymbol* field : us->fields)
                if (field->getType().getBitWidth() > 64)
                    unsupported(sl, "a ref to an automatic structure with a member wider than "
                                    "64 bits");
        }
    }
    ops.push_back(st->frame ? Operand::frame(st->id) : Operand::slot(st->id));
    ops.insert(ops.end(), st->elem.begin(), st->elem.end());
    for (const ast::Expression* ix : indices) {
        const Val iv = lowerExpr(*ix);
        if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
        ops.push_back(iv.op);
    }
    if (!st->frame) {
        noteRead({st->id, 0, 0, 0, 0});
        if (!isConst) noteWrite({st->id, 0, 0, 0, 0});
    }
    return emit(Op::RefMake, sir::refType(), std::move(ops), sl);
}

Operand Lowerer::arrayArgument(const ast::Expression& actual, const sir::Param& prm,
                               const std::vector<sir::ArrayDim>& dims, slang::SourceLocation sl) {
    // A fixed-size unpacked array argument is passed as the whole array, or
    // a run of elements of one, and copied element by element (SIR_SPEC.md
    // 8). An input that is not a variable is first computed into an array
    // of the caller's frame.
    const std::optional<ArrayTarget> a = arrayTarget(actual);
    if (a && a->home == Place::Home::Slot) {
        const sir::Slot& slot = design_.slots[a->slot];
        Operand o = Operand::slot(a->slot);
        if (!a->indices.empty()) {
            // The elements of a sub-array lie together, from its left bounds.
            std::vector<std::int64_t> first = a->indices;
            std::uint32_t count = 1;
            for (std::size_t d = a->indices.size(); d < slot.dims.size(); ++d) {
                first.push_back(slot.dims[d].left);
                count *= static_cast<std::uint32_t>((slot.dims[d].left <= slot.dims[d].right
                                                         ? slot.dims[d].right - slot.dims[d].left
                                                         : slot.dims[d].left - slot.dims[d].right) +
                                                    1);
            }
            o.firstElem = elementNumber(slot, first);
            o.numElems = count;
        }
        const sir::SlotRange r{a->slot, o.firstElem, o.numElems, 0, 0};
        if (prm.dir != sir::Direction::Input) noteWrite(r);
        if (prm.dir != sir::Direction::Output) noteRead(r);
        return o;
    }
    if (a && a->home == Place::Home::Frame && a->indices.empty()) return Operand::frame(a->slot);
    if (prm.dir != sir::Direction::Input)
        unsupported(sl, "an output or inout array argument of this actual");
    const auto fv = static_cast<sir::FrameVarId>(frameVars().size());
    frameVars().push_back({"", prm.type, sir::kNone, dims, {}});
    Stores stores;
    // What arrayTarget found is copied from: lowering the actual again
    // would evaluate it twice, a function's value calling the function
    // again (4.9.7: an input is evaluated once, at the call).
    if (a)
        copyArray(ArrayBase{Place::Home::Frame, fv}, {}, baseOf(*a), a->indices, stores, sl);
    else
        assignArray(ArrayBase{Place::Home::Frame, fv}, {}, actual, stores, sl);
    flushStores(stores, false, sl);
    return Operand::frame(fv);
}

Val Lowerer::lowerCall(const ast::CallExpression& call) {
    const auto& sym = *std::get<0>(call.subroutine);
    const ast::Type& ret = sym.getReturnType();
    if (unpackedStruct(ret) != nullptr || ret.getCanonicalType().isUnpackedArray()) {
        // An aggregate value not used: it still needs a place to go.
        (void)callAggregate(call);
        return {Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    return lowerCallTo(call, nullptr);
}

Lowerer::Storage Lowerer::tempStorage(const ast::Type& t, slang::SourceLocation sl) {
    Storage st;
    st.frame = true;
    st.id = static_cast<std::uint32_t>(frameVars().size());
    if (unpackedStruct(t) != nullptr) {
        std::vector<Leaf> leaves;
        structLeaves(t, "", leaves);
        for (const Leaf& l : leaves) {
            if (l.type->getCanonicalType().isUnpackedArray() || !l.dims.empty())
                unsupported(sl, "automatic unpacked array in a structure or of structures");
            frameVars().push_back({"", typeOf(*l.type, sl), sir::kNone, {}, {}});
        }
        return st;
    }
    std::vector<sir::ArrayDim> dims;
    const ast::Type& et = arrayElement(t, dims);
    if (unpackedStruct(et) != nullptr) unsupported(sl, "array of structures held here");
    frameVars().push_back({"", typeOf(et, sl), sir::kNone, std::move(dims), {}});
    return st;
}

Lowerer::Storage Lowerer::callAggregate(const ast::CallExpression& call) {
    const slang::SourceLocation sl = call.sourceRange.start();
    const Storage tmp = tempStorage(std::get<0>(call.subroutine)->getReturnType(), sl);
    (void)lowerCallTo(call, &tmp);
    return tmp;
}

std::optional<Lowerer::LaterWork> Lowerer::laterWork(const ast::SubroutineSymbol& f) {
    if (const auto it = laterWork_.find(&f); it != laterWork_.end()) return it->second;
    laterWork_[&f] = std::nullopt;  // a recursive call adds nothing
    std::optional<LaterWork> found;
    const auto take = [&](slang::SourceLocation at, const std::string& what) {
        if (!found) found = LaterWork{at, what + " in '" + std::string(f.name) + "'"};
    };
    // A blocking ->, $strobe, $monitor and deferred assertions do not
    // count (SCHEDULER_SPEC.md 6.5), nor the initial value of a static
    // variable, which static initialization runs, not the call.
    auto visitor = ast::makeVisitor(
        [&](auto& v, const ast::VariableSymbol& s) {
            if (s.lifetime == ast::VariableLifetime::Automatic) v.visitDefault(s);
        },
        [&](auto& v, const ast::AssignmentExpression& a) {
            if (a.isNonBlocking()) take(a.sourceRange.start(), "a nonblocking assignment");
            v.visitDefault(a);
        },
        [&](auto& v, const ast::EventTriggerStatement& t) {
            if (t.isNonBlocking) take(t.sourceRange.start(), "a nonblocking event trigger");
            v.visitDefault(t);
        },
        [&](auto& v, const ast::BlockStatement& b) {
            if (b.blockKind == ast::StatementBlockKind::JoinNone)
                take(startOf(b), "a fork with join_none");
            v.visitDefault(b);
        },
        [&](auto& v, const ast::CallExpression& c) {
            if (!c.isSystemCall()) {
                const ast::SubroutineSymbol& callee = *std::get<0>(c.subroutine);
                if (callee.subroutineKind == ast::SubroutineKind::Function)
                    if (const std::optional<LaterWork> inner = laterWork(callee); inner && !found)
                        found = inner;
            }
            v.visitDefault(c);
        });
    f.visit(visitor);  // its declarations and its body
    laterWork_[&f] = found;
    return found;
}

Val Lowerer::lowerCallTo(const ast::CallExpression& call, const Storage* retTo) {
    const slang::SourceLocation sl = call.sourceRange.start();
    const auto& sym = *std::get<0>(call.subroutine);
    // A function that schedules work after it returns may be called only
    // from a thread of an initial or always procedure (13.4.4,
    // SCHEDULER_SPEC.md 6.5); a call inside a function counts where that
    // function is called.
    if (!b_.isSub && sym.subroutineKind == ast::SubroutineKind::Function) {
        const sir::ProcessKind k = proc().kind;
        const char* caller =
            k == sir::ProcessKind::StaticInit ? "the initial value of a static variable"
            : k == sir::ProcessKind::Cont     ? "a continuous assignment"
            : k == sir::ProcessKind::Final    ? "a final procedure"
            : k == sir::ProcessKind::Override ? "the right side of a procedural assign or force"
                                              : nullptr;
        if (caller != nullptr)
            if (const std::optional<LaterWork> w = laterWork(sym))
                error(sl,
                      "'" + std::string(sym.name) +
                          "' schedules work that runs after it returns, so it can be called only "
                          "from an initial or always procedure, not from " +
                          caller,
                      w->at, w->what);
    }
    const sir::SubroutineId id = subroutineOf(sym, sl);
    callees_[{b_.isSub, b_.proc}].push_back(id);
    // What a function reads is an always_comb's too, unless an assertion's
    // action block calls it; what a task reads is not (SCHEDULER_SPEC.md 6.3).
    if (sym.subroutineKind == ast::SubroutineKind::Function && b_.actions == 0)
        readCallees_[{b_.isSub, b_.proc}].push_back(id);
    // A task a process calls may write what an always_comb writes
    // (checkTaskAndActionWrites).
    if (sym.subroutineKind == ast::SubroutineKind::Task && !b_.isSub)
        taskCalls_[b_.proc].push_back({id, sl});
    const std::vector<sir::Param> params = design_.subroutine(id).params;
    const Type rt = design_.subroutine(id).returnType;
    const bool waits = design_.subroutine(id).mayWait;
    const auto args = call.arguments();
    const auto formals = sym.getArguments();
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shape = argParams_[id];
    if (args.size() != formals.size() || shape.size() < args.size())
        unsupported(sl, "call with this argument list");

    std::vector<Operand> ops{Operand::subroutine(id)};
    struct CopyBack {
        const ast::Expression* place;
        sir::FrameVarId fv;
        Type type;
    };
    std::vector<CopyBack> copies;
    /// Structures given back through frame variables: to where, from where.
    std::vector<std::pair<Storage, Storage>> structOuts;
    std::vector<const ast::Type*> structOutTypes;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto [first, count] = shape[i];
        const sir::Param& prm = params[first];
        const ast::Type& ft = formals[i]->getType();
        if (unpackedStruct(ft) != nullptr) {
            // One operand per leaf (SIR_SPEC.md 8): the values of an input,
            // frame variables for an output or inout, copied back after.
            Storage tmp = tempStorage(ft, sl);
            if (prm.dir != sir::Direction::Output) {
                Stores stores;
                assignAggregate(tmp, ft, *outputPlace(args[i]), stores, sl);
                flushStores(stores, false, sl);
            }
            for (std::uint32_t k = 0; k < count; ++k) {
                const Storage leaf = leafAt(tmp, k);
                if (prm.dir == sir::Direction::Input)
                    ops.push_back(
                        coerce(loadPlace(storagePlace(leaf), sl), params[first + k].type, sl).op);
                else
                    ops.push_back(Operand::frame(leaf.id));
            }
            if (prm.dir != sir::Direction::Input) {
                const std::optional<Storage> dst = storageOf(*outputPlace(args[i]), sl);
                if (!dst) unsupported(sl, "this output or inout structure argument");
                structOuts.emplace_back(*dst, tmp);
                structOutTypes.push_back(&ft);
            }
            continue;
        }
        if (prm.dir == sir::Direction::Ref || prm.dir == sir::Direction::ConstRef) {
            ops.push_back(refOf(*outputPlace(args[i]), prm.dir == sir::Direction::ConstRef, sl).op);
            continue;
        }
        const sir::Subroutine& callee = design_.subroutine(id);
        const std::vector<sir::ArrayDim>& dims = prm.frameVar != sir::kNone
                                                     ? callee.frame[prm.frameVar].dims
                                                     : design_.slots[prm.slot].dims;
        if (!dims.empty()) {
            ops.push_back(arrayArgument(*outputPlace(args[i]), prm, dims, sl));
            continue;
        }
        if (prm.dir == sir::Direction::Input) {
            ops.push_back(coerce(lowerExpr(*args[i]), prm.type, sl).op);
            continue;
        }
        // An output or inout: a whole variable of the formal's type is
        // passed as it is; anything else through a frame variable.
        const ast::Expression& place = *outputPlace(args[i]);
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
    if (shape.size() > args.size()) {
        // $return: the frame variables that receive the value.
        if (retTo == nullptr || !retTo->frame) unsupported(sl, "call of this function here");
        const auto [first, count] = shape[args.size()];
        (void)first;
        for (std::uint32_t k = 0; k < count; ++k)
            ops.push_back(Operand::frame(retTo->id + k));
    }
    Val result{Operand::temp(sir::TempId{0}), sir::voidType()};
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
    for (std::size_t k = 0; k < structOuts.size(); ++k) {
        Stores stores;
        copyAggregate(structOuts[k].first, structOuts[k].second, *structOutTypes[k], stores, sl);
        flushStores(stores, false, sl);
    }
    return result;
}

}  // namespace kirimasim::front
