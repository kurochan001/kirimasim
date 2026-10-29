// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
    const bool aggregateRet = isAggregate(ret);
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
    b_.sub = id;
    b_.scale = timeScaleOf(*sym.getParentScope());
    b_.scope = sym.getHierarchicalPath();  // its own scope, also when called from elsewhere
    evalSymbol_ = &sym;
    try {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> shape;
        std::vector<std::pair<const ast::VariableSymbol*, Storage>> memberInitializers;
        // The arguments of an unpacked structure: one per leaf, named
        // <argument>.<member> (SIR_SPEC.md 8).
        const auto leafParams = [&](const ast::VariableSymbol& v, const std::string& name,
                                    sir::Direction dir) {
            std::vector<Leaf> leaves;
            structLeaves(v.getType(), "", leaves);
            const bool automatic = v.lifetime == ast::VariableLifetime::Automatic;
            const std::uint32_t first = automatic ? structFrameVars(v, v.location).index()
                                                  : structSlots(v, v.location).index();
            const auto at = static_cast<std::uint32_t>(design_.subroutine(id).params.size());
            for (std::size_t k = 0; k < leaves.size(); ++k) {
                sir::Param prm;
                prm.name = name + leaves[k].path;
                prm.dir = dir;
                const auto leaf = static_cast<std::uint32_t>(first + k);
                if (automatic) {
                    prm.frameVar = sir::FrameVarId{leaf};
                    prm.type = frameVar(prm.frameVar).type;
                    frameVar(prm.frameVar).defaultInit =
                        initValues_ && dir == sir::Direction::Output &&
                        sir::isFourState(prm.type) && !leaves[k].initialized;
                } else {
                    prm.slot = sir::SlotId{leaf};
                    prm.type = design_.slots[leaf].type;
                    noteWrite({sir::SlotId{leaf}, 0, 0, 0, 0});
                }
                design_.subroutine(id).params.push_back(std::move(prm));
            }
            if (dir == sir::Direction::Output && hasMemberDefaults(v.getType())) {
                Storage storage;
                storage.frame = automatic;
                if (automatic)
                    storage.var = sir::FrameVarId{first};
                else
                    storage.slot = sir::SlotId{first};
                memberInitializers.emplace_back(&v, std::move(storage));
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
                frameVars().push_back({prm.name,
                                       sir::refType(sir::isContainer(prm.type) && dims.empty()
                                                        ? prm.type.detail
                                                        : sir::TypeId{sir::kNone}),
                                       sir::kNone, dims, prm.type});
                b_.frameVars.emplace(a, prm.frameVar);
                ownFrame(*a, prm.frameVar);
                prm.refStatic = a->flags.has(ast::VariableFlags::RefStatic);
            } else if (a->lifetime == ast::VariableLifetime::Automatic) {
                prm.frameVar = frameVarOf(*a, a->location);
                frameVar(prm.frameVar).defaultInit =
                    initValues_ && prm.dir == sir::Direction::Output && sir::isFourState(prm.type);
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
            const sir::FrameVarId held =
                isStruct ? structFrameVars(rv, rv.location, true) : frameVarOf(rv, rv.location);
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
                const sir::FrameVarId leaf{held.index() + static_cast<std::uint32_t>(k)};
                prm.type = frameVar(leaf).type;
                // The implicit return variable is automatic even in a
                // static function; the hidden slots below only copy it out.
                frameVar(leaf).defaultInit =
                    initValues_ && sir::isFourState(prm.type) && !leaves[k].initialized;
                if (automatic) {
                    prm.frameVar = leaf;
                } else {
                    prm.slot = newSlot(rv, "$return" + leaves[k].path, *leaves[k].type,
                                       leaves[k].dims, rv.location, false);
                    if (k == 0) b_.retSlots = prm.slot;
                }
                design_.subroutine(id).params.push_back(std::move(prm));
            }
            shape.emplace_back(at, static_cast<std::uint32_t>(leaves.size()));
            b_.retAggregate = held;
            b_.retAggregateType = &ret;
            if (hasMemberDefaults(ret)) {
                Storage storage;
                storage.frame = true;
                storage.var = held;
                memberInitializers.emplace_back(&rv, std::move(storage));
            }
        } else if (sym.returnValVar != nullptr && !ret.isVoid()) {
            // The function's own name as a variable (13.4.1).
            const ast::VariableSymbol& rv = *sym.returnValVar;
            if (rv.lifetime == ast::VariableLifetime::Automatic) {
                b_.retFrame = frameVarOf(rv, rv.location);
                frameVar(*b_.retFrame).defaultInit =
                    initValues_ && sir::isFourState(frameVar(*b_.retFrame).type);
            } else
                b_.retSlot = slotOf(rv, rv.location);
        }
        argParams_[id] = std::move(shape);
        if (std::any_of(design_.subroutine(id).params.begin(), design_.subroutine(id).params.end(),
                        [](const auto& param) { return param.refStatic; }))
            qualifyReferenceBindings();
        // A structure member's declaration initializer is its type default
        // (7.2.2), also for output formals and the implicit return variable.
        for (const auto& [variable, storage] : memberInitializers) {
            if (!storage.frame) {
                lowerStaticInit(*variable, storage.slot, variable->location, true);
                continue;
            }
            Stores stores;
            memberDefaults(storage, variable->getType(), stores, variable->location);
            flushStores(stores, false, variable->location);
        }
        lowerStatement(sym.getBody());
        if (!b_.dead) lowerReturn(nullptr, sym.location);
        finishLexical();
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
    const auto finishReturn = [&] { leaveScopes(0, sl); };
    const Type rt = design_.subroutine(b_.sub).returnType;
    if (b_.retAggregate && b_.retAggregateType != nullptr) {
        // The value goes to $return like an aggregate assignment
        // (SIR_SPEC.md 8, VALUE_SEMANTICS.md 3.13).
        Storage held;
        held.frame = true;
        held.var = *b_.retAggregate;
        if (e != nullptr) {
            Stores stores;
            assignAggregate(held, *b_.retAggregateType, *e, stores, sl);
            flushStores(stores, false, sl);
        }
        if (b_.retSlots) {
            // A static function's $return slots.
            Storage out;
            out.slot = *b_.retSlots;
            Stores stores;
            copyAggregate(out, held, *b_.retAggregateType, stores, sl);
            flushStores(stores, false, sl);
        }
        finishReturn();
        terminate(Op::Ret, {}, sl);
        return;
    }
    if (rt.kind == sir::TypeKind::Void) {
        finishReturn();
        terminate(Op::Ret, {}, sl);
        return;
    }
    Val v;
    if (e != nullptr) {
        v = coerce(lowerExpr(*e), rt, sl);
    } else if (b_.retFrame) {
        v = emit(Op::Fload, frameVar(*b_.retFrame).type, {Operand::frame(*b_.retFrame)}, sl);
    } else if (b_.retSlot) {
        v = emit(Op::Load, design_.slot(*b_.retSlot).type, {Operand::slot(*b_.retSlot)}, sl);
    } else {
        unsupported(sl, "function without a return value");
    }
    const Val result = copyValue(v, rt, sl);
    finishReturn();
    terminate(Op::Ret, {result.op}, sl);
}

Val Lowerer::refOf(const ast::Expression& actual, bool isConst, slang::SourceLocation sl) {
    if (isDynamicContainer(*actual.type) ||
        (actual.kind == ast::ExpressionKind::ElementSelect &&
         isDynamicContainer(*actual.as<ast::ElementSelectExpression>().value().type)))
        return containerReference(actual, isConst, sl);
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
    ops.push_back(st->frame ? Operand::frame(st->var) : Operand::slot(st->slot));
    ops.insert(ops.end(), st->elem.begin(), st->elem.end());
    for (const ast::Expression* ix : indices) {
        const Val iv = lowerExpr(*ix);
        if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
        ops.push_back(iv.op);
    }
    // A selected element can itself be a fixed array. The reference names
    // its first element in declaration order; the formal retains the
    // remaining dimensions (13.5.2, SIR_SPEC.md 8). Complete the coordinates
    // here so every tier sees the same ordinary ref.make operation.
    if (ops.size() > 1) {
        const ast::Type* remaining = &actual.type->getCanonicalType();
        while (remaining->isUnpackedArray()) {
            const auto left = static_cast<std::uint64_t>(remaining->getFixedRange().left);
            ops.push_back(constant(sir::s2(64), left).op);
            remaining = &remaining->getArrayElementType()->getCanonicalType();
        }
    }
    if (!st->frame) {
        noteRead({st->slot, 0, 0, 0, 0});
        if (!isConst) noteWrite({st->slot, 0, 0, 0, 0});
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
    if (a && a->home == Place::Home::Slot && !sir::isContainer(prm.type)) {
        const sir::Slot& slot = design_.slot(a->slot);
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
    if (a && a->home == Place::Home::Frame && a->indices.empty() && !sir::isContainer(prm.type))
        return Operand::frame(a->frameHome());
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
    else if (actual.kind == ast::ExpressionKind::ElementSelect) {
        const auto src = storageOf(actual, sl);
        if (!src) unsupported(sl, "an input array argument of this actual");
        Storage dst;
        dst.frame = true;
        dst.var = fv;
        copyArrayStorage(dst, *src, stores, sl);
    } else
        assignArray(ArrayBase{Place::Home::Frame, fv}, {}, actual, stores, sl);
    flushStores(stores, false, sl);
    return Operand::frame(fv);
}

Val Lowerer::lowerCall(const ast::CallExpression& call) {
    const auto& sym = *std::get<0>(call.subroutine);
    const ast::Type& ret = sym.getReturnType();
    if (isAggregate(ret)) {
        // An aggregate value not used: it still needs a place to go.
        (void)callAggregate(call);
        return {Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    return lowerCallTo(call, nullptr);
}

Lowerer::Storage Lowerer::tempStorage(const ast::Type& t, slang::SourceLocation sl) {
    Storage st;
    st.frame = true;
    st.var = sir::FrameVarId{static_cast<std::uint32_t>(frameVars().size())};  // the next one
    std::vector<Leaf> leaves;
    structLeaves(t, "", leaves);
    for (const Leaf& leaf : leaves) {
        std::vector<sir::ArrayDim> dims = leaf.dims;
        const ast::Type& element = arrayElement(*leaf.type, dims);
        frameVars().push_back({"", typeOf(element, sl), sir::kNone, std::move(dims), {}});
    }
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

Val Lowerer::lowerEventCopy(const ast::Expression& e) {
    const auto sl = e.sourceRange.start();
    const Type type{sir::TypeKind::Event, 0};
    checkStack(sl);
    if (const auto it = substitutions_.find(&e); it != substitutions_.end()) return it->second;
    // Only input and declaration initialization use this entry (capture
    // contract 5; SIR_SPEC 7.4/8). General event rebinding stays unsupported.
    if (e.kind == ast::ExpressionKind::Conversion) {
        const auto& operand = e.as<ast::ConversionExpression>().operand();
        if (operand.type->isEvent() || operand.type->isNull()) return lowerEventCopy(operand);
    }
    if (e.kind == ast::ExpressionKind::NullLiteral) return constant(type, 0);
    if (e.type->isEvent()) {
        const auto operand = eventOperand(e, sl);
        return operand.kind == Operand::Kind::Slot ? emit(Op::Load, type, {operand}, sl)
                                                   : Val{operand, type};
    }
    unsupported(sl, "event copy from this expression");
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
    callees_.of(b_).push_back(id);
    // What a function reads is an always_comb's too, unless an assertion's
    // action block calls it; what a task reads is not (SCHEDULER_SPEC.md 6.3).
    if (sym.subroutineKind == ast::SubroutineKind::Function && b_.actions == 0)
        readCallees_.of(b_).push_back(id);
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
                    ops.push_back(Operand::frame(leaf.var));
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
                                                     ? callee.frame[prm.frameVar.index()].dims
                                                     : design_.slot(prm.slot).dims;
        if (!dims.empty()) {
            ops.push_back(arrayArgument(*outputPlace(args[i]), prm, dims, sl));
            continue;
        }
        if (prm.dir == sir::Direction::Input) {
            ops.push_back((prm.type.kind == sir::TypeKind::Event
                               ? lowerEventCopy(*args[i])
                               : copyValue(lowerExpr(*args[i]), prm.type, sl))
                              .op);
            continue;
        }
        if (sir::isContainer(prm.type)) {
            const auto fv = newFrameVar(prm.type);
            const auto* actual = outputPlace(args[i]);
            if (prm.dir == sir::Direction::Inout)
                emitVoid(Op::Fstore,
                         {Operand::frame(fv), copyValue(lowerExpr(*actual), prm.type, sl).op}, sl);
            ops.push_back(Operand::frame(fv));
            copies.push_back({actual, fv, prm.type});
            continue;
        }
        // An output or inout: a whole variable of the formal's type is
        // passed as it is; anything else through a frame variable.
        const ast::Expression& place = *outputPlace(args[i]);
        if (place.kind == ast::ExpressionKind::NamedValue) {
            const ast::Symbol& v = place.as<ast::NamedValueExpression>().symbol;
            if (const std::optional<sir::FrameVarId> f = findFrameVar(v)) {
                if (frameVar(*f).type == prm.type) {
                    ops.push_back(Operand::frame(*f));
                    continue;
                }
            } else if (isVariable(v) && inSlot(v)) {
                const sir::SlotId s = slotOf(v.as<ast::VariableSymbol>(), sl);
                if (design_.slot(s).dims.empty() && design_.slot(s).type == prm.type) {
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
            ops.push_back(Operand::frame(leafAt(*retTo, k).var));
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
        lowerStore(*c.place,
                   sir::isContainer(v.type) ? v : coerce(v, typeOf(*c.place->type, sl), sl), false,
                   sl);
    }
    for (std::size_t k = 0; k < structOuts.size(); ++k) {
        Stores stores;
        copyAggregate(structOuts[k].first, structOuts[k].second, *structOutTypes[k], stores, sl);
        flushStores(stores, false, sl);
    }
    return result;
}

}  // namespace kirimasim::front
