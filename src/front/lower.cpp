// Lowering of slang's elaborated design to SIR: modules, variables and
// procedures. Statements are in lower_stmt.cpp and expressions in
// lower_expr.cpp (lowerer.h).
//
// So far: top modules whose members are variables of integral types up to
// 64 bits (and fixed-size unpacked arrays of them) and initial, always,
// always_comb, always_latch and always_ff procedures. Everything else is
// reported as an unsupported construct (CLI.md 5), never silently dropped.

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haremasim/front/front.h"
#include "haremasim/sir/timescale.h"
#include "haremasim/sir/types.h"
#include "haremasim/sir/verify.h"
#include "lowerer.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/symbols/SubroutineSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/driver/Driver.h"
#include "slang/numeric/Time.h"

namespace haremasim::front {

namespace {

/// The exponent of a slang time value: 1ns is -9, 100ps is -10.
int exponentOf(const slang::TimeScaleValue& v) {
    const int magnitude = v.magnitude == slang::TimeScaleMagnitude::Hundred ? 2
                          : v.magnitude == slang::TimeScaleMagnitude::Ten   ? 1
                                                                            : 0;
    return -3 * static_cast<int>(v.unit) + magnitude;
}

/// The global time precision: the smallest precision of all design
/// elements (3.14.3). An element without a timescale of its own takes the
/// default, which is --timescale if given and 1ns/1ns otherwise
/// (SCHEDULER_SPEC.md 1). slang has already reported a design that mixes
/// elements with and without one, unless --timescale filled the gaps.
int globalPrecision(const ast::Compilation& compilation) {
    const std::optional<slang::TimeScale> given = compilation.getDefaultTimeScale();
    const int fallback = given ? exponentOf(given->precision) : sir::TimeScale{}.precision;
    std::optional<int> result;
    const auto take = [&](const std::optional<slang::TimeScale>& ts) {
        const int e = ts ? exponentOf(ts->precision) : fallback;
        result = result ? std::min(*result, e) : e;
    };
    for (const ast::Symbol* sym : compilation.getDefinitions())
        if (sym->kind == ast::SymbolKind::Definition)
            take(sym->as<ast::DefinitionSymbol>().timeScale);
    for (const ast::PackageSymbol* pkg : compilation.getPackages())
        if (pkg != &compilation.getStdPackage()) take(pkg->timeScale);
    for (const ast::CompilationUnitSymbol* unit : compilation.getCompilationUnits())
        if (unit->timeScale) take(unit->timeScale);
    return result.value_or(fallback);
}

sir::Edge edgeOf(ast::EdgeKind k) {
    switch (k) {
        case ast::EdgeKind::PosEdge: return sir::Edge::Pos;
        case ast::EdgeKind::NegEdge: return sir::Edge::Neg;
        case ast::EdgeKind::BothEdges: return sir::Edge::Both;
        case ast::EdgeKind::None: break;
    }
    return sir::Edge::Any;
}

/// Whether range `a` covers range `b` of the same slot (0 counts means
/// "all"; `width` is the slot's element width).
bool covers(const sir::SlotRange& a, const sir::SlotRange& b, std::uint32_t width,
            std::uint32_t elems) {
    if (a.slot != b.slot) return false;
    const std::uint32_t af = a.firstElem;
    const std::uint32_t an = a.numElems == 0 ? elems : a.numElems;
    const std::uint32_t bf = b.firstElem;
    const std::uint32_t bn = b.numElems == 0 ? elems : b.numElems;
    const std::uint32_t al = a.lsb;
    const std::uint32_t aw = a.width == 0 ? width : a.width;
    const std::uint32_t bl = b.lsb;
    const std::uint32_t bw = b.width == 0 ? width : b.width;
    return af <= bf && bf + bn <= af + an && al <= bl && bl + bw <= al + aw;
}

/// Whether two ranges share a bit.
bool overlap(const sir::SlotRange& a, const sir::SlotRange& b, std::uint32_t width,
             std::uint32_t elems) {
    if (a.slot != b.slot) return false;
    const std::uint32_t an = a.numElems == 0 ? elems : a.numElems;
    const std::uint32_t bn = b.numElems == 0 ? elems : b.numElems;
    const std::uint32_t aw = a.width == 0 ? width : a.width;
    const std::uint32_t bw = b.width == 0 ? width : b.width;
    return a.firstElem < b.firstElem + bn && b.firstElem < a.firstElem + an && a.lsb < b.lsb + bw &&
           b.lsb < a.lsb + aw;
}

}  // namespace

/// The time unit and precision of the scope a procedure is in; 1ns/1ns
/// when nothing sets them (SCHEDULER_SPEC.md 1).
sir::TimeScale timeScaleOf(const ast::Scope& scope) {
    const std::optional<slang::TimeScale> ts = scope.getTimeScale();
    if (!ts) return {};
    return {exponentOf(ts->base), exponentOf(ts->precision)};
}

/// Whether a statement may wait (a timing control, wait, fork with join or
/// join_any). Unknown statements count as waiting.
bool mayWaitIn(const ast::Statement& s, std::vector<const ast::Symbol*>& seen) {
    switch (s.kind) {
        case ast::StatementKind::Empty:
        case ast::StatementKind::VariableDeclaration:
        case ast::StatementKind::Break:
        case ast::StatementKind::Continue:
        case ast::StatementKind::DisableFork:
        case ast::StatementKind::Return: return false;
        case ast::StatementKind::ExpressionStatement: {
            const ast::Expression& e = s.as<ast::ExpressionStatement>().expr;
            if (e.kind == ast::ExpressionKind::Assignment)
                return e.as<ast::AssignmentExpression>().timingControl != nullptr;
            if (e.kind != ast::ExpressionKind::Call) return false;
            // A function never waits; a task may if its body may (13.3).
            const auto& call = e.as<ast::CallExpression>();
            if (call.isSystemCall()) return false;
            const ast::SubroutineSymbol& sub = *std::get<0>(call.subroutine);
            if (sub.subroutineKind == ast::SubroutineKind::Function) return false;
            if (std::find(seen.begin(), seen.end(), &sub) != seen.end()) return false;
            seen.push_back(&sub);
            return mayWaitIn(sub.getBody(), seen);
        }
        case ast::StatementKind::List:
            for (const ast::Statement* item : s.as<ast::StatementList>().list)
                if (mayWaitIn(*item, seen)) return true;
            return false;
        case ast::StatementKind::Block: {
            const auto& b = s.as<ast::BlockStatement>();
            if (b.blockKind != ast::StatementBlockKind::Sequential &&
                b.blockKind != ast::StatementBlockKind::JoinNone)
                return true;
            return b.blockKind == ast::StatementBlockKind::Sequential && mayWaitIn(b.body, seen);
        }
        case ast::StatementKind::Conditional: {
            const auto& c = s.as<ast::ConditionalStatement>();
            return mayWaitIn(c.ifTrue, seen) ||
                   (c.ifFalse != nullptr && mayWaitIn(*c.ifFalse, seen));
        }
        case ast::StatementKind::Case: {
            const auto& c = s.as<ast::CaseStatement>();
            for (const auto& g : c.items)
                if (mayWaitIn(*g.stmt, seen)) return true;
            return c.defaultCase != nullptr && mayWaitIn(*c.defaultCase, seen);
        }
        case ast::StatementKind::ForLoop:
            return mayWaitIn(s.as<ast::ForLoopStatement>().body, seen);
        case ast::StatementKind::ForeachLoop:
            return mayWaitIn(s.as<ast::ForeachLoopStatement>().body, seen);
        case ast::StatementKind::RepeatLoop:
            return mayWaitIn(s.as<ast::RepeatLoopStatement>().body, seen);
        case ast::StatementKind::WhileLoop:
            return mayWaitIn(s.as<ast::WhileLoopStatement>().body, seen);
        case ast::StatementKind::DoWhileLoop:
            return mayWaitIn(s.as<ast::DoWhileLoopStatement>().body, seen);
        case ast::StatementKind::ForeverLoop:
            return mayWaitIn(s.as<ast::ForeverLoopStatement>().body, seen);
        default: return true;
    }
}

bool mayWait(const ast::Statement& s) {
    std::vector<const ast::Symbol*> seen;
    return mayWaitIn(s, seen);
}

// --- Diagnostics and types ---------------------------------------------------

void Lowerer::unsupported(slang::SourceLocation sl, std::string_view what) {
    report(sl, what);
    throw Unsupported{};
}

void Lowerer::report(slang::SourceLocation sl, std::string_view what) {
    ++numErrors_;
    if (sl.valid()) {
        diags_ += std::string(sm_.getFileName(sl)) + ":" + std::to_string(sm_.getLineNumber(sl)) +
                  ":" + std::to_string(sm_.getColumnNumber(sl)) + ": ";
    } else {
        diags_ += "haremasim: ";
    }
    diags_ += "error: unsupported: ";
    diags_ += what;
    diags_ += " (not implemented yet)\n";
}

sir::SourceLoc Lowerer::loc(slang::SourceLocation sl) {
    if (!sl.valid()) return {};
    const std::uint32_t buf = sl.buffer().getId();
    auto it = fileIds_.find(buf);
    if (it == fileIds_.end()) {
        const auto id = static_cast<std::uint32_t>(design_.sources.size());
        design_.sources.emplace_back(sm_.getFileName(sl));
        it = fileIds_.emplace(buf, id).first;
    }
    return {it->second, static_cast<std::uint32_t>(sm_.getLineNumber(sl)),
            static_cast<std::uint32_t>(sm_.getColumnNumber(sl))};
}

sir::Type Lowerer::typeOf(const ast::Type& type, slang::SourceLocation sl) {
    const ast::Type& t = type.getCanonicalType();
    if (t.isString()) return sir::Type{sir::TypeKind::String, 0};
    if (t.isFloating())
        return t.as<ast::FloatingType>().floatKind == ast::FloatingType::ShortReal
                   ? sir::Type{sir::TypeKind::ShortReal, 0}
                   : sir::Type{sir::TypeKind::Real, 0};  // real and realtime (6.12)
    if (!t.isIntegral()) unsupported(sl, "type '" + t.toString() + "'");
    const std::uint32_t w = t.getBitWidth();
    if (w == 0 || w > 64) unsupported(sl, "values wider than 64 bits");
    if (t.isFourState()) return t.isSigned() ? sir::s4(w) : sir::u4(w);
    return t.isSigned() ? sir::s2(w) : sir::u2(w);
}

// --- Building blocks ---------------------------------------------------------

sir::BlockId Lowerer::newBlock() {
    blocks().emplace_back();
    return static_cast<sir::BlockId>(blocks().size() - 1);
}

Val Lowerer::emit(sir::Op op, sir::Type t, std::vector<sir::Operand> ops,
                  slang::SourceLocation sl) {
    const sir::TempId r = newTemp();
    blocks()[b_.block].instrs.push_back({op, t, r, std::move(ops), loc(sl)});
    return {sir::Operand::temp(r), t};
}

void Lowerer::emitVoid(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl) {
    blocks()[b_.block].instrs.push_back({op, sir::voidType(), sir::kNone, std::move(ops), loc(sl)});
}

void Lowerer::emitInto(sir::BlockId b, sir::Op op, std::vector<sir::Operand> ops,
                       slang::SourceLocation sl) {
    blocks()[b].instrs.push_back({op, sir::voidType(), sir::kNone, std::move(ops), loc(sl)});
}

void Lowerer::terminate(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl) {
    emitVoid(op, std::move(ops), sl);
    b_.dead = true;
}

Val Lowerer::constant(sir::Type t, std::uint64_t a, std::uint64_t b) {
    const std::uint64_t m = t.width >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << t.width) - 1;
    sir::Constant c;
    c.type = t;
    c.aWords = {a & m};
    if (sir::isFourState(t)) c.bWords = {b & m};
    const auto id = static_cast<sir::ConstId>(design_.constants.size());
    design_.constants.push_back(std::move(c));
    return {sir::Operand::constant(id), t};
}

// --- Variables ---------------------------------------------------------------

sir::SlotId Lowerer::slotOf(const ast::VariableSymbol& v, slang::SourceLocation sl) {
    if (auto it = slots_.find(&v); it != slots_.end()) return it->second;
    const ast::Type* t = &v.getType().getCanonicalType();
    std::vector<sir::ArrayDim> dims;
    while (t->kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& a = t->as<ast::FixedSizeUnpackedArrayType>();
        dims.push_back({a.range.left, a.range.right});
        t = &a.elementType.getCanonicalType();
    }
    if (t->isUnpackedArray()) unsupported(sl, "variable of type '" + v.getType().toString() + "'");
    sir::Slot s;
    // Static variables of unnamed blocks share a hierarchical path; the
    // second and later get "~2", "~3", ... (names are for diagnostics and
    // the text form, and must be unique; SIR_SPEC.md 3.1).
    s.name = v.getHierarchicalPath();
    if (const std::uint32_t n = ++slotNames_[s.name]; n > 1) s.name += "~" + std::to_string(n);
    s.type = typeOf(*t, sl);
    s.dims = std::move(dims);
    s.instance = instance_;
    // A variable or argument of the subroutine being lowered belongs to it.
    if (b_.isSub) {
        for (const ast::Scope* sc = v.getParentScope(); sc != nullptr;
             sc = sc->asSymbol().getParentScope()) {
            if (&sc->asSymbol() == b_.subSymbol) {
                s.subroutine = b_.proc;
                break;
            }
            if (sc->asSymbol().kind == ast::SymbolKind::InstanceBody) break;
        }
    }
    const auto id = static_cast<sir::SlotId>(design_.slots.size());
    design_.names.push_back({s.name, id});
    design_.slots.push_back(std::move(s));
    slots_.emplace(&v, id);
    return id;
}

sir::FrameVarId Lowerer::frameVarOf(const ast::VariableSymbol& v, slang::SourceLocation sl) {
    if (auto it = b_.frameVars.find(&v); it != b_.frameVars.end()) return it->second;
    const sir::Type t = typeOf(v.getType(), sl);
    const auto id = static_cast<sir::FrameVarId>(frameVars().size());
    frameVars().push_back({std::string(v.name), t, sir::kNone});
    b_.frameVars.emplace(&v, id);
    return id;
}

std::optional<sir::FrameVarId> Lowerer::findFrameVar(const ast::Symbol& s) const {
    if (auto it = b_.frameVars.find(&s); it != b_.frameVars.end()) return it->second;
    return std::nullopt;
}

sir::FrameVarId Lowerer::newFrameVar(sir::Type t) {
    const auto id = static_cast<sir::FrameVarId>(frameVars().size());
    frameVars().push_back({std::string(), t, sir::kNone});
    return id;
}

void Lowerer::noteWrite(const sir::SlotRange& r) {
    const sir::Slot& s = design_.slots[r.slot];
    const auto elems = static_cast<std::uint32_t>(sir::elementCount(s));
    std::vector<sir::SlotRange>& writes = b_.isSub ? subWrites_[b_.proc] : proc().writes;
    for (const sir::SlotRange& w : writes)
        if (covers(w, r, s.type.width, elems)) return;
    writes.push_back(r);
}

void Lowerer::noteRead(const sir::SlotRange& r) { b_.reads.push_back(r); }

// --- Modules and procedures --------------------------------------------------

void Lowerer::lower(const ast::RootSymbol& root) {
    design_.timePrecision = precision_;
    for (const ast::InstanceSymbol* top : root.topInstances)
        lowerTop(*top);
    if (numErrors_ != 0) return;
    finishDesign();
    checkWriters();
}

void Lowerer::finishDesign() {
    const auto add = [&](std::vector<sir::SlotRange>& set, const sir::SlotRange& r) {
        const sir::Slot& s = design_.slots[r.slot];
        const auto elems = static_cast<std::uint32_t>(sir::elementCount(s));
        for (const sir::SlotRange& w : set)
            if (covers(w, r, s.type.width, elems)) return false;
        set.push_back(r);
        return true;
    };
    // What each subroutine writes and reads, with what it calls, until
    // nothing changes (recursion included).
    std::map<sir::SubroutineId, std::vector<sir::SlotRange>> writes = subWrites_;
    std::map<sir::SubroutineId, std::vector<sir::SlotRange>> reads = subReads_;
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto& [key, called] : callees_) {
            if (!key.first) continue;
            for (const sir::SubroutineId c : called) {
                for (const sir::SlotRange& r : std::vector(writes[c]))
                    changed = add(writes[key.second], r) || changed;
                for (const sir::SlotRange& r : std::vector(reads[c]))
                    changed = add(reads[key.second], r) || changed;
            }
        }
    }
    // A process writes what the subroutines it calls write; always_comb and
    // always_latch are sensitive to what they read there too (9.2.2.2.1,
    // SCHEDULER_SPEC.md 6.3, 6.4).
    for (const auto& [key, called] : callees_) {
        if (key.first) continue;
        for (const sir::SubroutineId c : called) {
            for (const sir::SlotRange& r : writes[c])
                add(design_.processes[key.second].writes, r);
            if (combReads_.count(key.second) != 0)
                for (const sir::SlotRange& r : reads[c])
                    combReads_[key.second].push_back(r);
        }
    }
    // The implicit sensitivity: what the block reads, less what it writes
    // (9.2.2.2.1, SCHEDULER_SPEC.md 6.3).
    for (const auto& [p, procReads] : combReads_) {
        sir::Process& proc = design_.processes[p];
        for (const sir::SlotRange& r : procReads) {
            const sir::Slot& s = design_.slots[r.slot];
            const auto elems = static_cast<std::uint32_t>(sir::elementCount(s));
            // Variables and arguments declared in a called function are not
            // part of it either.
            bool skip = s.subroutine != sir::kNone;
            for (const sir::SlotRange& w : proc.writes)
                skip = skip || covers(w, r, s.type.width, elems);
            for (const sir::Trigger& t : proc.triggers)
                skip = skip || covers(t.range, r, s.type.width, elems);
            if (!skip) proc.triggers.push_back({r, sir::Edge::Any});
        }
    }
}

void Lowerer::checkWriters() {
    // A variable that always_comb, always_latch or always_ff writes has no
    // other writer (9.2.2.2, 9.2.2.4); haremasim checks it here, at
    // elaboration (SCHEDULER_SPEC.md 6.4).
    const auto exclusive = [](sir::ProcessKind k) {
        return k == sir::ProcessKind::Comb || k == sir::ProcessKind::Latch ||
               k == sir::ProcessKind::Ff;
    };
    const auto where = [&](const sir::SourceLoc& l) {
        const std::string file = l.file < design_.sources.size() ? design_.sources[l.file] : "?";
        return file + ":" + std::to_string(l.line) + ":" + std::to_string(l.column);
    };
    const auto& ps = design_.processes;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        for (std::size_t j = 0; j < ps.size(); ++j) {
            // A declaration's initial value is not a write by a process
            // (6.8), so static-init is no other writer.
            if (i == j || !exclusive(ps[i].kind) || (exclusive(ps[j].kind) && j < i) ||
                ps[j].kind == sir::ProcessKind::StaticInit)
                continue;
            bool reported = false;
            for (const sir::SlotRange& a : ps[i].writes) {
                for (const sir::SlotRange& b : ps[j].writes) {
                    const sir::Slot& s = design_.slots[a.slot];
                    const auto elems = static_cast<std::uint32_t>(sir::elementCount(s));
                    if (reported || !overlap(a, b, s.type.width, elems)) continue;
                    ++numErrors_;
                    diags_ += where(ps[i].loc) + ": error: '" + s.name +
                              "' is written here and by " + ps[j].name + " at " + where(ps[j].loc) +
                              "; a variable that always_comb, always_latch or always_ff writes "
                              "has no other writer (IEEE 1800-2023 9.2.2.2, 9.2.2.4)\n";
                    reported = true;
                }
            }
        }
    }
}

void Lowerer::lowerTop(const ast::InstanceSymbol& inst) {
    const auto instId = static_cast<sir::InstanceId>(design_.instances.size());
    design_.instances.push_back(
        {std::string(inst.name), std::string(inst.getDefinition().name), sir::kNone});
    instance_ = instId;
    std::map<std::string, std::uint32_t> counts;
    // Variables first, so that every procedure finds their slots.
    for (const ast::Symbol& member : inst.body.members()) {
        if (member.kind != ast::SymbolKind::Variable) continue;
        const auto& v = member.as<ast::VariableSymbol>();
        try {
            const sir::SlotId s = slotOf(v, v.location);
            if (v.getInitializer() != nullptr) lowerStaticInit(v, s, v.location);
        } catch (const Unsupported&) {
            continue;  // already reported
        }
    }
    for (const ast::Symbol& member : inst.body.members()) {
        switch (member.kind) {
            case ast::SymbolKind::Variable:
            case ast::SymbolKind::Parameter:
            case ast::SymbolKind::TypeParameter:
            case ast::SymbolKind::TypeAlias:
            case ast::SymbolKind::StatementBlock: continue;  // scopes inside procedures
            case ast::SymbolKind::Subroutine: continue;      // lowered when first called
            case ast::SymbolKind::ProceduralBlock:
                lowerProcedure(member.as<ast::ProceduralBlockSymbol>(), instId, counts);
                continue;
            default:
                report(member.location,
                       "module member '" + std::string(ast::toString(member.kind)) + "'");
        }
    }
    // The static-init process of the instance ends here.
    if (auto it = staticBody_.find(instId); it != staticBody_.end()) {
        Body saved = std::move(b_);
        b_ = std::move(it->second);
        if (!b_.dead) terminate(sir::Op::Ret, {}, inst.location);
        it->second = std::move(b_);
        b_ = std::move(saved);
    }
}

void Lowerer::lowerStaticInit(const ast::VariableSymbol& v, sir::SlotId slot,
                              slang::SourceLocation sl) {
    // Every static initializer of an instance goes, in declaration order,
    // into one process that runs before all others (6.8, SIR_SPEC.md 7.1).
    if (staticInit_.find(instance_) == staticInit_.end()) {
        sir::Process p;
        p.name = design_.instances[instance_].scopeName + ".$static";
        p.kind = sir::ProcessKind::StaticInit;
        p.instance = instance_;
        p.loc = loc(sl);
        p.blocks.resize(1);
        const auto id = static_cast<std::uint32_t>(design_.processes.size());
        design_.processes.push_back(std::move(p));
        staticInit_[instance_] = id;
        Body body;
        body.proc = id;
        body.scale = timeScaleOf(*v.getParentScope());
        staticBody_[instance_] = std::move(body);
    }
    Body saved = std::move(b_);
    b_ = std::move(staticBody_[instance_]);
    const ast::Symbol* savedEval = evalSymbol_;
    evalSymbol_ = &v;
    const auto restore = [&] {
        staticBody_[instance_] = std::move(b_);
        b_ = std::move(saved);
        evalSymbol_ = savedEval;
    };
    try {
        const sir::Type t = design_.slots[slot].type;
        const Val val = coerce(lowerExpr(*v.getInitializer()), t, sl);
        emitVoid(sir::Op::Store, {sir::Operand::slot(slot), val.op}, sl);
        noteWrite({slot, 0, 0, 0, 0});
    } catch (const Unsupported&) {
        restore();
        throw;
    }
    restore();
}

void Lowerer::lowerProcedure(const ast::ProceduralBlockSymbol& pb, sir::InstanceId inst,
                             std::map<std::string, std::uint32_t>& counts) {
    const std::string kindName(ast::toString(pb.procedureKind));
    std::string lower = kindName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "alwayscomb") lower = "always_comb";
    if (lower == "alwayslatch") lower = "always_latch";
    if (lower == "alwaysff") lower = "always_ff";

    sir::Process p;
    p.name = design_.instances[inst].scopeName + "." + lower + std::to_string(counts[lower]++);
    p.instance = inst;
    p.loc = loc(pb.location);
    p.blocks.resize(1);
    const std::size_t first = design_.processes.size();
    design_.processes.push_back(std::move(p));
    b_ = Body{};
    b_.proc = static_cast<std::uint32_t>(first);
    b_.scale = timeScaleOf(*pb.getParentScope());
    evalSymbol_ = &pb;

    const ast::Statement& bodyStmt = pb.getBody();
    try {
        switch (pb.procedureKind) {
            case ast::ProceduralBlockKind::Initial:
                proc().kind = sir::ProcessKind::Initial;
                lowerStatement(bodyStmt);
                if (!b_.dead) terminate(sir::Op::Ret, {}, pb.location);
                break;
            case ast::ProceduralBlockKind::AlwaysComb:
            case ast::ProceduralBlockKind::AlwaysLatch: {
                proc().kind = pb.procedureKind == ast::ProceduralBlockKind::AlwaysComb
                                  ? sir::ProcessKind::Comb
                                  : sir::ProcessKind::Latch;
                lowerStatement(bodyStmt);
                if (!b_.dead) terminate(sir::Op::Ret, {}, pb.location);
                // The sensitivity is made once every call is known
                // (finishDesign).
                combReads_[b_.proc] = b_.reads;
                break;
            }
            case ast::ProceduralBlockKind::Always:
            case ast::ProceduralBlockKind::AlwaysFF: {
                const bool ff = pb.procedureKind == ast::ProceduralBlockKind::AlwaysFF;
                // A leading event control on fixed signals followed by a
                // statement that never waits: ff-plain (or ff), with the
                // event control as the trigger (SIR_SPEC.md 7.1).
                std::vector<sir::Trigger> triggers;
                const ast::Statement* inner = nullptr;
                if (bodyStmt.kind == ast::StatementKind::Timed) {
                    const auto& ts = bodyStmt.as<ast::TimedStatement>();
                    std::vector<const ast::TimingControl*> items;
                    if (ts.timing.kind == ast::TimingControlKind::SignalEvent)
                        items.push_back(&ts.timing);
                    else if (ts.timing.kind == ast::TimingControlKind::EventList)
                        for (const ast::TimingControl* t :
                             ts.timing.as<ast::EventListControl>().events)
                            items.push_back(t);
                    bool fixed = !items.empty() && !mayWait(ts.stmt);
                    for (const ast::TimingControl* t : items) {
                        if (!fixed) break;
                        if (t->kind != ast::TimingControlKind::SignalEvent) {
                            fixed = false;
                            break;
                        }
                        const auto& se = t->as<ast::SignalEventControl>();
                        const std::optional<sir::SlotRange> r = eventSlot(se.expr);
                        if (!r || se.iffCondition != nullptr) {
                            fixed = false;
                            break;
                        }
                        triggers.push_back({*r, edgeOf(se.edge)});
                    }
                    if (fixed) inner = &ts.stmt;
                }
                if (inner != nullptr) {
                    proc().kind = ff ? sir::ProcessKind::Ff : sir::ProcessKind::FfPlain;
                    proc().triggers = std::move(triggers);
                    lowerStatement(*inner);
                    if (!b_.dead) terminate(sir::Op::Ret, {}, pb.location);
                    break;
                }
                if (ff) unsupported(pb.location, "always_ff without a single event control");
                if (!mayWait(bodyStmt))
                    unsupported(pb.location, "always procedure without a timing control");
                // Any other always: a general process that loops forever.
                proc().kind = sir::ProcessKind::General;
                const sir::BlockId loop = newBlock();
                terminate(sir::Op::Br, {sir::Operand::block(loop)}, pb.location);
                setBlock(loop);
                lowerStatement(bodyStmt);
                if (!b_.dead) terminate(sir::Op::Br, {sir::Operand::block(loop)}, pb.location);
                break;
            }
            case ast::ProceduralBlockKind::Final:
                // Runs once when the simulation ends; it cannot wait (9.2.3).
                proc().kind = sir::ProcessKind::Final;
                lowerStatement(bodyStmt);
                if (!b_.dead) terminate(sir::Op::Ret, {}, pb.location);
                break;
        }
    } catch (const Unsupported&) {
        design_.processes.resize(first);  // with any fork children
        combReads_.erase(static_cast<std::uint32_t>(first));
        // A static-init process made while lowering this one goes too.
        for (auto it = staticInit_.begin(); it != staticInit_.end();) {
            if (it->second >= first) {
                staticBody_.erase(it->first);
                it = staticInit_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

CompileResult compile(std::span<const std::string> args) {
    CompileResult result;
    slang::driver::Driver driver;
    driver.addStandardArgs();

    std::vector<const char*> argv;
    argv.push_back("haremasim");
    argv.push_back("--std=1800-2023");  // always IEEE 1800-2023 (CLI.md 2.1)
    for (const auto& a : args)
        argv.push_back(a.c_str());
    if (!driver.parseCommandLine(static_cast<int>(argv.size()), argv.data())) return result;
    if (!driver.processOptions()) return result;

    driver.diagEngine.removeClient(driver.textDiagClient);
    auto client = std::make_shared<slang::TextDiagnosticClient>();
    driver.diagEngine.addClient(client);

    if (!driver.parseAllSources()) {
        result.numErrors = driver.diagEngine.getNumErrors();
        result.diagnostics = client->getString();
        return result;
    }
    std::unique_ptr<ast::Compilation> compilation = driver.createCompilation();
    driver.reportCompilation(*compilation, /* quiet */ true);
    result.numErrors = driver.diagEngine.getNumErrors();
    result.numWarnings = driver.diagEngine.getNumWarnings();
    result.diagnostics = client->getString();
    if (result.numErrors != 0) return result;

    Lowerer lowerer(*compilation->getSourceManager(), globalPrecision(*compilation));
    lowerer.lower(compilation->getRoot());
    result.diagnostics += lowerer.diagnostics();
    if (lowerer.numErrors() != 0) {
        result.numErrors += lowerer.numErrors();
        result.unsupported = true;
        return result;
    }
    result.design = lowerer.takeDesign();
    for (const sir::VerifyError& e : sir::verify(result.design)) {
        result.verifyErrors +=
            "haremasim: internal error: SIR verifier: " + e.where + ": " + e.message + "\n";
    }
    result.success = result.verifyErrors.empty();
    return result;
}

}  // namespace haremasim::front
