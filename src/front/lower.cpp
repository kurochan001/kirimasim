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
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "kirimasim/front/front.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/sir/types.h"
#include "kirimasim/sir/verify.h"
#include "lowerer.h"
#include "slang/analysis/AnalysisManager.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/symbols/PortSymbols.h"
#include "slang/ast/symbols/SubroutineSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/ast/types/NetType.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/driver/Driver.h"
#include "slang/numeric/Time.h"

namespace kirimasim::front {

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

/// The SIR kind of a net (SIR_SPEC.md 3.1), or nullopt for the kinds not
/// supported yet: trireg (VALUE_SEMANTICS.md 5), interconnect and
/// user-defined nettypes.
std::optional<sir::SlotKind> netKindOf(const ast::NetSymbol& n) {
    switch (n.netType.netKind) {
        case ast::NetType::Wire: return sir::SlotKind::Wire;
        case ast::NetType::Tri: return sir::SlotKind::Tri;
        case ast::NetType::WAnd: return sir::SlotKind::Wand;
        case ast::NetType::TriAnd: return sir::SlotKind::Triand;
        case ast::NetType::WOr: return sir::SlotKind::Wor;
        case ast::NetType::TriOr: return sir::SlotKind::Trior;
        case ast::NetType::Tri0: return sir::SlotKind::Tri0;
        case ast::NetType::Tri1: return sir::SlotKind::Tri1;
        case ast::NetType::Supply0: return sir::SlotKind::Supply0;
        case ast::NetType::Supply1: return sir::SlotKind::Supply1;
        case ast::NetType::UWire: return sir::SlotKind::Uwire;
        default: return std::nullopt;
    }
}

/// The strengths of a drive strength specification for 1 and for 0, from
/// 0 (highz) to 7 (supply); strong where none is given (10.3.4, table
/// 28-7).
std::pair<std::uint8_t, std::uint8_t>
driveLevels(std::pair<std::optional<ast::DriveStrength>, std::optional<ast::DriveStrength>> ds) {
    const auto level = [](std::optional<ast::DriveStrength> d) -> std::uint8_t {
        if (!d) return 6;
        switch (*d) {
            case ast::DriveStrength::Supply: return 7;
            case ast::DriveStrength::Strong: return 6;
            case ast::DriveStrength::Pull: return 5;
            case ast::DriveStrength::Weak: return 3;
            case ast::DriveStrength::HighZ: return 0;
        }
        return 6;
    };
    return {level(ds.second), level(ds.first)};  // slang gives (strength0, strength1)
}

/// The lvalue an output port connection names: slang binds it like an
/// output argument, as an assignment whose left side is the lvalue.
const ast::Expression* outputPlace(const ast::Expression* e) {
    if (e->kind == ast::ExpressionKind::Assignment) {
        const auto& a = e->as<ast::AssignmentExpression>();
        if (a.isLValueArg()) return &a.left();
    }
    return e;
}

/// Whether an expression names a net, or a part or element of one, or
/// holds one in a concatenation.
bool namesNet(const ast::Expression& e) {
    switch (e.kind) {
        case ast::ExpressionKind::NamedValue:
        case ast::ExpressionKind::HierarchicalValue: {
            const ast::Symbol* s = namedSymbol(e);
            return s != nullptr && s->kind == ast::SymbolKind::Net;
        }
        case ast::ExpressionKind::ElementSelect:
            return namesNet(e.as<ast::ElementSelectExpression>().value());
        case ast::ExpressionKind::RangeSelect:
            return namesNet(e.as<ast::RangeSelectExpression>().value());
        case ast::ExpressionKind::Conversion:
            return namesNet(e.as<ast::ConversionExpression>().operand());
        case ast::ExpressionKind::Concatenation:
            for (const ast::Expression* op : e.as<ast::ConcatenationExpression>().operands())
                if (namesNet(*op)) return true;
            return false;
        default: return false;
    }
}

/// The kind of the net that a port connection merges (table 23-1): the
/// internal or the external kind.
sir::SlotKind mergedKind(sir::SlotKind internal, sir::SlotKind external) {
    using K = sir::SlotKind;
    const auto group = [](K k) {
        switch (k) {
            case K::Wire:
            case K::Tri: return 0;
            case K::Wand:
            case K::Triand: return 1;
            case K::Wor:
            case K::Trior: return 2;
            case K::Tri0: return 3;
            case K::Tri1: return 4;
            case K::Uwire: return 5;
            case K::Supply0: return 6;
            case K::Supply1: return 7;
            case K::Var: break;
        }
        return 0;
    };
    // Rows internal, columns external, in the order of group(): E external,
    // I internal; e and i are the same with a warning of the table.
    static constexpr const char* kTable[] = {
        "EEEEEEEE",  // wire, tri
        "IEeeeeEE",  // wand, triand
        "IeEeeeEE",  // wor, trior
        "IeeEeeEE",  // tri0
        "IeeeEeEE",  // tri1
        "IiiiiEEE",  // uwire
        "IIIIIIEe",  // supply0
        "IIIIIIeE",  // supply1
    };
    const char c = kTable[group(internal)][group(external)];
    return c == 'E' || c == 'e' ? external : internal;
}

/// Calls f on every element of type T (an instance or a primitive
/// instance) of an array of instances, the elements of nested arrays
/// included, in order (23.3.2, 28.3.6).
template <typename T, typename F>
void forEachArrayElement(const ast::InstanceArraySymbol& a, F&& f) {
    for (const ast::Symbol* e : a.elements) {
        if (e->kind == ast::SymbolKind::InstanceArray)
            forEachArrayElement<T>(e->as<ast::InstanceArraySymbol>(), f);
        else if (T::isKind(e->kind))
            f(e->as<T>());
    }
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
        case ast::StatementKind::Disable:
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

const ast::Symbol* namedSymbol(const ast::Expression& e) {
    if (e.kind != ast::ExpressionKind::NamedValue &&
        e.kind != ast::ExpressionKind::HierarchicalValue)
        return nullptr;
    const ast::Symbol* s = &e.as<ast::ValueExpressionBase>().symbol;
    // A modport port that names a member of the interface instance is that
    // member (25.5).
    if (s->kind == ast::SymbolKind::ModportPort) {
        const ast::Symbol* internal = s->as<ast::ModportPortSymbol>().internalSymbol;
        if (internal != nullptr) return internal;
    }
    return s;
}

const ast::UnpackedStructType* unpackedStruct(const ast::Type& t) {
    const ast::Type& c = t.getCanonicalType();
    return c.kind == ast::SymbolKind::UnpackedStructType ? &c.as<ast::UnpackedStructType>()
                                                         : nullptr;
}

const ast::UnpackedStructType* structElement(const ast::Type& t) {
    const ast::Type* c = &t.getCanonicalType();
    while (c->kind == ast::SymbolKind::FixedSizeUnpackedArrayType)
        c = &c->as<ast::FixedSizeUnpackedArrayType>().elementType.getCanonicalType();
    return unpackedStruct(*c);
}

void structLeaves(const ast::Type& t, const std::string& prefix, std::vector<Leaf>& out,
                  const std::vector<sir::ArrayDim>& outer) {
    // An array of structures adds its dimensions to every leaf of the
    // structure (SIR_SPEC.md 4.2).
    if (structElement(t) == nullptr) {
        out.push_back({prefix, &t, outer});
        return;
    }
    std::vector<sir::ArrayDim> dims = outer;
    const ast::Type* c = &t.getCanonicalType();
    while (c->kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& a = c->as<ast::FixedSizeUnpackedArrayType>();
        dims.push_back({a.range.left, a.range.right});
        c = &a.elementType.getCanonicalType();
    }
    for (const ast::FieldSymbol* f : unpackedStruct(*c)->fields)
        structLeaves(f->getType(), prefix + "." + std::string(f->name), out, dims);
}

std::uint32_t leafCount(const ast::Type& t) {
    const ast::UnpackedStructType* st = structElement(t);
    if (st == nullptr) return 1;
    std::uint32_t n = 0;
    for (const ast::FieldSymbol* f : st->fields)
        n += leafCount(f->getType());
    return n;
}

std::uint32_t leafOffset(const ast::UnpackedStructType& st, const ast::FieldSymbol& f) {
    std::uint32_t n = 0;
    for (const ast::FieldSymbol* g : st.fields) {
        if (g == &f) break;
        n += leafCount(g->getType());
    }
    return n;
}

bool inSlot(const ast::Symbol& s) {
    if (s.kind == ast::SymbolKind::Net) return true;
    if (s.kind != ast::SymbolKind::Variable && s.kind != ast::SymbolKind::FormalArgument)
        return false;
    return s.as<ast::VariableSymbol>().lifetime == ast::VariableLifetime::Static;
}

// --- Diagnostics and types ---------------------------------------------------

void Lowerer::unsupported(slang::SourceLocation sl, std::string_view what) {
    report(sl, what);
    throw Unsupported{};
}

void Lowerer::error(slang::SourceLocation sl, std::string_view what) {
    ++numErrors_;
    if (sl.valid())
        diags_ += std::string(sm_.getFileName(sl)) + ":" + std::to_string(sm_.getLineNumber(sl)) +
                  ":" + std::to_string(sm_.getColumnNumber(sl)) + ": ";
    else
        diags_ += "kirimasim: ";
    diags_ += "error: ";
    diags_ += what;
    diags_ += "\n";
    throw Unsupported{};
}

void Lowerer::report(slang::SourceLocation sl, std::string_view what) {
    ++numErrors_;
    if (sl.valid()) {
        diags_ += std::string(sm_.getFileName(sl)) + ":" + std::to_string(sm_.getLineNumber(sl)) +
                  ":" + std::to_string(sm_.getColumnNumber(sl)) + ": ";
    } else {
        diags_ += "kirimasim: ";
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
    // A handle to a kernel event (SIR_SPEC.md 3.2.1).
    if (t.isEvent()) return sir::Type{sir::TypeKind::Event, 0};
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

Val Lowerer::blockParam(sir::BlockId b, sir::Type t) {
    const sir::TempId p = newTemp();
    blocks()[b].params.push_back({p, t});
    return {sir::Operand::temp(p), t};
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

std::string Lowerer::scopeNameOf(const ast::Symbol& sym) const {
    // The scope a declaration or assignment is in, for %m and $error
    // (SYSTEM_TASKS.md 3.8): the instance, subroutine, named block or
    // generate block; unnamed blocks are not part of the hierarchy.
    for (const ast::Scope* sc = sym.getParentScope(); sc != nullptr;
         sc = sc->asSymbol().getParentScope()) {
        const ast::Symbol& s = sc->asSymbol();
        if (s.kind == ast::SymbolKind::StatementBlock && s.name.empty()) continue;
        if (s.kind == ast::SymbolKind::InstanceBody || s.kind == ast::SymbolKind::Package ||
            s.kind == ast::SymbolKind::CompilationUnit)
            break;
        return s.getHierarchicalPath();
    }
    return design_.instances[instance_].scopeName;
}

sir::SlotId Lowerer::slotOf(const ast::ValueSymbol& v, slang::SourceLocation sl) {
    if (auto it = slots_.find(&v); it != slots_.end()) return it->second;
    if (structElement(v.getType()) != nullptr)
        unsupported(sl, "unpacked structure used as a whole here");
    const sir::SlotId id = newSlot(v, "", v.getType(), {}, sl);
    slots_.emplace(&v, id);
    return id;
}

sir::SlotId Lowerer::structSlots(const ast::ValueSymbol& v, slang::SourceLocation sl) {
    // One slot per innermost member, in declaration order (SIR_SPEC.md 4.2).
    if (auto it = structSlots_.find(&v); it != structSlots_.end()) return it->second;
    if (v.kind == ast::SymbolKind::Net) unsupported(sl, "net of an unpacked structure type");
    std::vector<Leaf> leaves;
    structLeaves(v.getType(), "", leaves);
    const auto first = static_cast<sir::SlotId>(design_.slots.size());
    for (const Leaf& l : leaves)
        newSlot(v, l.path, *l.type, l.dims, sl);
    structSlots_.emplace(&v, first);
    return first;
}

sir::SlotId Lowerer::newSlot(const ast::ValueSymbol& v, const std::string& member,
                             const ast::Type& type, const std::vector<sir::ArrayDim>& outer,
                             slang::SourceLocation sl) {
    // A variable or net of a package or of a compilation unit ($unit,
    // 3.12.1) belongs to its row, one in the design (SIR_SPEC.md 3.3).
    const std::optional<sir::InstanceId> row = rowOf(v);
    sir::SlotKind kind = sir::SlotKind::Var;
    if (v.kind == ast::SymbolKind::Net) {
        const auto& n = v.as<ast::NetSymbol>();
        const std::optional<sir::SlotKind> k = netKindOf(n);
        if (!k) unsupported(sl, "net of kind '" + std::string(n.netType.name) + "'");
        if (n.getDelay() != nullptr) unsupported(sl, "net with a delay");
        kind = *k;
    }
    const ast::Type* t = &type.getCanonicalType();
    std::vector<sir::ArrayDim> dims = outer;
    while (t->kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& a = t->as<ast::FixedSizeUnpackedArrayType>();
        dims.push_back({a.range.left, a.range.right});
        t = &a.elementType.getCanonicalType();
    }
    if (t->isUnpackedArray()) unsupported(sl, "variable of type '" + type.toString() + "'");
    sir::Slot s;
    // Static variables of unnamed blocks share a hierarchical path; the
    // second and later get "~2", "~3", ... (names are for diagnostics and
    // the text form, and must be unique; SIR_SPEC.md 3.1). A member of an
    // unpacked structure adds its path, ".x" (SIR_SPEC.md 4.2).
    s.name = v.getHierarchicalPath();
    if (row && v.getParentScope() == &instanceSyms_[*row]->as<ast::Scope>())
        s.name = design_.instances[*row].scopeName + "::" + std::string(v.name);
    s.name += member;
    if (const std::uint32_t n = ++slotNames_[s.name]; n > 1) s.name += "~" + std::to_string(n);
    s.type = typeOf(*t, sl);
    s.kind = kind;
    s.dims = std::move(dims);
    s.instance = row.value_or(instance_);
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
    return id;
}

sir::FrameVarId Lowerer::frameVarOf(const ast::VariableSymbol& v, slang::SourceLocation sl) {
    if (auto it = b_.frameVars.find(&v); it != b_.frameVars.end()) return it->second;
    // A fixed-size unpacked array keeps its dimensions; its elements follow
    // in the frame (SIR_SPEC.md 7.4).
    std::vector<sir::ArrayDim> dims;
    const ast::Type& et = arrayElement(v.getType(), dims);
    if (et.isUnpackedArray() || (!dims.empty() && unpackedStruct(et) != nullptr))
        unsupported(sl, "automatic variable of type '" + v.getType().toString() + "'");
    const sir::Type t = typeOf(et, sl);
    // An event slot gets its event when the design starts (SIR_SPEC.md
    // 3.2.1); a frame would need one each time it is made.
    if (t.kind == sir::TypeKind::Event) unsupported(sl, "automatic event variable");
    const auto id = static_cast<sir::FrameVarId>(frameVars().size());
    frameVars().push_back({std::string(v.name), t, sir::kNone, std::move(dims), {}});
    b_.frameVars.emplace(&v, id);
    return id;
}

const ast::Type& arrayElement(const ast::Type& type, std::vector<sir::ArrayDim>& dims) {
    const ast::Type* t = &type.getCanonicalType();
    while (t->kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& a = t->as<ast::FixedSizeUnpackedArrayType>();
        dims.push_back({a.range.left, a.range.right});
        t = &a.elementType.getCanonicalType();
    }
    return *t;
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
    std::vector<sir::SlotRange>& writes = b_.isSub ? subWrites_[b_.proc] : proc().writes;
    for (const sir::SlotRange& w : writes)
        if (sir::covers(s, w, r)) return;
    writes.push_back(r);
}

void Lowerer::noteRead(const sir::SlotRange& r) { b_.reads.push_back(r); }

// --- Modules and procedures --------------------------------------------------

void Lowerer::lower(const ast::RootSymbol& root) {
    design_.timePrecision = precision_;
    declarePackages(root);
    for (const ast::InstanceSymbol* top : root.topInstances)
        declareInstance(*top, sir::kNone);
    for (const ast::InstanceSymbol* top : root.topInstances)
        lowerInstance(*top);
    // The static-init processes end here: the static variables of blocks
    // inside procedures add to them while the procedures are lowered.
    for (auto& [inst, body] : staticBody_) {
        Body saved = std::move(b_);
        b_ = std::move(body);
        // A compilation unit has no location of its own; its first
        // initializer's will do.
        slang::SourceLocation end = instanceSyms_[inst]->location;
        if (!end.valid()) end = staticLoc_.at(inst);
        if (!b_.dead) terminate(sir::Op::Ret, {}, end);
        body = std::move(b_);
        b_ = std::move(saved);
    }
    if (numErrors_ != 0) return;
    finishDesign();
    resolveNets();
}

std::string Lowerer::whereOf(const sir::SourceLoc& l) const {
    const std::string file = l.file < design_.sources.size() ? design_.sources[l.file] : "?";
    return file + ":" + std::to_string(l.line) + ":" + std::to_string(l.column);
}

std::string Lowerer::processName(const std::string& prefix, const std::string& kind) {
    const std::string key = prefix + "." + kind;
    return key + std::to_string(nameCounts_[key]++);
}

void Lowerer::dropProcesses(std::size_t first) {
    design_.processes.resize(first);  // with any fork children
    combReads_.erase(static_cast<std::uint32_t>(first));
    strengths_.erase(strengths_.lower_bound(static_cast<std::uint32_t>(first)), strengths_.end());
    netPorts_.erase(netPorts_.lower_bound(static_cast<std::uint32_t>(first)), netPorts_.end());
    sides_.erase(sides_.lower_bound(static_cast<std::uint32_t>(first)), sides_.end());
    // A static-init process made while lowering the dropped ones goes too.
    for (auto it = staticInit_.begin(); it != staticInit_.end();) {
        if (it->second >= first) {
            staticBody_.erase(it->first);
            staticLoc_.erase(it->first);
            it = staticInit_.erase(it);
        } else {
            ++it;
        }
    }
}

void Lowerer::finishDesign() {
    const auto add = [&](std::vector<sir::SlotRange>& set, const sir::SlotRange& r) {
        const sir::Slot& s = design_.slots[r.slot];
        for (const sir::SlotRange& w : set)
            if (sir::covers(s, w, r)) return false;
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
            // Variables and arguments declared in a called function are not
            // part of it either.
            bool skip = s.subroutine != sir::kNone;
            for (const sir::SlotRange& w : proc.writes)
                skip = skip || sir::covers(s, w, r);
            for (const sir::Trigger& t : proc.triggers)
                skip = skip || sir::covers(s, t.range, r);
            if (!skip) proc.triggers.push_back({r, sir::Edge::Any});
        }
    }
}

void Lowerer::resolveNets() {
    // The drivers of each net: the cont processes that write it (a
    // continuous assignment, a net declaration assignment or a port).
    // Two drivers of a uwire never get here: slang's analysis rejects them
    // (6.6.2).
    std::map<sir::SlotId, std::vector<std::pair<std::uint32_t, sir::SlotRange>>> drivers;
    for (std::uint32_t p = 0; p < design_.processes.size(); ++p) {
        if (design_.processes[p].kind != sir::ProcessKind::Cont) continue;
        for (const sir::SlotRange& w : design_.processes[p].writes)
            if (design_.slots[w.slot].kind != sir::SlotKind::Var) drivers[w.slot].push_back({p, w});
    }
    for (const auto& [net, writes] : drivers) {
        // Only nets with bits that several drivers write, or with a driver
        // and their own pull or supply, are resolved (VALUE_SEMANTICS.md
        // 5.5). Where drivers write separate bits, each bit has the value
        // of its one driver either way.
        const sir::SlotKind kind = design_.slots[net].kind;
        const bool own = kind == sir::SlotKind::Tri0 || kind == sir::SlotKind::Tri1 ||
                         kind == sir::SlotKind::Supply0 || kind == sir::SlotKind::Supply1;
        bool shared = false;
        for (std::size_t i = 0; i < writes.size() && !shared; ++i)
            for (std::size_t j = i + 1; j < writes.size() && !shared; ++j)
                shared = writes[i].first != writes[j].first &&
                         sir::overlaps(design_.slots[net], writes[i].second, writes[j].second);
        if (!shared && !own) continue;
        std::vector<std::uint32_t> ps;
        for (const auto& w : writes)
            if (std::find(ps.begin(), ps.end(), w.first) == ps.end()) ps.push_back(w.first);
        // Drivers that meet through a port between nets resolve on one net
        // with their own strengths (VALUE_SEMANTICS.md 5.3); that needs
        // the nets merged, not done yet.
        const auto through = std::find_if(ps.begin(), ps.end(),
                                          [&](std::uint32_t p) { return netPorts_.contains(p); });
        if (through != ps.end()) {
            ++numErrors_;
            diags_ += whereOf(design_.processes[*through].loc) +
                      ": error: unsupported: " + "drivers of the net '" + design_.slots[net].name +
                      "' that meet through a port (not implemented yet)\n";
            continue;
        }
        const sir::SourceLoc where = design_.processes[ps.front()].loc;
        if (!design_.slots[net].dims.empty()) {
            ++numErrors_;
            diags_ += whereOf(where) + ": error: unsupported: an array of nets with " +
                      "several drivers (not implemented yet)\n";
            continue;
        }
        const auto unknownSide = std::find_if(ps.begin(), ps.end(), [&](std::uint32_t p) {
            const auto it = sides_.find(p);
            return it != sides_.end() && !it->second.known;
        });
        if (unknownSide != ps.end()) {
            ++numErrors_;
            diags_ +=
                whereOf(design_.processes[*unknownSide].loc) +
                ": error: unsupported: a three-state gate that drives this part of the net '" +
                design_.slots[net].name + "' with other drivers (not implemented yet)\n";
            continue;
        }
        const sir::Type type = design_.slots[net].type;
        const sir::InstanceId instance = design_.slots[net].instance;
        design_.slots[net].flags |= sir::kMultiDriver;

        // Each driver writes its own slot, which starts at z (SIR_SPEC.md
        // 3.5): the bits it does not write do not drive.
        std::vector<sir::SlotId> slots;
        std::vector<sir::SlotId> sideSlots;
        for (std::size_t k = 0; k < ps.size(); ++k) {
            sir::Slot d;
            d.name = design_.slots[net].name + ".$driver" + std::to_string(k);
            d.type = type;
            d.kind = sir::SlotKind::Wire;
            d.instance = instance;
            d.driverOf = net;
            const auto it = strengths_.find(ps[k]);
            if (it != strengths_.end()) std::tie(d.strength1, d.strength0) = it->second;
            const auto id = static_cast<sir::SlotId>(design_.slots.size());
            design_.slots.push_back(std::move(d));
            slots.push_back(id);
            sir::Process& p = design_.processes[ps[k]];
            for (sir::BasicBlock& bb : p.blocks)
                for (sir::Instr& in : bb.instrs)
                    if (sir::writesSlot(in.op) && !in.operands.empty() &&
                        in.operands[0].kind == sir::Operand::Kind::Slot && in.operands[0].id == net)
                        in.operands[0].id = id;
            for (sir::SlotRange& w : p.writes)
                if (w.slot == net) w.slot = id;
            // A three-state gate's sides go to a side slot of twice the
            // width (SIR_SPEC.md 3.5), which starts with no side driven.
            const auto side = sides_.find(ps[k]);
            if (side == sides_.end()) {
                sideSlots.push_back(sir::kNone);
                continue;
            }
            sir::Slot ss;
            ss.name = design_.slots[net].name + ".$side" + std::to_string(k);
            ss.type = sir::u2(2 * type.width);
            ss.instance = instance;
            const auto sid = static_cast<sir::SlotId>(design_.slots.size());
            design_.slots.push_back(std::move(ss));
            sideSlots.push_back(sid);
            std::vector<sir::Instr>& instrs = p.blocks[0].instrs;
            for (std::size_t i = side->second.first; i < side->second.first + side->second.count;
                 ++i) {
                if (instrs[i].op != sir::Op::Store) continue;
                sir::Operand& o = instrs[i].operands[0];
                o.id = sid;
                p.writes.push_back({sid, 0, 0, o.lsb, o.width});
            }
            sides_.erase(side);
        }

        // The net's value is resolved again whenever a driver changes
        // (VALUE_SEMANTICS.md 5.6).
        sir::Process r;
        r.name = design_.slots[net].name + ".$resolve";
        r.kind = sir::ProcessKind::Cont;
        r.instance = instance;
        r.loc = where;
        r.blocks.resize(1);
        std::vector<sir::Operand> ops{sir::Operand::str(std::string(sir::kindName(kind)))};
        for (std::size_t k = 0; k < slots.size(); ++k) {
            const sir::TempId t = r.numTemps++;
            r.blocks[0].instrs.push_back(
                {sir::Op::Load, type, t, {sir::Operand::slot(slots[k])}, where});
            r.triggers.push_back({sir::SlotRange{slots[k], 0, 0, 0, 0}, sir::Edge::Any});
            ops.push_back(sir::Operand::temp(t));
            ops.push_back(constant(sir::u2(3), design_.slots[slots[k]].strength1).op);
            ops.push_back(constant(sir::u2(3), design_.slots[slots[k]].strength0).op);
            if (sideSlots[k] == sir::kNone) continue;
            const sir::TempId s = r.numTemps++;
            r.blocks[0].instrs.push_back({sir::Op::Load,
                                          design_.slots[sideSlots[k]].type,
                                          s,
                                          {sir::Operand::slot(sideSlots[k])},
                                          where});
            r.triggers.push_back({sir::SlotRange{sideSlots[k], 0, 0, 0, 0}, sir::Edge::Any});
            ops.push_back(sir::Operand::temp(s));
        }
        const sir::TempId v = r.numTemps++;
        r.blocks[0].instrs.push_back({sir::Op::Resolve, type, v, std::move(ops), where});
        r.blocks[0].instrs.push_back({sir::Op::Store,
                                      sir::voidType(),
                                      sir::kNone,
                                      {sir::Operand::slot(net), sir::Operand::temp(v)},
                                      where});
        r.blocks[0].instrs.push_back({sir::Op::Ret, sir::voidType(), sir::kNone, {}, where});
        r.writes.push_back({net, 0, 0, 0, 0});
        design_.processes.push_back(std::move(r));
    }
    // A three-state gate on a net that does not resolve writes L and H as
    // x alone (SIR_SPEC.md 3.5): its side computation goes.
    for (const auto& [proc, side] : sides_) {
        std::vector<sir::Instr>& instrs = design_.processes[proc].blocks[0].instrs;
        const auto at = instrs.begin() + static_cast<std::ptrdiff_t>(side.first);
        instrs.erase(at, at + static_cast<std::ptrdiff_t>(side.count));
    }
    sides_.clear();
}

void Lowerer::declarePackages(const ast::RootSymbol& root) {
    // The rows of the packages and compilation units with variables or
    // nets first, in source order: a package where it is declared, a
    // compilation unit at its first variable or net. One with subroutines
    // alone gets its row when one of them is lowered (rowFor); it has
    // nothing to initialize.
    const auto holds = [](const ast::Symbol& m) {
        return m.kind == ast::SymbolKind::Variable || m.kind == ast::SymbolKind::Net;
    };
    const auto holdsAny = [&](const ast::Scope& sc) {
        for (const ast::Symbol& m : sc.members())
            if (holds(m)) return true;
        return false;
    };
    std::vector<const ast::Symbol*> order;
    for (const ast::CompilationUnitSymbol* unit : root.compilationUnits) {
        for (const ast::Symbol& m : unit->members()) {
            if (m.kind == ast::SymbolKind::Package && holdsAny(m.as<ast::PackageSymbol>()))
                order.push_back(&m);
            else if (holds(m) && std::find(order.begin(), order.end(), unit) == order.end())
                order.push_back(unit);
        }
    }
    for (const ast::PackageSymbol* pkg : root.getCompilation().getPackages())
        if (std::find(order.begin(), order.end(), pkg) == order.end() && holdsAny(*pkg))
            order.push_back(pkg);
    for (const ast::Symbol* sym : order)
        rowFor(*sym);
    // Then their slots and static initializers, in the same order.
    for (const ast::Symbol* sym : order) {
        instance_ = scopeRows_.at(sym);
        declareScope(sym->as<ast::Scope>());
    }
    instance_ = 0;
}

sir::InstanceId Lowerer::rowFor(const ast::Symbol& scope) {
    if (const auto it = scopeRows_.find(&scope); it != scopeRows_.end()) return it->second;
    const bool unit = scope.kind == ast::SymbolKind::CompilationUnit;
    // Compilation units are per file; the second and later are $unit#2,
    // ... (SIR_SPEC.md 7.1).
    std::string name(scope.name);
    if (unit) name = ++units_ == 1 ? "$unit" : "$unit#" + std::to_string(units_);
    const auto id = static_cast<sir::InstanceId>(design_.instances.size());
    design_.instances.push_back(
        {name, name, sir::kNone, unit ? sir::InstanceKind::Unit : sir::InstanceKind::Package});
    instanceSyms_.push_back(&scope);
    scopeRows_[&scope] = id;
    return id;
}

const ast::Symbol* Lowerer::packageOf(const ast::Symbol& s) {
    for (const ast::Scope* sc = s.getParentScope(); sc != nullptr;
         sc = sc->asSymbol().getParentScope()) {
        const ast::Symbol& owner = sc->asSymbol();
        if (owner.kind == ast::SymbolKind::InstanceBody) return nullptr;
        if (owner.kind == ast::SymbolKind::Package ||
            owner.kind == ast::SymbolKind::CompilationUnit)
            return &owner;
    }
    return nullptr;
}

std::optional<sir::InstanceId> Lowerer::rowOf(const ast::Symbol& s) {
    const ast::Symbol* scope = packageOf(s);
    if (scope == nullptr) return std::nullopt;
    return rowFor(*scope);
}

void Lowerer::declareInstance(const ast::InstanceSymbol& inst, sir::InstanceId parent) {
    const auto id = static_cast<sir::InstanceId>(design_.instances.size());
    design_.instances.push_back(
        {inst.getHierarchicalPath(), std::string(inst.getDefinition().name), parent});
    instanceIds_[&inst] = id;
    instanceSyms_.push_back(&inst);
    mergePorts(inst);  // before the instance's own nets get slots
    const sir::InstanceId saved = instance_;
    instance_ = id;
    declareScope(inst.body);
    instance_ = saved;
}

void Lowerer::mergePorts(const ast::InstanceSymbol& child) {
    // A port between whole nets of the same type is one net: the child's
    // net shares the parent's slot, whatever the direction (SIR_SPEC.md
    // 3.5, 23.3.3.7).
    for (const ast::PortConnection* conn : child.getPortConnections()) {
        if (conn->port.kind != ast::SymbolKind::Port) continue;
        const auto& ps = conn->port.as<ast::PortSymbol>();
        const ast::Expression* ext = conn->getExpression();
        if (ext == nullptr || ps.internalSymbol == nullptr ||
            ps.internalSymbol->kind != ast::SymbolKind::Net)
            continue;
        const auto& inner = ps.internalSymbol->as<ast::NetSymbol>();
        const ast::Expression* outer = outputPlace(ext);
        if (outer->kind != ast::ExpressionKind::NamedValue &&
            outer->kind != ast::ExpressionKind::HierarchicalValue)
            continue;
        const ast::Symbol* os = namedSymbol(*outer);
        if (os == nullptr || os->kind != ast::SymbolKind::Net) continue;
        const std::optional<sir::SlotKind> innerKind = netKindOf(inner);
        if (!innerKind || inner.getDelay() != nullptr || slots_.contains(&inner)) continue;
        const slang::SourceLocation sl = outer->sourceRange.start();
        try {
            const sir::SlotId s = slotOf(os->as<ast::ValueSymbol>(), sl);
            sir::Slot& slot = design_.slots[s];
            if (!slot.dims.empty() || inner.getType().getCanonicalType().isUnpackedArray() ||
                slot.type != typeOf(inner.getType(), sl))
                continue;
            // slang gives the warnings of table 23-1 (-Wnet-inconsistent).
            slot.kind = mergedKind(*innerKind, slot.kind);
            slots_.emplace(&inner, s);
            design_.names.push_back({inner.getHierarchicalPath(), s});
            mergedPorts_.insert(conn);
            mergedSlots_.insert(s);
        } catch (const Unsupported&) {
            continue;  // already reported
        }
    }
}

void Lowerer::declareScope(const ast::Scope& scope) {
    for (const ast::Symbol& member : scope.members()) {
        switch (member.kind) {
            case ast::SymbolKind::Variable:
            case ast::SymbolKind::Net: {
                const auto& v = member.as<ast::ValueSymbol>();
                try {
                    if (structElement(v.getType()) != nullptr) {
                        // One slot per member; the initial value or the
                        // members' defaults (7.2.2) in static-init.
                        const sir::SlotId first = structSlots(v, v.location);
                        if (member.kind == ast::SymbolKind::Variable &&
                            (v.getInitializer() != nullptr || hasMemberDefaults(v.getType())))
                            lowerStaticInit(member.as<ast::VariableSymbol>(), first, v.location);
                        break;
                    }
                    const sir::SlotId s = slotOf(v, v.location);
                    // A net's initializer is a continuous assignment
                    // (10.3.1), lowered with the processes.
                    if (member.kind == ast::SymbolKind::Variable && v.getInitializer() != nullptr)
                        lowerStaticInit(member.as<ast::VariableSymbol>(), s, v.location);
                } catch (const Unsupported&) {
                    continue;  // already reported
                }
                break;
            }
            case ast::SymbolKind::Instance:
                declareInstance(member.as<ast::InstanceSymbol>(), instance_);
                break;
            case ast::SymbolKind::InstanceArray:
                forEachArrayElement<ast::InstanceSymbol>(
                    member.as<ast::InstanceArraySymbol>(),
                    [&](const ast::InstanceSymbol& i) { declareInstance(i, instance_); });
                break;
            case ast::SymbolKind::GenerateBlock:
                if (!member.as<ast::GenerateBlockSymbol>().isUninstantiated)
                    declareScope(member.as<ast::GenerateBlockSymbol>());
                break;
            case ast::SymbolKind::GenerateBlockArray:
                for (const ast::GenerateBlockSymbol* g :
                     member.as<ast::GenerateBlockArraySymbol>().entries)
                    if (!g->isUninstantiated) declareScope(*g);
                break;
            default: break;
        }
    }
}

void Lowerer::lowerInstance(const ast::InstanceSymbol& inst) {
    const sir::InstanceId saved = instance_;
    instance_ = instanceIds_.at(&inst);
    lowerScope(inst.body, design_.instances[instance_].scopeName);
    instance_ = saved;
}

void Lowerer::lowerScope(const ast::Scope& scope, const std::string& prefix) {
    for (const ast::Symbol& member : scope.members()) {
        switch (member.kind) {
            case ast::SymbolKind::Variable:   // declareScope made the slot
            case ast::SymbolKind::Parameter:  // constants, used where they are named
            case ast::SymbolKind::TypeParameter:
            case ast::SymbolKind::TypeAlias:
            case ast::SymbolKind::ForwardingTypedef:
            case ast::SymbolKind::TransparentMember:  // an enum value made visible
            case ast::SymbolKind::Genvar:             // used up by elaboration
            case ast::SymbolKind::ExplicitImport:     // names resolved by slang
            case ast::SymbolKind::WildcardImport:
            case ast::SymbolKind::EmptyMember:
            case ast::SymbolKind::Port:           // connected by the parent (lowerPorts)
            case ast::SymbolKind::InterfacePort:  // names through it reach the interface
            case ast::SymbolKind::Modport:        // a view of the interface's members (25.5)
            case ast::SymbolKind::StatementBlock: continue;  // scopes inside procedures
            case ast::SymbolKind::Subroutine: continue;      // lowered when first called
            case ast::SymbolKind::Net: {
                const auto& n = member.as<ast::NetSymbol>();
                const ast::Expression* init = n.getInitializer();
                const auto it = slots_.find(&n);
                if (init == nullptr || it == slots_.end()) continue;  // none, or reported
                const sir::SlotId s = it->second;
                // A net declaration assignment is a continuous assignment
                // to the net (10.3.1).
                lowerCont(
                    processName(prefix, "assign"), n.location, n,
                    [&] {
                        if (!design_.slots[s].dims.empty())
                            unsupported(n.location, "assignment to a whole unpacked array");
                        const Val v = coerce(lowerExpr(*init), design_.slots[s].type, n.location);
                        emitVoid(sir::Op::Store, {sir::Operand::slot(s), v.op}, n.location);
                        noteWrite({s, 0, 0, 0, 0});
                    },
                    driveLevels(n.getDriveStrength()));
                continue;
            }
            case ast::SymbolKind::ContinuousAssign: {
                const auto& ca = member.as<ast::ContinuousAssignSymbol>();
                lowerCont(
                    processName(prefix, "assign"), ca.location, ca,
                    [&] {
                        if (ca.getDelay() != nullptr)
                            unsupported(ca.location, "continuous assignment with a delay");
                        const ast::Expression& e = ca.getAssignment();
                        if (e.kind != ast::ExpressionKind::Assignment)
                            unsupported(ca.location, "this continuous assignment");
                        lowerAssignment(e.as<ast::AssignmentExpression>());
                    },
                    driveLevels(ca.getDriveStrength()));
                continue;
            }
            case ast::SymbolKind::ProceduralBlock:
                lowerProcedure(member.as<ast::ProceduralBlockSymbol>(), prefix);
                continue;
            case ast::SymbolKind::Instance: {
                const auto& child = member.as<ast::InstanceSymbol>();
                lowerPorts(child);
                lowerInstance(child);
                continue;
            }
            case ast::SymbolKind::InstanceArray:
                // Each element is an instance or a gate; slang gives each its
                // slice of the connections (23.3.3.5, 28.3.6).
                forEachArrayElement<ast::InstanceSymbol>(member.as<ast::InstanceArraySymbol>(),
                                                         [&](const ast::InstanceSymbol& i) {
                                                             lowerPorts(i);
                                                             lowerInstance(i);
                                                         });
                forEachArrayElement<ast::PrimitiveInstanceSymbol>(
                    member.as<ast::InstanceArraySymbol>(),
                    [&](const ast::PrimitiveInstanceSymbol& g) { lowerPrimitive(g, prefix); });
                continue;
            case ast::SymbolKind::PrimitiveInstance:
                lowerPrimitive(member.as<ast::PrimitiveInstanceSymbol>(), prefix);
                continue;
            case ast::SymbolKind::GenerateBlock: {
                const auto& g = member.as<ast::GenerateBlockSymbol>();
                if (!g.isUninstantiated) lowerScope(g, g.getHierarchicalPath());
                continue;
            }
            case ast::SymbolKind::GenerateBlockArray:
                for (const ast::GenerateBlockSymbol* g :
                     member.as<ast::GenerateBlockArraySymbol>().entries)
                    if (!g->isUninstantiated) lowerScope(*g, g->getHierarchicalPath());
                continue;
            default:
                report(member.location,
                       "module member '" + std::string(ast::toString(member.kind)) + "'");
        }
    }
}

void Lowerer::lowerPorts(const ast::InstanceSymbol& child) {
    // Each connection is an implicit continuous assignment (4.9.6): an
    // input from the parent's expression to the port's net or variable,
    // an output the other way (SIR_SPEC.md 3.5; folding is P2).
    const std::string scope = design_.instances[instanceIds_.at(&child)].scopeName;
    for (const ast::PortConnection* conn : child.getPortConnections()) {
        const ast::Symbol& port = conn->port;
        // An interface port connects no value; names through it reach the
        // interface instance directly (25.3).
        if (port.kind == ast::SymbolKind::InterfacePort) continue;
        if (port.kind != ast::SymbolKind::Port) {
            report(port.location, "port of kind '" + std::string(ast::toString(port.kind)) + "'");
            continue;
        }
        if (mergedPorts_.contains(conn)) continue;  // one net with the parent's (mergePorts)
        const auto& ps = port.as<ast::PortSymbol>();
        const ast::Expression* ext = conn->getExpression();
        if (ext == nullptr) {
            // Unconnected: the port's net or variable keeps its initial
            // value, unless the port has a default (23.2.2.4).
            if (ps.getInitializer() != nullptr)
                report(ps.location, "default value of an unconnected port");
            continue;
        }
        const ast::Symbol* internal = ps.internalSymbol;
        if (internal == nullptr || !inSlot(*internal)) {
            report(ps.location, "port connected to an expression inside the module");
            continue;
        }
        const auto it = slots_.find(internal);
        if (it == slots_.end()) continue;  // its declaration was reported
        const sir::SlotId s = it->second;
        const slang::SourceLocation sl = ext->sourceRange.start();
        const std::string name = scope + ".$port." + std::string(ps.name);
        // A port between nets passes strengths both ways (VALUE_SEMANTICS.md
        // 5.3), which these continuous assignments do not do.
        const std::size_t before = design_.processes.size();
        const bool betweenNets =
            internal->kind == ast::SymbolKind::Net &&
            namesNet(ps.direction == ast::ArgumentDirection::Out ? *outputPlace(ext) : *ext);
        switch (ps.direction) {
            case ast::ArgumentDirection::In:
                lowerCont(name, sl, child, [&] {
                    if (!design_.slots[s].dims.empty()) unsupported(sl, "unpacked array port");
                    const Val v = coerce(lowerExpr(*ext), design_.slots[s].type, sl);
                    emitVoid(sir::Op::Store, {sir::Operand::slot(s), v.op}, sl);
                    noteWrite({s, 0, 0, 0, 0});
                });
                break;
            case ast::ArgumentDirection::Out:
                lowerCont(name, sl, child, [&] {
                    if (!design_.slots[s].dims.empty()) unsupported(sl, "unpacked array port");
                    const ast::Expression& place = *outputPlace(ext);
                    const Val v =
                        emit(sir::Op::Load, design_.slots[s].type, {sir::Operand::slot(s)}, sl);
                    noteRead({s, 0, 0, 0, 0});
                    lowerStore(place, coerce(v, typeOf(*place.type, sl), sl), false, sl);
                });
                break;
            case ast::ArgumentDirection::InOut: report(ps.location, "inout port"); break;
            case ast::ArgumentDirection::Ref: report(ps.location, "ref port"); break;
        }
        if (betweenNets && design_.processes.size() > before)
            netPorts_.insert(static_cast<std::uint32_t>(before));
    }
}

void Lowerer::lowerPrimitive(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix) {
    // A built-in gate or pull is a cont process that computes its outputs
    // from its inputs whenever one changes (VALUE_SEMANTICS.md 5.8,
    // SIR_SPEC.md 3.5, 7.1).
    const ast::PrimitiveSymbol& prim = g.primitiveType;
    const std::string kind(prim.name);
    if (prim.primitiveKind == ast::PrimitiveSymbol::UserDefined) {
        report(g.location, "user-defined primitive");
        return;
    }
    static const std::set<std::string, std::less<>> kLogic = {"and", "nand", "or",
                                                              "nor", "xor",  "xnor"};
    static const std::set<std::string, std::less<>> kTri = {"bufif0", "bufif1", "notif0", "notif1"};
    const bool pull = kind == "pullup" || kind == "pulldown";
    const bool logic = kLogic.contains(kind);
    const bool tri = kTri.contains(kind);
    if (!logic && !tri && !pull && kind != "buf" && kind != "not") {
        // MOS switches and bidirectional switches are phase 4
        // (VALUE_SEMANTICS.md 5.1).
        report(g.location, "switch '" + kind + "'");
        return;
    }
    const std::span<const ast::Expression* const> conns = g.getPortConnections();
    if (conns.empty()) return;  // slang reported the connections
    const auto ds = g.getDriveStrength();
    std::pair<std::uint8_t, std::uint8_t> strength = driveLevels(ds);
    if (pull) {
        // A pull drives pull strength unless one is given (28.10).
        if (!ds.second) strength.first = 5;
        if (!ds.first) strength.second = 5;
    }
    const std::string name = g.name.empty() && g.arrayPath.empty() ? processName(prefix, "gate")
                                                                   : g.getHierarchicalPath();
    const slang::SourceLocation sl = g.location;
    const auto first = static_cast<std::uint32_t>(design_.processes.size());
    std::optional<GateSide> side;
    lowerCont(
        name, sl, g,
        [&] {
            if (g.getDelay() != nullptr) unsupported(sl, "gate with a delay");
            const sir::Type bit = sir::u4(1);
            const auto in = [&](std::size_t i) { return coerce(lowerExpr(*conns[i]), bit, sl); };
            const auto out = [&](std::size_t i, const Val& v) {
                const Place p = placeOf(*outputPlace(conns[i]), sl);
                storePlace(p, coerce(v, p.type, sl), false, sl);
                return p;
            };
            const auto op2 = [&](sir::Op op, const Val& a, const Val& b) {
                return emit(op, a.type, {a.op, b.op}, sl);
            };
            const auto inv = [&](const Val& a) { return emit(sir::Op::Not, a.type, {a.op}, sl); };
            // An input z reads as x (28.4, 28.5): z ^ 0 is x.
            const auto known = [&](const Val& a) { return op2(sir::Op::Xor, a, constant(bit, 0)); };
            if (pull) {
                out(0, constant(bit, kind == "pullup" ? 1 : 0));
                return;
            }
            if (logic) {
                // The bitwise operator over the inputs in order (28.4).
                const sir::Op op = kind == "and" || kind == "nand" ? sir::Op::And
                                   : kind == "or" || kind == "nor" ? sir::Op::Or
                                                                   : sir::Op::Xor;
                Val v = known(in(1));
                for (std::size_t i = 2; i < conns.size(); ++i)
                    v = op2(op, v, in(i));
                if (kind == "nand" || kind == "nor" || kind == "xnor") v = inv(v);
                out(0, v);
                return;
            }
            if (!tri) {
                // buf and not: the last terminal is the input, the others
                // outputs (28.5).
                const Val x = in(conns.size() - 1);
                const Val v = kind == "buf" ? known(x) : inv(x);
                for (std::size_t i = 0; i + 1 < conns.size(); ++i)
                    out(i, v);
                return;
            }
            // bufif0, bufif1, notif0, notif1 (28.6, table 28-5): the data
            // when the control enables, z when it disables, and L or H (x
            // with one side, VALUE_SEMANTICS.md 5.8) when it is x or z.
            const bool if1 = kind.back() == '1';
            const Val data = kind.starts_with("not") ? inv(in(1)) : known(in(1));
            const Val ctrl = in(2);
            const Val on =
                emit(sir::Op::Ceq, sir::u2(1), {ctrl.op, constant(bit, if1 ? 1 : 0).op}, sl);
            const Val off =
                emit(sir::Op::Ceq, sir::u2(1), {ctrl.op, constant(bit, if1 ? 0 : 1).op}, sl);
            const Val v = emit(sir::Op::Select, bit,
                               {on.op, data.op,
                                emit(sir::Op::Select, bit,
                                     {off.op, constant(bit, 0, 1).op, constant(bit, 1, 1).op}, sl)
                                    .op},
                               sl);
            const Place p = out(0, v);
            // The sides it drives, for the net's side slot when the net
            // resolves (SIR_SPEC.md 3.5): no 1 side for L, no 0 side for H.
            // resolveNets keeps these instructions or removes them.
            const bool net = p.kind == Place::Kind::Slot || p.kind == Place::Kind::Bits;
            if (!net || design_.slots[p.slot].kind == sir::SlotKind::Var) return;
            const std::uint32_t w = design_.slots[p.slot].type.width;
            const bool fixed = p.kind == Place::Kind::Slot
                                   ? w == 1
                                   : !p.pos && p.indices.empty() && p.width == 1 && p.lsb >= 0 &&
                                         p.lsb < static_cast<std::int64_t>(w);
            if (!fixed || !design_.slots[p.slot].dims.empty() || 2 * w > 64) {
                side = GateSide{0, 0, false};
                return;
            }
            const auto at = static_cast<std::uint32_t>(p.kind == Place::Kind::Slot ? 0 : p.lsb);
            const std::size_t start = blocks()[b_.block].instrs.size();
            const sir::Type flag = sir::u2(1);
            const Val unknown = inv(op2(sir::Op::Or, on, off));
            const auto is = [&](std::uint64_t value) {
                return emit(sir::Op::Ceq, flag, {data.op, constant(bit, value).op}, sl);
            };
            const Val one = inv(op2(sir::Op::And, unknown, is(0)));
            const Val zero = inv(op2(sir::Op::And, unknown, is(1)));
            emitVoid(sir::Op::Store, {sir::Operand::slot(sir::kNone, at, 1), one.op}, sl);
            emitVoid(sir::Op::Store, {sir::Operand::slot(sir::kNone, w + at, 1), zero.op}, sl);
            side = GateSide{start, blocks()[b_.block].instrs.size() - start, true};
        },
        strength);
    if (side && design_.processes.size() > first) sides_[first] = *side;
}

template <typename F>
void Lowerer::lowerCont(const std::string& name, slang::SourceLocation sl,
                        const ast::Symbol& evalAt, F&& body,
                        std::pair<std::uint8_t, std::uint8_t> strength) {
    sir::Process p;
    p.name = name;
    p.kind = sir::ProcessKind::Cont;
    p.instance = instance_;
    p.loc = loc(sl);
    p.blocks.resize(1);
    const std::size_t first = design_.processes.size();
    design_.processes.push_back(std::move(p));
    b_ = Body{};
    b_.proc = static_cast<std::uint32_t>(first);
    b_.scale = timeScaleOf(*evalAt.getParentScope());
    b_.scope = scopeNameOf(evalAt);
    evalSymbol_ = &evalAt;
    try {
        body();
        if (!b_.dead) terminate(sir::Op::Ret, {}, sl);
    } catch (const Unsupported&) {
        dropProcesses(first);
        return;
    }
    if (strength != std::pair<std::uint8_t, std::uint8_t>{6, 6})
        strengths_[static_cast<std::uint32_t>(first)] = strength;
    // Evaluated again whenever an operand of the right-hand side changes
    // (10.3.2).
    triggerOnReads();
}

void Lowerer::triggerOnReads() {
    for (const sir::SlotRange& r : b_.reads) {
        const sir::Slot& s = design_.slots[r.slot];
        if (s.subroutine != sir::kNone) continue;
        bool dup = false;
        for (const sir::Trigger& t : proc().triggers)
            dup = dup || sir::covers(s, t.range, r);
        if (!dup) proc().triggers.push_back({r, sir::Edge::Any});
    }
}

void Lowerer::lowerStaticInit(const ast::VariableSymbol& v, sir::SlotId slot,
                              slang::SourceLocation sl) {
    // Every static initializer of an instance goes, in declaration order,
    // into one process that runs before all others (6.8, SIR_SPEC.md 7.1).
    if (staticInit_.find(instance_) == staticInit_.end()) {
        sir::Process p;
        // <instance>.$static; <package>::$static and $unit::$static (SIR_SPEC.md 7.1).
        p.name = design_.instances[instance_].scopeName +
                 (design_.instances[instance_].kind == sir::InstanceKind::Module ? ".$static"
                                                                                 : "::$static");
        p.kind = sir::ProcessKind::StaticInit;
        p.instance = instance_;
        p.loc = loc(sl);
        p.blocks.resize(1);
        const auto id = static_cast<std::uint32_t>(design_.processes.size());
        design_.processes.push_back(std::move(p));
        staticInit_[instance_] = id;
        staticLoc_[instance_] = sl;
        Body body;
        body.proc = id;
        body.scale = timeScaleOf(*v.getParentScope());
        staticBody_[instance_] = std::move(body);
    }
    Body saved = std::move(b_);
    b_ = std::move(staticBody_[instance_]);
    b_.scope = scopeNameOf(v);
    const ast::Symbol* savedEval = evalSymbol_;
    evalSymbol_ = &v;
    const auto restore = [&] {
        staticBody_[instance_] = std::move(b_);
        b_ = std::move(saved);
        evalSymbol_ = savedEval;
    };
    try {
        if (structElement(v.getType()) != nullptr) {
            // The initializer, or else the members' defaults (7.2.2).
            Stores stores;
            Storage dst;
            dst.id = slot;
            if (const ast::Expression* init = v.getInitializer())
                assignAggregate(dst, v.getType(), *init, stores, sl);
            else
                memberDefaults(dst, v.getType(), stores, sl);
            flushStores(stores, false, sl);
        } else if (!design_.slots[slot].dims.empty()) {
            Stores stores;
            assignArray(slot, {}, *v.getInitializer(), stores, sl);  // element by element
            flushStores(stores, false, sl);
        } else {
            const sir::Type t = design_.slots[slot].type;
            const Val val = coerce(lowerExpr(*v.getInitializer()), t, sl);
            emitVoid(sir::Op::Store, {sir::Operand::slot(slot), val.op}, sl);
            noteWrite({slot, 0, 0, 0, 0});
        }
    } catch (const Unsupported&) {
        restore();
        throw;
    }
    restore();
}

void Lowerer::lowerProcedure(const ast::ProceduralBlockSymbol& pb, const std::string& prefix) {
    const std::string kindName(ast::toString(pb.procedureKind));
    std::string lower = kindName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "alwayscomb") lower = "always_comb";
    if (lower == "alwayslatch") lower = "always_latch";
    if (lower == "alwaysff") lower = "always_ff";

    sir::Process p;
    p.name = processName(prefix, lower);
    p.instance = instance_;
    p.loc = loc(pb.location);
    p.blocks.resize(1);
    const std::size_t first = design_.processes.size();
    design_.processes.push_back(std::move(p));
    b_ = Body{};
    b_.proc = static_cast<std::uint32_t>(first);
    b_.scale = timeScaleOf(*pb.getParentScope());
    b_.scope = prefix;
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
                if (bodyStmt.kind == ast::StatementKind::Timed &&
                    bodyStmt.as<ast::TimedStatement>().timing.kind ==
                        ast::TimingControlKind::ImplicitEvent &&
                    !mayWait(bodyStmt.as<ast::TimedStatement>().stmt)) {
                    // always @* with a statement that never waits: comb-at,
                    // sensitive to what the statement reads (SIR_SPEC.md
                    // 7.1, SCHEDULER_SPEC.md 9.2): not what it only writes,
                    // and not what the functions it calls read inside.
                    proc().kind = sir::ProcessKind::CombAt;
                    lowerStatement(bodyStmt.as<ast::TimedStatement>().stmt);
                    if (!b_.dead) terminate(sir::Op::Ret, {}, pb.location);
                    triggerOnReads();
                    break;
                }
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
        dropProcesses(first);
    }
}

CompileResult compile(std::span<const std::string> args) {
    CompileResult result;
    slang::driver::Driver driver;
    driver.addStandardArgs();

    std::vector<const char*> argv;
    argv.push_back("kirimasim");
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
    // slang's analysis checks the writers and drivers of variables and nets
    // (SCHEDULER_SPEC.md 6.4, 12), here as for `kirimasim parse`.
    driver.runAnalysis(*compilation);
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
            "kirimasim: internal error: SIR verifier: " + e.where + ": " + e.message + "\n";
    }
    result.success = result.verifyErrors.empty();
    return result;
}

}  // namespace kirimasim::front
