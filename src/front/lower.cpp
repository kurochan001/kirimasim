// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
#include <limits>
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
#include "kirimasim/sir/strength.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/sir/types.h"
#include "kirimasim/sir/verify.h"
#include "lowerer.h"
#include "query_checks.h"
#include "slang/analysis/AnalysisManager.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/symbols/ClassSymbols.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/symbols/PortSymbols.h"
#include "slang/ast/symbols/SpecifySymbols.h"
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
        case ast::StatementKind::ImmediateAssertion: {
            // The action blocks of a simple assertion run here (16.3); the
            // action of a deferred one runs later, not in this thread (16.4).
            const auto& a = s.as<ast::ImmediateAssertionStatement>();
            if (a.isDeferred) return false;
            return (a.ifTrue != nullptr && mayWaitIn(*a.ifTrue, seen)) ||
                   (a.ifFalse != nullptr && mayWaitIn(*a.ifFalse, seen));
        }
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

bool hasEventLeaves(const ast::Type& t) {
    if (t.isEvent()) return true;
    if (t.isArray()) return hasEventLeaves(*t.getArrayElementType());
    if (const auto* structure = unpackedStruct(t))
        for (const auto* field : structure->fields)
            if (hasEventLeaves(field->getType())) return true;
    return false;
}

void structLeaves(const ast::Type& t, const std::string& prefix, std::vector<Leaf>& out,
                  const std::vector<sir::ArrayDim>& outer, bool initialized) {
    // An array of structures adds its dimensions to every leaf of the
    // structure (SIR_SPEC.md 4.2).
    if (structElement(t) == nullptr) {
        out.push_back({prefix, &t, outer, initialized});
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
        structLeaves(f->getType(), prefix + "." + std::string(f->name), out, dims,
                     initialized || f->getInitializer() != nullptr);
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
    if (!isVariable(s)) return false;
    return s.as<ast::VariableSymbol>().lifetime == ast::VariableLifetime::Static;
}

// --- Diagnostics and types ---------------------------------------------------

void Lowerer::unsupported(slang::SourceLocation sl, std::string_view what) {
    report(sl, what);
    throw Unsupported{};
}

void Lowerer::error(slang::SourceLocation sl, std::string_view what, slang::SourceLocation noteAt,
                    std::string_view note) {
    reportError(sl, what, noteAt, note);
    throw Unsupported{};
}

void Lowerer::reportError(slang::SourceLocation sl, std::string_view what,
                          slang::SourceLocation noteAt, std::string_view note) {
    ++numErrors_;
    const auto line = [&](slang::SourceLocation at, std::string_view kind, std::string_view text) {
        if (at.valid())
            diags_ += std::string(sm_.getFileName(at)) + ":" +
                      std::to_string(sm_.getLineNumber(at)) + ":" +
                      std::to_string(sm_.getColumnNumber(at)) + ": ";
        else
            diags_ += "kirimasim: ";
        diags_ += kind;
        diags_ += text;
        diags_ += '\n';
    };
    line(sl, "error: ", what);
    if (noteAt.valid()) line(noteAt, "note: ", note);
}

void Lowerer::warning(slang::SourceLocation sl, std::string_view what) {
    if (std::find(warned_.begin(), warned_.end(), sl) != warned_.end()) return;
    warned_.push_back(sl);
    ++numWarnings_;
    if (sl.valid())
        diags_ += std::string(sm_.getFileName(sl)) + ":" + std::to_string(sm_.getLineNumber(sl)) +
                  ":" + std::to_string(sm_.getColumnNumber(sl)) + ": ";
    else
        diags_ += "kirimasim: ";
    diags_ += "warning: ";
    diags_ += what;
    diags_ += '\n';
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
    if (isDynamicContainer(t)) return containerType(t, sl);
    if (t.isString()) return sir::Type{sir::TypeKind::String, 0};
    // A handle to a kernel event (SIR_SPEC.md 3.2.1).
    if (t.isEvent()) return sir::Type{sir::TypeKind::Event, 0};
    // A chandle holds null, 0, until DPI (VALUE_SEMANTICS.md 7.8); null
    // itself is one.
    if (t.isCHandle() || t.isNull()) return sir::Type{sir::TypeKind::Chandle, 0};
    if (t.isFloating())
        return t.as<ast::FloatingType>().floatKind == ast::FloatingType::ShortReal
                   ? sir::Type{sir::TypeKind::ShortReal, 0}
                   : sir::Type{sir::TypeKind::Real, 0};  // real and realtime (6.12)
    if (!t.isIntegral()) unsupported(sl, "type '" + t.toString() + "'");
    const std::uint32_t w = t.getBitWidth();
    if (w == 0) unsupported(sl, "a value of no bits");
    if (t.isFourState()) return t.isSigned() ? sir::s4(w) : sir::u4(w);
    return t.isSigned() ? sir::s2(w) : sir::u2(w);
}

// --- Building blocks ---------------------------------------------------------

sir::BlockId Lowerer::newBlock() {
    blocks().emplace_back();
    return sir::BlockId{static_cast<std::uint32_t>(blocks().size() - 1)};
}

Val Lowerer::blockParam(sir::BlockId b, sir::Type t) {
    const sir::TempId p = newTemp();
    block(b).params.push_back({p, t});
    return {sir::Operand::temp(p), t};
}

Val Lowerer::emit(sir::Op op, sir::Type t, std::vector<sir::Operand> ops,
                  slang::SourceLocation sl) {
    const sir::TempId r = newTemp();
    block(b_.block).instrs.push_back({op, t, r, std::move(ops), loc(sl)});
    return {sir::Operand::temp(r), t};
}

void Lowerer::emitVoid(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl) {
    block(b_.block).instrs.push_back({op, sir::voidType(), sir::kNone, std::move(ops), loc(sl)});
}

void Lowerer::emitInto(sir::BlockId b, sir::Op op, std::vector<sir::Operand> ops,
                       slang::SourceLocation sl) {
    block(b).instrs.push_back({op, sir::voidType(), sir::kNone, std::move(ops), loc(sl)});
}

void Lowerer::terminate(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl) {
    emitVoid(op, std::move(ops), sl);
    b_.dead = true;
}

Val Lowerer::constant(sir::Type t, std::uint64_t a, std::uint64_t b) {
    // A wider type gets a and b as its low word, zero above.
    const std::size_t words = sir::isIntegral(t) ? sir::integralWords(t.width) : 1;
    std::vector<std::uint64_t> aw(words, 0);
    std::vector<std::uint64_t> bw(words, 0);
    aw[0] = a;
    bw[0] = b;
    return constantWords(t, std::move(aw), std::move(bw));
}

Val Lowerer::allXConstant(sir::Type t) {
    const std::size_t words = sir::integralWords(t.width);
    return constantWords(t, std::vector<std::uint64_t>(words, ~std::uint64_t{0}),
                         std::vector<std::uint64_t>(words, ~std::uint64_t{0}));
}

Val Lowerer::constantWords(sir::Type t, std::vector<std::uint64_t> a,
                           std::vector<std::uint64_t> b) {
    // The bits above the width are 0 (SIR_SPEC.md 6).
    const std::uint32_t top = sir::isIntegral(t) ? t.width % 64 : 0;
    if (top != 0) {
        a.back() &= (std::uint64_t{1} << top) - 1;
        b.back() &= (std::uint64_t{1} << top) - 1;
    }
    sir::Constant c;
    c.type = t;
    c.aWords = std::move(a);
    if (sir::isFourState(t)) c.bWords = std::move(b);
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
    return design_.instance(instance_).scopeName;
}

sir::SlotId Lowerer::slotOf(const ast::ValueSymbol& v, slang::SourceLocation sl) {
    if (auto it = slots_.find(&v); it != slots_.end()) return it->second;
    // A static variable of a subroutine belongs to it (SIR_SPEC.md 3.3),
    // whether the subroutine or a hierarchical name from outside it (13.3.1,
    // 23.6) is lowered first: the subroutine is lowered here, which makes
    // the variable when its body uses it.
    std::optional<sir::SubroutineId> owner;
    if (const ast::SubroutineSymbol* sub = declaringSubroutine(v);
        sub != nullptr && (!b_.isSub || b_.subSymbol != sub)) {
        owner = subroutineOf(*sub, sl);
        if (auto it = slots_.find(&v); it != slots_.end()) return it->second;
    }
    if (structElement(v.getType()) != nullptr)
        unsupported(sl, "unpacked structure used as a whole here");
    const sir::SlotId id = newSlot(v, "", v.getType(), {}, sl);
    if (owner) design_.slot(id).subroutine = *owner;
    slots_.emplace(&v, id);
    return id;
}

const ast::SubroutineSymbol* Lowerer::declaringSubroutine(const ast::Symbol& s) {
    for (const ast::Scope* sc = s.getParentScope(); sc != nullptr;
         sc = sc->asSymbol().getParentScope()) {
        const ast::Symbol& scope = sc->asSymbol();
        if (scope.kind == ast::SymbolKind::Subroutine) return &scope.as<ast::SubroutineSymbol>();
        if (scope.kind == ast::SymbolKind::InstanceBody) return nullptr;
    }
    return nullptr;
}

std::optional<sir::InstanceId> Lowerer::declaringInstance(const ast::Symbol& s) {
    if (const std::optional<sir::InstanceId> row = rowOf(s)) return row;
    for (const ast::Scope* sc = s.getParentScope(); sc != nullptr;
         sc = sc->asSymbol().getParentScope()) {
        if (sc->asSymbol().kind != ast::SymbolKind::InstanceBody) continue;
        const ast::InstanceSymbol* inst =
            sc->asSymbol().as<ast::InstanceBodySymbol>().parentInstance;
        if (const auto it = instanceIds_.find(inst); it != instanceIds_.end()) return it->second;
        return std::nullopt;
    }
    return std::nullopt;
}

sir::SlotId Lowerer::paramSlot(const ast::ParameterSymbol& p, slang::SourceLocation sl) {
    if (auto it = slots_.find(&p); it != slots_.end()) return it->second;
    std::vector<sir::ArrayDim> dims;
    const ast::Type& et = arrayElement(p.getType(), dims);
    if (unpackedStruct(et) != nullptr || et.isUnpackedArray())
        unsupported(sl, "parameter of type '" + p.getType().toString() + "' at a run-time index");
    // Its scope: a package or compilation unit, or else the instance whose
    // body declares it.
    const sir::InstanceId saved = instance_;
    instance_ = declaringInstance(p).value_or(instance_);
    const sir::SlotId id = slotOf(p, sl);
    design_.slot(id).instance = instance_;
    // The elements in slot order: the rightmost dimension fastest (4.2).
    std::vector<const slang::ConstantValue*> values;
    const auto flatten = [&](const slang::ConstantValue& cv, auto& self) -> void {
        if (!cv.isUnpacked()) {
            values.push_back(&cv);
            return;
        }
        for (const slang::ConstantValue& e : cv.elements())
            self(e, self);
    };
    flatten(p.getValue(), flatten);
    if (values.size() != sir::elementCount(design_.slot(id))) {
        instance_ = saved;
        unsupported(sl, "the value of parameter '" + std::string(p.name) + "'");
    }
    // The stores are the declaration's.
    const slang::SourceLocation at = p.location;
    ensureStaticInit(p, at);
    Body body = std::move(b_);
    b_ = std::move(staticBody_[instance_]);
    const auto restore = [&] {
        staticBody_[instance_] = std::move(b_);
        b_ = std::move(body);
        instance_ = saved;
    };
    try {
        const sir::Type t = design_.slot(id).type;
        for (std::size_t i = 0; i < values.size(); ++i) {
            sir::Operand o = sir::Operand::slot(id);
            o.firstElem = static_cast<std::uint32_t>(i);
            o.numElems = 1;
            emitVoid(sir::Op::Store, {o, constantFrom(*values[i], t, at).op}, at);
        }
        noteWrite({id, 0, 0, 0, 0});
    } catch (const Unsupported&) {
        restore();
        throw;
    }
    restore();
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
        newSlot(v, l.path, *l.type, l.dims, sl, !l.initialized);
    structSlots_.emplace(&v, first);
    return first;
}

sir::SlotId Lowerer::newSlot(const ast::ValueSymbol& v, const std::string& member,
                             const ast::Type& type, const std::vector<sir::ArrayDim>& outer,
                             slang::SourceLocation sl, bool defaultEligible) {
    if (v.kind == ast::SymbolKind::ClassProperty) {
        for (const ast::Scope* scope = v.getParentScope(); scope != nullptr;
             scope = scope->asSymbol().getParentScope()) {
            if (scope->asSymbol().kind != ast::SymbolKind::ClassType) continue;
            const auto& classType = scope->asSymbol().as<ast::ClassType>();
            if (classType.genericClass != nullptr || classType.isUninstantiated)
                unsupported(sl, "static property of a parameterized class");
        }
    }
    // A variable or net of a package or of a compilation unit ($unit,
    // 3.12.1) belongs to its row, one in the design (SIR_SPEC.md 3.3).
    const std::optional<sir::InstanceId> row = rowOf(v);
    sir::SlotKind kind = sir::SlotKind::Var;
    if (v.kind == ast::SymbolKind::Net) {
        const auto& n = v.as<ast::NetSymbol>();
        const std::optional<sir::SlotKind> k = netKindOf(n);
        if (!k) unsupported(sl, "net of kind '" + std::string(n.netType.name) + "'");
        // A delay given with an assignment is the assignment's (10.3.3);
        // one for every driver of the net is not done yet.
        if (n.getDelay() != nullptr && n.getInitializer() == nullptr)
            unsupported(sl, "net with a delay");
        kind = *k;
    }
    const ast::Type* t = &type.getCanonicalType();
    std::vector<sir::ArrayDim> dims = outer;
    while (t->kind == ast::SymbolKind::FixedSizeUnpackedArrayType) {
        const auto& a = t->as<ast::FixedSizeUnpackedArrayType>();
        dims.push_back({a.range.left, a.range.right});
        t = &a.elementType.getCanonicalType();
    }
    if (t->isUnpackedArray() && !isDynamicContainer(*t))
        unsupported(sl, "variable of type '" + type.toString() + "'");
    sir::Slot s;
    // Static variables of unnamed blocks share a hierarchical path; the
    // second and later get "~2", "~3", ... (names are for diagnostics and
    // the text form, and must be unique; SIR_SPEC.md 3.1). A member of an
    // unpacked structure adds its path, ".x" (SIR_SPEC.md 4.2).
    s.name = v.getHierarchicalPath();
    if (row && v.getParentScope() == &instanceSyms_[row->index()]->as<ast::Scope>())
        s.name = design_.instance(*row).scopeName + "::" + std::string(v.name);
    s.name += member;
    if (const std::uint32_t n = ++slotNames_[s.name]; n > 1) s.name += "~" + std::to_string(n);
    s.type = typeOf(*t, sl);
    s.kind = kind;
    if (initValues_ && defaultEligible && kind == sir::SlotKind::Var && sir::isFourState(s.type) &&
        isVariable(v)) {
        const auto& variable = v.as<ast::VariableSymbol>();
        s.defaultInit =
            v.kind == ast::SymbolKind::FormalArgument
                ? v.as<ast::FormalArgumentSymbol>().direction == ast::ArgumentDirection::Out
                : variable.getInitializer() == nullptr;
    }
    s.dims = std::move(dims);
    if (sir::elementCount(s) == 0 ||
        sir::elementCount(s) > std::numeric_limits<std::uint32_t>::max())
        unsupported(sl, "a fixed array beyond the SIR element-address range");
    s.instance = row.value_or(instance_);
    // A variable or argument of the subroutine being lowered belongs to it.
    if (b_.isSub) {
        for (const ast::Scope* sc = v.getParentScope(); sc != nullptr;
             sc = sc->asSymbol().getParentScope()) {
            if (&sc->asSymbol() == b_.subSymbol) {
                // A guard's or watch's variables are those of the task that waits.
                // subSymbol is set here only when the waiter is a task, so its
                // frameOf names a subroutine.
                const sir::Subroutine& cur = design_.subroutine(b_.sub);
                s.subroutine =
                    sir::readsWaiterFrame(cur.kind) ? cur.frameOf.asSubroutine() : b_.sub;
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
    if ((et.isUnpackedArray() && !isDynamicContainer(et)) ||
        (!dims.empty() && unpackedStruct(et) != nullptr))
        unsupported(sl, "automatic variable of type '" + v.getType().toString() + "'");
    const sir::Type t = typeOf(et, sl);
    // Input events share their actual's object. Copyback would rebind an
    // existing event variable and remains outside the approved contract.
    if (t.kind == sir::TypeKind::Event && v.kind == ast::SymbolKind::FormalArgument &&
        v.as<ast::FormalArgumentSymbol>().direction != ast::ArgumentDirection::In)
        unsupported(sl, "automatic event argument");
    const auto id = static_cast<sir::FrameVarId>(frameVars().size());
    frameVars().push_back({std::string(v.name), t, sir::kNone, std::move(dims), {}});
    b_.frameVars.emplace(&v, id);
    ownFrame(v, id);
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

void Lowerer::noteWrite(const sir::SlotRange& logical) {
    const sir::SlotRange r = sir::physicalRange(design_, logical);
    const sir::Slot& s = design_.slot(r.slot);
    std::vector<sir::SlotRange>& writes = b_.isSub ? subWrites_[b_.sub] : proc().writes;
    for (const sir::SlotRange& w : writes)
        if (sir::covers(s, w, r)) return;
    writes.push_back(r);
}

void Lowerer::noteRead(const sir::SlotRange& r) {
    if (b_.actions == 0) b_.reads.push_back(sir::physicalRange(design_, r));
}

// --- Modules and procedures --------------------------------------------------

void Lowerer::lower(const ast::RootSymbol& root) {
    design_.timePrecision = precision_;
    root_ = &root;
    scanDisables(root);
    // Anonymous programs hold items that only programs use (24.6).
    for (const ast::CompilationUnitSymbol* unit : root.compilationUnits) {
        for (const ast::Symbol& m : unit->members()) {
            reportDpiImport(m);
        }
    }
    for (const ast::PackageSymbol* pkg : root.getCompilation().getPackages()) {
        for (const ast::Symbol& m : pkg->members()) {
            reportDpiImport(m);
        }
    }
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
        slang::SourceLocation end = instanceSyms_[inst.index()]->location;
        if (!end.valid()) end = staticLoc_.at(inst);
        if (!b_.dead) terminate(sir::Op::Ret, {}, end);
        body = std::move(b_);
        b_ = std::move(saved);
    }
    checkStaticInitOrder();
    if (numErrors_ == 0) checkGenericClassStatics();
    buildDumpTable();
    if (numErrors_ != 0) return;
    finishDesign();
    connectPartialNetPorts();
    prepareStrengthNets();
    if (numErrors_ != 0) return;
    resolveNets();
    finishStrengthNets();
    finishDrives();
}

std::string Lowerer::sourceText(slang::SourceRange r) const {
    const slang::SourceRange o = sm_.getFullyOriginalRange(r);
    if (o.start().buffer() != o.end().buffer() || o.end().offset() < o.start().offset()) return {};
    const std::string_view text = sm_.getSourceText(o.start().buffer());
    if (o.end().offset() > text.size()) return {};
    return std::string(text.substr(o.start().offset(), o.end().offset() - o.start().offset()));
}

std::string Lowerer::whereOf(const sir::SourceLoc& l) const {
    const std::string file = l.file < design_.sources.size() ? design_.sources[l.file] : "?";
    return file + ":" + std::to_string(l.line) + ":" + std::to_string(l.column);
}

std::string Lowerer::processName(const std::string& prefix, const std::string& kind) {
    const std::string key = prefix + "." + kind;
    return key + std::to_string(nameCounts_[key]++);
}

void Lowerer::dropProcesses(sir::ProcessId first) {
    design_.processes.resize(first.index());  // with any fork children
    combReads_.erase(first);
    combLocals_.erase(first);
    implicitLocations_.erase(implicitLocations_.lower_bound(first), implicitLocations_.end());
    implicitOwnReads_.erase(implicitOwnReads_.lower_bound(first), implicitOwnReads_.end());
    strengths_.erase(strengths_.lower_bound(first), strengths_.end());
    netPorts_.erase(netPorts_.lower_bound(first), netPorts_.end());
    sides_.erase(sides_.lower_bound(first), sides_.end());
    // A static-init process made while lowering the dropped ones goes too.
    std::erase_if(staticInits_, [&](const auto& s) { return s.second >= first; });
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
        const sir::Slot& s = design_.slot(r.slot);
        for (const sir::SlotRange& w : set)
            if (sir::covers(s, w, r)) return false;
        set.push_back(r);
        return true;
    };
    // What each subroutine writes, with the functions it calls, and what it
    // reads, with the functions it calls outside assertion action blocks,
    // until nothing changes (recursion included). A task's body is no part
    // of its caller's, for what it writes (SIR_SPEC.md 3.4) or reads
    // (SCHEDULER_SPEC.md 6.3), nor is a deferred assertion's action, which
    // runs after the caller (16.4.1).
    const auto later = [&](sir::SubroutineId c) {
        return design_.subroutine(c).kind == sir::SubroutineKind::Task || actions_.contains(c);
    };
    std::map<sir::SubroutineId, std::vector<sir::SlotRange>> writes = subWrites_;
    std::map<sir::SubroutineId, std::vector<sir::SlotRange>> reads = subReads_;
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto& [sub, called] : callees_.bySubroutine)
            for (const sir::SubroutineId c : called)
                if (!later(c))
                    for (const sir::SlotRange& r : std::vector(writes[c]))
                        changed = add(writes[sub], r) || changed;
        for (const auto& [sub, called] : readCallees_.bySubroutine)
            for (const sir::SubroutineId c : called)
                for (const sir::SlotRange& r : std::vector(reads[c]))
                    changed = add(reads[sub], r) || changed;
    }
    // A process writes what the functions it calls write; always_comb and
    // always_latch are sensitive to what they read too (SCHEDULER_SPEC.md
    // 6.3, 6.4). A cont process takes them after its own writes are known as
    // what it drives (finishDrives).
    for (const auto& [p, called] : callees_.byProcess) {
        const bool cont = design_.process(p).kind == sir::ProcessKind::Cont;
        for (const sir::SubroutineId c : called)
            if (!later(c))
                for (const sir::SlotRange& r : writes[c])
                    add(cont ? callWrites_[p] : design_.process(p).writes, r);
    }
    for (const auto& [p, called] : readCallees_.byProcess) {
        if (combReads_.count(p) == 0) continue;
        for (const sir::SubroutineId c : called)
            for (const sir::SlotRange& r : reads[c])
                combReads_[p].push_back(r);
    }
    // The implicit sensitivity: what the block reads, less what it writes
    // (9.2.2.2.1, SCHEDULER_SPEC.md 6.3).
    for (const auto& [p, procReads] : combReads_) {
        sir::Process& proc = design_.process(p);
        for (const sir::SlotRange& r : procReads) {
            const sir::Slot& s = design_.slot(r.slot);
            // Variables declared by this block or a called function, and
            // function arguments, are not part of its sensitivity either.
            bool skip = s.subroutine != sir::kNone || combLocals_[p].contains(r.slot);
            for (const sir::SlotRange& w : proc.writes)
                skip = skip || sir::covers(s, w, r);
            for (const sir::Trigger& t : proc.triggers)
                skip = skip || sir::covers(s, t.range, r);
            if (!skip) proc.triggers.push_back({r, sir::Edge::Any});
        }
    }
    // Static trigger ranges do not describe heap cells. Until implicit
    // sensitivity has a typed heap encoding, diagnose it instead of running
    // a process that misses element changes. Explicit guards/watches use
    // the scheduler's heap monitor path.
    for (const auto& [id, at] : implicitLocations_) {
        const auto& process = design_.process(id);
        bool heapRead = false;
        for (const auto& trigger : process.triggers)
            heapRead = heapRead || sir::isContainer(design_.slot(trigger.range.slot).type);
        for (const auto& read : implicitOwnReads_[id]) {
            const auto& slot = design_.slot(read.slot);
            bool written = combLocals_[id].contains(read.slot);
            if (process.kind == sir::ProcessKind::Comb || process.kind == sir::ProcessKind::Latch)
                for (const auto& write : process.writes)
                    written = written || sir::covers(slot, write, read);
            if (!written && sir::isContainer(slot.type)) heapRead = true;
        }
        std::set<sir::SubroutineId> called;
        std::vector<sir::SubroutineId> pending;
        if (const auto found = readCallees_.byProcess.find(id);
            found != readCallees_.byProcess.end())
            pending = found->second;
        while (!pending.empty()) {
            const auto sub = pending.back();
            pending.pop_back();
            if (!called.insert(sub).second) continue;
            if (const auto nested = readCallees_.bySubroutine.find(sub);
                nested != readCallees_.bySubroutine.end())
                pending.insert(pending.end(), nested->second.begin(), nested->second.end());
        }
        for (const auto sub : called)
            for (const auto& read : reads[sub]) {
                const auto& slot = design_.slot(read.slot);
                if (sir::isContainer(slot.type) && !called.contains(slot.subroutine))
                    heapRead = true;
            }
        if (heapRead) report(at, "implicit sensitivity to dynamic container values");
        if (std::ranges::any_of(called,
                                [&](const auto sub) { return triggeredReaders_.contains(sub); }))
            report(at, "implicit sensitivity to named-event triggered values");
    }
}

void Lowerer::finishDrives() {
    // What a cont process drives is what its own instructions write, once
    // resolveNets has moved them to the driver slots; what the functions it
    // calls write are procedural writes (6.5, SIR_SPEC.md 3.4).
    for (std::uint32_t i = 0; i < design_.processes.size(); ++i) {
        const sir::ProcessId p{i};
        sir::Process& proc = design_.process(p);
        if (proc.kind != sir::ProcessKind::Cont) continue;
        proc.drives = proc.writes;
        const auto it = callWrites_.find(p);
        if (it == callWrites_.end()) continue;
        for (const sir::SlotRange& r : it->second)
            if (std::ranges::none_of(proc.writes, [&](const sir::SlotRange& w) {
                    return sir::covers(design_.slot(r.slot), w, r);
                }))
                proc.writes.push_back(r);
    }
}

void Lowerer::resolveNets() {
    // The drivers of each net: the cont processes that write it (a
    // continuous assignment, a net declaration assignment or a port).
    // Two drivers of a uwire never get here: slang's analysis rejects them
    // (6.6.2).
    std::map<sir::SlotId, std::vector<std::pair<sir::ProcessId, sir::SlotRange>>> drivers;
    for (std::uint32_t i = 0; i < design_.processes.size(); ++i) {
        const sir::ProcessId p{i};
        if (design_.process(p).kind != sir::ProcessKind::Cont) continue;
        for (const sir::SlotRange& w : design_.process(p).writes)
            if (design_.slot(w.slot).kind != sir::SlotKind::Var) drivers[w.slot].push_back({p, w});
    }
    for (const auto& [net, writes] : drivers) {
        // Only nets with bits that several drivers write, or with a driver
        // and their own pull or supply, are resolved (VALUE_SEMANTICS.md
        // 5.5). Where drivers write separate bits, each bit has the value
        // of its one driver either way.
        const sir::SlotKind kind = design_.slot(net).kind;
        const bool own = kind == sir::SlotKind::Tri0 || kind == sir::SlotKind::Tri1 ||
                         kind == sir::SlotKind::Supply0 || kind == sir::SlotKind::Supply1;
        bool shared = false;
        for (std::size_t i = 0; i < writes.size() && !shared; ++i)
            for (std::size_t j = i + 1; j < writes.size() && !shared; ++j)
                shared = writes[i].first != writes[j].first &&
                         sir::overlaps(design_.slot(net), writes[i].second, writes[j].second);
        const bool metadata = strengthNets_.contains(net);
        if (!shared && !own && !metadata) continue;
        if (!shared && !own && metadata) newlyResolvedStrengthNets_.insert(net);
        std::vector<sir::ProcessId> ps;
        for (const auto& w : writes)
            if (std::find(ps.begin(), ps.end(), w.first) == ps.end()) ps.push_back(w.first);
        // Drivers that meet through a port between nets resolve on one net
        // with their own strengths (VALUE_SEMANTICS.md 5.3); that needs
        // the nets merged, not done yet.
        const auto through = std::find_if(ps.begin(), ps.end(),
                                          [&](sir::ProcessId p) { return netPorts_.contains(p); });
        if (through != ps.end() && (!metadata || shared || own)) {
            ++numErrors_;
            diags_ += whereOf(design_.process(*through).loc) +
                      ": error: unsupported: " + "drivers of the net '" + design_.slot(net).name +
                      "' that meet through a port (not implemented yet)\n";
            continue;
        }
        const sir::SourceLoc where = design_.process(ps.front()).loc;
        if (!design_.slot(net).dims.empty()) {
            resolveArrayNet(net, ps, where);
            continue;
        }
        const auto unknownSide = std::find_if(ps.begin(), ps.end(), [&](sir::ProcessId p) {
            const auto it = sides_.find(p);
            return it != sides_.end() && !it->second.known;
        });
        if (unknownSide != ps.end()) {
            ++numErrors_;
            diags_ +=
                whereOf(design_.process(*unknownSide).loc) +
                ": error: unsupported: a three-state gate that drives this part of the net '" +
                design_.slot(net).name + "' with other drivers (not implemented yet)\n";
            continue;
        }
        const sir::Type type = design_.slot(net).type;
        const sir::InstanceId instance = design_.slot(net).instance;
        design_.slot(net).flags |= sir::kMultiDriver;

        // Each driver writes its own slot, which starts at z (SIR_SPEC.md
        // 3.5): the bits it does not write do not drive.
        std::vector<sir::SlotId> slots;
        std::vector<sir::SlotId> sideSlots;
        for (std::size_t k = 0; k < ps.size(); ++k) {
            sir::Slot d;
            d.name = design_.slot(net).name + ".$driver" + std::to_string(k);
            d.type = type;
            d.kind = sir::SlotKind::Wire;
            d.instance = instance;
            d.driverOf = net;
            const auto it = strengths_.find(ps[k]);
            if (it != strengths_.end()) std::tie(d.strength1, d.strength0) = it->second;
            const auto id = static_cast<sir::SlotId>(design_.slots.size());
            design_.slots.push_back(std::move(d));
            slots.push_back(id);
            sir::Process& p = design_.process(ps[k]);
            for (sir::BasicBlock& bb : p.blocks)
                for (sir::Instr& in : bb.instrs)
                    if (sir::writesSlot(in.op) && !in.operands.empty() &&
                        in.operands[0].kind == sir::Operand::Kind::Slot &&
                        in.operands[0].asSlot() != sir::kNone &&
                        sir::physicalSlot(design_, in.operands[0].asSlot()) == net)
                        in.operands[0].id = id.index();
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
            ss.name = design_.slot(net).name + ".$side" + std::to_string(k);
            ss.type = sir::u2(2 * type.width);
            ss.instance = instance;
            const auto sid = static_cast<sir::SlotId>(design_.slots.size());
            design_.slots.push_back(std::move(ss));
            sideSlots.push_back(sid);
            auto fields = side->second;
            std::optional<sir::Instr> delayed;
            // Inputs can end in a non-entry block after short-circuit calls.
            for (std::size_t blockIndex = 0; blockIndex < p.blocks.size() && !delayed;
                 ++blockIndex) {
                auto& candidates = p.blocks[blockIndex].instrs;
                for (std::size_t i = 0; i < candidates.size(); ++i) {
                    const auto& in = candidates[i];
                    if (in.op != sir::Op::DriveDelay || in.operands[0].asSlot() != id) continue;
                    delayed = std::move(candidates[i]);
                    candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(i));
                    if (blockIndex == fields.block.index() && i < fields.first) --fields.first;
                    break;
                }
            }
            auto& instrs = p.blocks[fields.block.index()].instrs;
            if (delayed) {
                std::vector<sir::Instr> complete;
                complete.reserve(instrs.size());
                for (std::size_t i = 0; i < instrs.size(); ++i) {
                    auto& in = instrs[i];
                    if (i >= fields.first && i < fields.first + fields.count &&
                        in.op == sir::Op::Store) {
                        in.operands[0].id = sid.index();
                        const auto& o = in.operands[0];
                        p.writes.push_back({sid, 0, 0, o.lsb, o.width});
                        delayed->operands.push_back(o);
                        delayed->operands.push_back(in.operands[1]);
                    } else {
                        complete.push_back(std::move(in));
                    }
                    // Every field is defined before the single registration.
                    if (i + 1 == fields.first + fields.count)
                        complete.push_back(std::move(*delayed));
                }
                instrs = std::move(complete);
            } else {
                for (std::size_t i = fields.first; i < fields.first + fields.count; ++i) {
                    if (instrs[i].op != sir::Op::Store) continue;
                    auto& o = instrs[i].operands[0];
                    o.id = sid.index();
                    p.writes.push_back({sid, 0, 0, o.lsb, o.width});
                }
            }
            sides_.erase(side);
            if (metadata && !shared && !own) addStrengthSideValidity(ps[k], id);
        }

        // The net's value is resolved again whenever a driver changes
        // (VALUE_SEMANTICS.md 5.6).
        sir::Process r;
        r.name = design_.slot(net).name + ".$resolve";
        r.kind = sir::ProcessKind::Cont;
        r.netResolver = true;
        r.instance = instance;
        r.loc = where;
        r.blocks.resize(1);
        std::vector<sir::Operand> ops{sir::Operand::str(std::string(sir::kindName(kind)))};
        for (std::size_t k = 0; k < slots.size(); ++k) {
            const sir::TempId t = sir::TempId{r.numTemps++};
            r.blocks[0].instrs.push_back(
                {sir::Op::Load, type, t, {sir::Operand::slot(slots[k])}, where});
            r.triggers.push_back({sir::SlotRange{slots[k], 0, 0, 0, 0}, sir::Edge::Any});
            ops.push_back(sir::Operand::temp(t));
            ops.push_back(constant(sir::u2(3), design_.slot(slots[k]).strength1).op);
            ops.push_back(constant(sir::u2(3), design_.slot(slots[k]).strength0).op);
            if (sideSlots[k] == sir::kNone) continue;
            const sir::TempId s = sir::TempId{r.numTemps++};
            r.blocks[0].instrs.push_back({sir::Op::Load,
                                          design_.slot(sideSlots[k]).type,
                                          s,
                                          {sir::Operand::slot(sideSlots[k])},
                                          where});
            r.triggers.push_back({sir::SlotRange{sideSlots[k], 0, 0, 0, 0}, sir::Edge::Any});
            ops.push_back(sir::Operand::temp(s));
        }
        const sir::TempId v = sir::TempId{r.numTemps++};
        r.blocks[0].instrs.push_back({sir::Op::Resolve, type, v, std::move(ops), where});
        r.blocks[0].instrs.push_back({sir::Op::Store,
                                      sir::voidType(),
                                      sir::kNone,
                                      {sir::Operand::slot(net), sir::Operand::temp(v)},
                                      where});
        r.blocks[0].instrs.push_back({sir::Op::Ret, sir::voidType(), sir::kNone, {}, where});
        r.writes.push_back({net, 0, 0, 0, 0});
        netResolvers_[net] = sir::ProcessId{static_cast<std::uint32_t>(design_.processes.size())};
        design_.processes.push_back(std::move(r));
    }
    // A three-state gate on a net that does not resolve writes L and H as
    // x alone (SIR_SPEC.md 3.5): its side computation goes.
    for (const auto& [proc, side] : sides_) {
        std::vector<sir::Instr>& instrs = design_.process(proc).blocks[side.block.index()].instrs;
        const auto at = instrs.begin() + static_cast<std::ptrdiff_t>(side.first);
        instrs.erase(at, at + static_cast<std::ptrdiff_t>(side.count));
    }
    sides_.clear();
}

void Lowerer::declarePackages(const ast::RootSymbol& root) {
    // The rows of the packages and compilation units with variables or
    // nets first, in source order: a package where it is declared, a
    // compilation unit at its first static storage, including class data.
    // One with subroutines
    // alone gets its row when one of them is lowered (rowFor); it has
    // nothing to initialize.
    const auto holds = [&](const ast::Symbol& m) {
        if (m.kind == ast::SymbolKind::GenericClassDef)
            return hasGenericClassStatics(m.as<ast::GenericClassDefSymbol>());
        return m.kind == ast::SymbolKind::Variable || m.kind == ast::SymbolKind::Net ||
               (m.kind == ast::SymbolKind::ClassType && hasClassStatics(m.as<ast::ClassType>()));
    };
    const auto holdsAny = [&](const ast::Scope& sc, const auto& self) -> bool {
        for (const ast::Symbol& m : sc.members())
            if (holds(m) || (m.kind == ast::SymbolKind::AnonymousProgram &&
                             self(m.as<ast::AnonymousProgramSymbol>(), self)))
                return true;
        return false;
    };
    std::vector<const ast::Symbol*> order;
    for (const ast::CompilationUnitSymbol* unit : root.compilationUnits) {
        for (const ast::Symbol& m : unit->members()) {
            if (m.kind == ast::SymbolKind::Package &&
                holdsAny(m.as<ast::PackageSymbol>(), holdsAny))
                order.push_back(&m);
            else if ((holds(m) || (m.kind == ast::SymbolKind::AnonymousProgram &&
                                   holdsAny(m.as<ast::AnonymousProgramSymbol>(), holdsAny))) &&
                     std::find(order.begin(), order.end(), unit) == order.end())
                order.push_back(unit);
        }
    }
    for (const ast::PackageSymbol* pkg : root.getCompilation().getPackages())
        if (std::find(order.begin(), order.end(), pkg) == order.end() && holdsAny(*pkg, holdsAny))
            order.push_back(pkg);
    for (const ast::Symbol* sym : order)
        rowFor(*sym);
    // Then their slots and static initializers, in the same order.
    for (const ast::Symbol* sym : order) {
        instance_ = scopeRows_.at(sym);
        declareScope(sym->as<ast::Scope>());
    }
    instance_ = sir::InstanceId{0};
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

bool Lowerer::reportDpiImport(const ast::Symbol& s) {
    if (s.kind != ast::SymbolKind::Subroutine ||
        !s.as<ast::SubroutineSymbol>().flags.has(ast::MethodFlags::DPIImport))
        return false;
    // One declaration is one symbol in every instance of its module.
    if (std::find(dpiReported_.begin(), dpiReported_.end(), s.location) == dpiReported_.end()) {
        dpiReported_.push_back(s.location);
        report(s.location, "DPI-C import '" + std::string(s.name) + "'");
    }
    return true;
}

void Lowerer::declareInstance(const ast::InstanceSymbol& inst, sir::InstanceId parent) {
    // Every instance comes here: at the top, in a module or a generate
    // block, and as an element of an instance array.
    const auto id = static_cast<sir::InstanceId>(design_.instances.size());
    design_.instances.push_back({inst.getHierarchicalPath(), std::string(inst.getDefinition().name),
                                 parent,
                                 inst.getDefinition().definitionKind == ast::DefinitionKind::Program
                                     ? sir::InstanceKind::Program
                                     : sir::InstanceKind::Module});
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
            const sir::SlotId root = sir::physicalSlot(design_, s);
            std::vector<sir::ArrayDim> dims;
            const ast::Type& leaf = arrayElement(inner.getType(), dims);
            const auto& actual = design_.slot(s);
            if (actual.type != typeOf(leaf, sl) || actual.dims.size() != dims.size()) continue;
            bool sameBounds = true;
            bool sameShape = true;
            for (std::size_t k = 0; k < dims.size(); ++k) {
                const auto& a = actual.dims[k];
                const auto& b = dims[k];
                sameBounds = sameBounds && a.left == b.left && a.right == b.right;
                const auto extent = [](sir::ArrayDim d) {
                    return static_cast<std::uint64_t>(std::max(d.left, d.right)) -
                           static_cast<std::uint64_t>(std::min(d.left, d.right));
                };
                sameShape = sameShape && extent(a) == extent(b);
            }
            if (!sameShape) continue;
            // slang gives the warnings of table 23-1 (-Wnet-inconsistent).
            design_.slot(root).kind = mergedKind(*innerKind, design_.slot(root).kind);
            sir::SlotId view = s;
            if (!sameBounds) {
                view = newSlot(inner, "", inner.getType(), {}, sl);
                design_.slot(view).viewOf = root;
                design_.slot(view).instance = instanceIds_.at(&child);
            } else {
                design_.names.push_back({inner.getHierarchicalPath(), s});
            }
            slots_.emplace(&inner, view);
            mergedPorts_.insert(conn);
            mergedSlots_.insert(root);
        } catch (const Unsupported&) {
            continue;  // already reported
        }
    }
}

void Lowerer::aliasNets(const ast::NetAliasSymbol& alias) {
    // alias makes whole nets of one type one net (10.11): they share the
    // first one's slot, like nets merged through a port (SIR_SPEC.md 3.5).
    // slang rejects nets of different kinds or widths and anything but
    // nets.
    const slang::SourceLocation sl = alias.location;
    const std::span<const ast::Expression* const> refs = alias.getNetReferences();
    std::optional<sir::SlotId> shared;
    std::vector<const ast::NetSymbol*> nets;
    for (const ast::Expression* e : refs) {
        const ast::Symbol* s = namedSymbol(*e);
        if (s == nullptr || s->kind != ast::SymbolKind::Net) {
            report(sl, "an alias of a part or a concatenation of nets");
            return;
        }
        const auto& n = s->as<ast::NetSymbol>();
        if (n.getType().getCanonicalType().isUnpackedArray()) {
            report(sl, "an alias of an array of nets");
            return;
        }
        if (const auto it = slots_.find(&n); it != slots_.end()) {
            if (shared && *shared != it->second) {
                report(sl, "an alias of nets that other aliases or ports joined apart");
                return;
            }
            shared = it->second;
        }
        nets.push_back(&n);
    }
    if (nets.empty()) return;
    try {
        const sir::SlotId s = shared ? *shared : slotOf(*nets.front(), sl);
        for (const ast::NetSymbol* n : nets) {
            if (slots_.contains(n)) continue;
            if (typeOf(n->getType(), sl) != design_.slot(s).type ||
                netKindOf(*n) != design_.slot(s).kind) {
                report(sl, "an alias of nets of different types or kinds");
                return;
            }
            slots_.emplace(n, s);
            design_.names.push_back({n->getHierarchicalPath(), s});
        }
    } catch (const Unsupported&) {
        return;  // already reported
    }
}

bool Lowerer::hasClassStatics(const ast::ClassType& type) {
    checkStack(type.location);
    if (type.isUninstantiated) return false;
    for (const ast::Symbol& member : type.members()) {
        if (member.getParentScope() != &type) continue;
        if (member.kind == ast::SymbolKind::ClassProperty &&
            member.as<ast::ClassPropertySymbol>().lifetime == ast::VariableLifetime::Static)
            return true;
        if (member.kind == ast::SymbolKind::ClassType &&
            hasClassStatics(member.as<ast::ClassType>()))
            return true;
        if (member.kind == ast::SymbolKind::GenericClassDef &&
            hasGenericClassStatics(member.as<ast::GenericClassDefSymbol>()))
            return true;
    }
    return false;
}

bool Lowerer::hasGenericClassStatics(const ast::GenericClassDefSymbol& type) {
    checkStack(type.location);
    if (std::find(genericClassStatics_.begin(), genericClassStatics_.end(), &type) ==
        genericClassStatics_.end())
        genericClassStatics_.push_back(&type);
    // Snapshot before querying members, which may elaborate more types. Only
    // the Boolean result is used; hash iteration never assigns SIR identities.
    std::vector<const ast::ClassType*> concrete;
    for (const auto& specialization : type.specializations()) {
        const auto& canonical = specialization.getCanonicalType();
        if (canonical.kind == ast::SymbolKind::ClassType)
            concrete.push_back(&canonical.as<ast::ClassType>());
    }
    for (const auto* specialization : concrete)
        if (hasClassStatics(*specialization)) return true;
    return false;
}

void Lowerer::checkGenericClassStatics() {
    std::set<slang::SourceLocation> unsupported;
    std::vector<std::size_t> previous;
    for (;;) {
        for (std::size_t i = 0; i < genericClassStatics_.size(); ++i) {
            const auto* type = genericClassStatics_[i];
            if (hasGenericClassStatics(*type)) unsupported.insert(type->location);
        }
        std::vector<std::size_t> current;
        current.reserve(genericClassStatics_.size());
        for (const auto* type : genericClassStatics_)
            current.push_back(type->numSpecializations());
        if (current == previous) break;
        previous = std::move(current);
    }
    // One positioned diagnostic per declaration, in source-location order.
    for (const auto at : unsupported)
        report(at, "static storage of a parameterized class");
}

void Lowerer::declareClassStatics(const ast::ClassType& type) {
    checkStack(type.location);
    if (type.genericClass != nullptr || type.isUninstantiated) return;
    if (!declaredClassStatics_.insert(&type).second) return;
    // Only the declaring class initializes a property. Inherited names keep
    // that same symbol/storage, and instance fields wait for construction.
    declareScope(type);
}

void Lowerer::declareScope(const ast::Scope& scope) {
    // Aliased nets first, so that they share one slot from the start.
    for (const ast::Symbol& member : scope.members())
        if (member.kind == ast::SymbolKind::NetAlias) aliasNets(member.as<ast::NetAliasSymbol>());
    for (const ast::Symbol& member : scope.members()) {
        switch (member.kind) {
            case ast::SymbolKind::ClockingBlock:
                try {
                    declareClocking(member.as<ast::ClockingBlockSymbol>());
                } catch (const Unsupported&) {
                    continue;  // The unsupported declaration was already diagnosed.
                }
                break;
            case ast::SymbolKind::ClassProperty:
                if (member.getParentScope() != &scope ||
                    member.as<ast::ClassPropertySymbol>().lifetime != ast::VariableLifetime::Static)
                    break;
                [[fallthrough]];
            case ast::SymbolKind::Variable:
            case ast::SymbolKind::Net: {
                const auto& v = member.as<ast::ValueSymbol>();
                try {
                    if (structElement(v.getType()) != nullptr) {
                        // One slot per member; the initial value or the
                        // members' defaults (7.2.2) in static-init.
                        const sir::SlotId first = structSlots(v, v.location);
                        if (member.kind != ast::SymbolKind::Net &&
                            (v.getInitializer() != nullptr || hasMemberDefaults(v.getType())))
                            lowerStaticInit(member.as<ast::VariableSymbol>(), first, v.location);
                        break;
                    }
                    const sir::SlotId s = slotOf(v, v.location);
                    // A net's initializer is a continuous assignment
                    // (10.3.1), lowered with the processes.
                    if (member.kind != ast::SymbolKind::Net && v.getInitializer() != nullptr)
                        lowerStaticInit(member.as<ast::VariableSymbol>(), s, v.location);
                } catch (const Unsupported&) {
                    continue;  // already reported
                }
                break;
            }
            case ast::SymbolKind::Instance:
                declareInstance(member.as<ast::InstanceSymbol>(), instance_);
                break;
            case ast::SymbolKind::ClassType:
                declareClassStatics(member.as<ast::ClassType>());
                break;
            case ast::SymbolKind::GenericClassDef:
                hasGenericClassStatics(member.as<ast::GenericClassDefSymbol>());
                break;
            case ast::SymbolKind::AnonymousProgram:
                declareScope(member.as<ast::AnonymousProgramSymbol>());
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
    // A copy: the first use of a package adds its row to the table while
    // the scope is lowered (rowFor).
    const std::string scope = design_.instance(instance_).scopeName;
    lowerScope(inst.body, scope);
    instance_ = saved;
}

void Lowerer::lowerScope(const ast::Scope& scope, const std::string& prefix) {
    for (const ast::Symbol& member : scope.members()) {
        switch (member.kind) {
            case ast::SymbolKind::ClockingBlock:
                lowerClocking(member.as<ast::ClockingBlockSymbol>());
                break;
            case ast::SymbolKind::Variable:         // declareScope made the slot
            case ast::SymbolKind::GenericClassDef:  // specialization storage checked after binding
            case ast::SymbolKind::ClassType:        // static fields declared; objects require
                                                    // construction
            case ast::SymbolKind::Parameter:        // constants, used where they are named
            case ast::SymbolKind::TypeParameter:
            case ast::SymbolKind::TypeAlias:
            case ast::SymbolKind::ForwardingTypedef:
            case ast::SymbolKind::TransparentMember:  // an enum value made visible
            case ast::SymbolKind::Genvar:             // used up by elaboration
            case ast::SymbolKind::ExplicitImport:     // names resolved by slang
            case ast::SymbolKind::WildcardImport:
            case ast::SymbolKind::EmptyMember:
            case ast::SymbolKind::DefParam:          // slang applied the value (23.10)
            case ast::SymbolKind::Specparam:         // a constant, used where it is named (6.20.5)
            case ast::SymbolKind::NetAlias:          // one slot (aliasNets)
            case ast::SymbolKind::Port:              // connected by the parent (lowerPorts)
            case ast::SymbolKind::InterfacePort:     // names through it reach the interface
            case ast::SymbolKind::Modport:           // a view of the interface's members (25.5)
            case ast::SymbolKind::AnonymousProgram:  // declarations retain their enclosing scope
            case ast::SymbolKind::StatementBlock: continue;  // scopes inside procedures
            case ast::SymbolKind::Subroutine:                // lowered when first called
                reportDpiImport(member);
                continue;
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
                        if (!design_.slot(s).dims.empty())
                            unsupported(n.location, "assignment to a whole unpacked array");
                        // The type first: lowering the value may add slots,
                        // which moves the table.
                        const sir::Type t = design_.slot(s).type;
                        const Val v = coerce(lowerExpr(*init), t, n.location);
                        storePlace(placeOfSlot(s), v, false, n.location);
                    },
                    // The delay of a net declaration assignment is that of
                    // the assignment alone (10.3.3).
                    driveLevels(n.getDriveStrength()), n.getDelay());
                continue;
            }
            case ast::SymbolKind::ContinuousAssign: {
                const auto& ca = member.as<ast::ContinuousAssignSymbol>();
                lowerCont(
                    processName(prefix, "assign"), ca.location, ca,
                    [&] {
                        const ast::Expression& e = ca.getAssignment();
                        if (e.kind != ast::ExpressionKind::Assignment)
                            unsupported(ca.location, "this continuous assignment");
                        lowerAssignment(e.as<ast::AssignmentExpression>());
                    },
                    driveLevels(ca.getDriveStrength()), ca.getDelay());
                continue;
            }
            case ast::SymbolKind::ProceduralBlock:
                lowerProcedure(member.as<ast::ProceduralBlockSymbol>(), prefix);
                continue;
            case ast::SymbolKind::SpecifyBlock: {
                // A specify block with specparams alone does nothing; path
                // delays and timing checks are not done yet (SIR_SPEC.md 6).
                bool onlyParams = true;
                for (const ast::Symbol& m : member.as<ast::SpecifyBlockSymbol>().members())
                    onlyParams = onlyParams && m.kind == ast::SymbolKind::Specparam;
                if (!onlyParams)
                    report(member.location, "specify block with path delays or timing checks");
                continue;
            }
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
    // an output the other way (SIR_SPEC.md 3.5; folding is P3).
    const std::string scope = design_.instance(instanceIds_.at(&child)).scopeName;
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
            // slang supplies defaults for omitted connections. A null
            // expression also represents an explicit .port(), which must
            // stay unconnected even when the port has a default (23.3.2.2).
            pullUnconnected(child, ps);
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
                    if (!design_.slot(s).dims.empty()) {
                        Storage dst;
                        dst.slot = s;
                        Stores stores;
                        assignAggregate(dst, internal->as<ast::ValueSymbol>().getType(), *ext,
                                        stores, sl);
                        flushStores(stores, false, sl);
                        return;
                    }
                    const sir::Type t = design_.slot(s).type;  // before lowering adds slots
                    const Val v = coerce(lowerExpr(*ext), t, sl);
                    emitVoid(sir::Op::Store, {sir::Operand::slot(s), v.op}, sl);
                    noteWrite({s, 0, 0, 0, 0});
                });
                break;
            case ast::ArgumentDirection::Out:
                lowerCont(name, sl, child, [&] {
                    if (!design_.slot(s).dims.empty()) {
                        const auto dst = storageOf(*outputPlace(ext), sl);
                        if (!dst) unsupported(sl, "this unpacked array output connection");
                        Storage src;
                        src.slot = s;
                        Stores stores;
                        copyArrayStorage(*dst, src, stores, sl);
                        flushStores(stores, false, sl);
                        return;
                    }
                    const ast::Expression& place = *outputPlace(ext);
                    const Val v =
                        emit(sir::Op::Load, design_.slot(s).type, {sir::Operand::slot(s)}, sl);
                    noteRead({s, 0, 0, 0, 0});
                    lowerStore(place, coerce(v, typeOf(*place.type, sl), sl), false, sl);
                });
                break;
            case ast::ArgumentDirection::InOut: report(ps.location, "inout port"); break;
            case ast::ArgumentDirection::Ref: report(ps.location, "ref port"); break;
        }
        // A program port is a program-scope object (24.3.2); its implicit
        // assignment executes in that instance's reactive set as well.
        if (design_.processes.size() > before &&
            design_.instance(instanceIds_.at(&child)).kind == sir::InstanceKind::Program)
            design_.processes[before].instance = instanceIds_.at(&child);
        if (betweenNets && design_.processes.size() > before) {
            netPorts_.insert(static_cast<sir::ProcessId>(before));
            if (internal->as<ast::NetSymbol>().getDelay() == nullptr)
                netPortCandidates_.push_back({sir::ProcessId{static_cast<std::uint32_t>(before)}, s,
                                              outputPlace(ext), loc(sl)});
        }
    }
}

void Lowerer::pullUnconnected(const ast::InstanceSymbol& child, const ast::PortSymbol& ps) {
    // slang records the directive in effect where the definition was
    // declared. Only input ports are pulled, by their declared direction.
    const ast::UnconnectedDrive drive = child.getDefinition().unconnectedDrive;
    if (drive == ast::UnconnectedDrive::None || ps.direction != ast::ArgumentDirection::In) return;
    const ast::Symbol* internal = ps.internalSymbol;
    if (internal == nullptr || !inSlot(*internal)) {
        report(ps.location, "port expression left unconnected under `unconnected_drive");
        return;
    }
    if (internal->kind != ast::SymbolKind::Net) {
        // A pull is a driver with a strength, which a variable does not
        // have; 23.3.3.2 gives it the initial value of its type.
        warning(ps.location, "`unconnected_drive does not pull input port '" +
                                 std::string(ps.name) +
                                 "', a variable; it keeps the initial value of its type");
        return;
    }
    const auto it = slots_.find(internal);
    if (it == slots_.end()) return;  // its declaration was reported
    const sir::SlotId s = it->second;
    const sir::Type t = design_.slot(s).type;
    if (!sir::isIntegral(t)) {
        report(ps.location, "unpacked array port left unconnected under `unconnected_drive");
        return;
    }
    // A pullup or pulldown on every bit: 1 or 0 at pull strength (28.10).
    const std::uint64_t word = drive == ast::UnconnectedDrive::Pull1 ? ~std::uint64_t{0} : 0;
    const std::size_t words = sir::integralWords(t.width);
    const std::string scope = design_.instance(instanceIds_.at(&child)).scopeName;
    lowerCont(scope + ".$pull." + std::string(ps.name), ps.location, child,
              [&] {
                  const Val v = constantWords(t, std::vector<std::uint64_t>(words, word),
                                              std::vector<std::uint64_t>(words, 0));
                  const bool array = !design_.slot(s).dims.empty();
                  const auto count = sir::elementCount(design_.slot(s));
                  for (std::uint32_t e = 0; e < count; ++e) {
                      auto target = sir::Operand::slot(s);
                      if (array) {
                          target.firstElem = e;
                          target.numElems = 1;
                      }
                      emitVoid(sir::Op::Store, {target, v.op}, ps.location);
                  }
                  noteWrite({s, 0, 0, 0, 0});
              },
              {5, 5});
}

std::string Lowerer::primitiveName(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix,
                                   std::string_view kind) {
    if (g.name.empty() && g.arrayPath.empty()) return processName(prefix, std::string(kind));
    // slang names an element of an array of gates or UDPs by the array
    // alone; it gets its indices, as an element of an array of module
    // instances does (28.3.6), so that the names of its slots are its own.
    std::string name = g.getHierarchicalPath();
    slang::SmallVector<slang::ConstantRange> dims;
    g.getArrayDimensions(dims);
    for (std::size_t i = 0; i < g.arrayPath.size() && i < dims.size(); ++i)
        name +=
            "[" + std::to_string(static_cast<std::int32_t>(g.arrayPath[i]) + dims[i].lower()) + "]";
    return name;
}

void Lowerer::lowerPrimitive(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix) {
    // A built-in gate or pull is a cont process that computes its outputs
    // from its inputs whenever one changes (VALUE_SEMANTICS.md 5.8,
    // SIR_SPEC.md 3.5, 7.1).
    const ast::PrimitiveSymbol& prim = g.primitiveType;
    const std::string kind(prim.name);
    if (prim.primitiveKind == ast::PrimitiveSymbol::UserDefined) {
        lowerUdp(g, prefix);
        return;
    }
    static constexpr std::string_view kLogic[] = {"and", "nand", "or", "nor", "xor", "xnor"};
    static constexpr std::string_view kTri[] = {"bufif0", "bufif1", "notif0", "notif1"};
    const bool pull = kind == "pullup" || kind == "pulldown";
    const bool logic = std::find(std::begin(kLogic), std::end(kLogic), kind) != std::end(kLogic);
    const bool tri = std::find(std::begin(kTri), std::end(kTri), kind) != std::end(kTri);
    if (kind == "nmos" || kind == "pmos" || kind == "rnmos" || kind == "rpmos" || kind == "cmos" ||
        kind == "rcmos") {
        lowerMos(g, prefix);
        return;
    }
    if (kind == "tran" || kind == "rtran" || kind == "tranif0" || kind == "tranif1" ||
        kind == "rtranif0" || kind == "rtranif1") {
        lowerPassive(g, prefix);
        return;
    }
    if (!logic && !tri && !pull && kind != "buf" && kind != "not") {
        // Other device families need their own execution contracts.
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
    const std::string name = primitiveName(g, prefix, "gate");
    const slang::SourceLocation sl = g.location;
    const auto first = static_cast<sir::ProcessId>(design_.processes.size());
    std::optional<GateSide> side;
    lowerCont(
        name, sl, g,
        [&] {
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
            const bool net = p.kind == Place::Kind::Slot || p.kind == Place::Kind::Bits ||
                             (p.kind == Place::Kind::Element && p.home == Place::Home::Slot);
            if (!net || design_.slot(p.slot).kind == sir::SlotKind::Var) return;
            const std::uint32_t w = design_.slot(p.slot).type.width;
            const bool fixed = p.kind == Place::Kind::Slot
                                   ? w == 1
                                   : !p.pos && p.indices.empty() && p.width == 1 && p.lsb >= 0 &&
                                         p.lsb < static_cast<std::int64_t>(w);
            if (!fixed || !design_.slot(p.slot).dims.empty()) {
                side = GateSide{0, 0, false};
                return;
            }
            const auto at = static_cast<std::uint32_t>(p.kind == Place::Kind::Slot ? 0 : p.lsb);
            const std::size_t start = block(b_.block).instrs.size();
            const sir::Type flag = sir::u2(1);
            const Val unknown = inv(op2(sir::Op::Or, on, off));
            const auto is = [&](std::uint64_t value) {
                return emit(sir::Op::Ceq, flag, {data.op, constant(bit, value).op}, sl);
            };
            const Val one = inv(op2(sir::Op::And, unknown, is(0)));
            const Val zero = inv(op2(sir::Op::And, unknown, is(1)));
            emitVoid(sir::Op::Store, {sir::Operand::slot(sir::kNone, at, 1), one.op}, sl);
            emitVoid(sir::Op::Store, {sir::Operand::slot(sir::kNone, w + at, 1), zero.op}, sl);
            side = GateSide{start, block(b_.block).instrs.size() - start, true, b_.block};
        },
        strength, g.getDelay());
    if (side && design_.processes.size() > first.index()) sides_[first] = *side;
}

void Lowerer::lowerPassive(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix) {
    const std::string kind(g.primitiveType.name);
    static constexpr std::pair<std::string_view, sir::StrengthEdgeKind> kinds[]{
        {"tran", sir::StrengthEdgeKind::Tran},
        {"rtran", sir::StrengthEdgeKind::Rtran},
        {"tranif0", sir::StrengthEdgeKind::Tranif0},
        {"tranif1", sir::StrengthEdgeKind::Tranif1},
        {"rtranif0", sir::StrengthEdgeKind::Rtranif0},
        {"rtranif1", sir::StrengthEdgeKind::Rtranif1}};
    const auto found =
        std::ranges::find_if(kinds, [&](const auto& item) { return item.first == kind; });
    if (found == std::end(kinds)) throw std::logic_error("missing passive device kind");
    const auto edgeKind = found->second;
    const bool conditional = sir::conditionalStrengthEdge(edgeKind);
    const auto conns = g.getPortConnections();
    if (conns.size() != (conditional ? 3U : 2U)) return;  // slang checks terminal counts.
    const auto sl = g.location;
    const auto first = sir::ProcessId{static_cast<std::uint32_t>(design_.processes.size())};
    PassiveConnection connection;
    connection.kind = edgeKind;
    connection.loc = loc(sl);
    connection.owner = conditional ? first : sir::ProcessId{sir::kNone};
    connection.delayed = g.getDelay() != nullptr;
    lowerCont(primitiveName(g, prefix, "switch"), sl, g, [&] {
        const auto terminal = [&](const ast::Expression& expr) {
            Place at = placeOf(*outputPlace(&expr), sl);
            while (at.kind == Place::Kind::Concat && at.base.size() == 1) {
                Place next = at.base[0];
                at = std::move(next);
            }
            if (at.type.width != 1) {
                slang::SmallVector<slang::ConstantRange> dims;
                g.getArrayDimensions(dims);
                if (dims.empty() || dims.size() != g.arrayPath.size())
                    unsupported(sl, "a vector passive terminal without a matching primitive array");
                std::uint64_t count = 1, position = 0;
                for (std::size_t i = 0; i < dims.size(); ++i) {
                    const auto width = static_cast<std::uint64_t>(dims[i].width());
                    if (width == 0 || count > UINT32_MAX / width || g.arrayPath[i] >= width)
                        unsupported(sl, "a passive primitive array outside the SIR range");
                    count *= width;
                    const auto index = static_cast<std::int64_t>(dims[i].lower()) + g.arrayPath[i];
                    const auto right = static_cast<std::int64_t>(dims[i].right);
                    const auto bit =
                        static_cast<std::uint64_t>(index > right ? index - right : right - index);
                    position = position * width + bit;
                }
                if (count != at.type.width)
                    unsupported(sl,
                                "a passive terminal width that does not match its primitive array");
                // Bit positions are relative to the right edge of the actual,
                // including concatenations, selections and ascending instance bounds.
                while (at.kind == Place::Kind::Concat) {
                    std::optional<Place> selected;
                    for (auto it = at.base.rbegin(); it != at.base.rend(); ++it) {
                        if (position < it->type.width) {
                            selected = *it;
                            break;
                        }
                        position -= it->type.width;
                    }
                    if (!selected) unsupported(sl, "an invalid passive concatenation mapping");
                    at = std::move(*selected);
                }
                at = narrow(std::move(at), static_cast<std::int64_t>(position), 1, sl);
            }
            const bool fixed = at.home == Place::Home::Slot && at.type.width == 1 &&
                               (at.kind == Place::Kind::Slot || at.kind == Place::Kind::Element ||
                                at.kind == Place::Kind::Bits) &&
                               !at.pos && (at.indices.empty() || at.elem.has_value());
            if (!fixed || design_.slot(at.slot).kind == sir::SlotKind::Var ||
                !sir::isFourState(at.type))
                unsupported(sl, "a non-static or non-four-state passive terminal");
            auto range = sir::physicalRange(design_, rangeOf(at));
            if (range.lsb >= design_.slot(range.slot).type.width ||
                (at.kind == Place::Kind::Bits &&
                 (at.lsb < 0 ||
                  at.lsb + at.width >
                      (at.windowWidth == 0 ? design_.slot(at.slot).type.width : at.windowWidth))))
                unsupported(sl, "an out-of-range passive terminal");
            range.firstElem = at.elem.value_or(0);
            range.numElems = range.width = 1;
            return range;
        };
        connection.first = terminal(*conns[0]);
        connection.second = terminal(*conns[1]);
        if (!conditional) {
            if (connection.delayed) unsupported(sl, "a delay on an unconditional passive switch");
            return;
        }
        std::vector<sir::Operand> args{sir::Operand::strengthEdge(sir::kNone),
                                       coerce(lowerExpr(*conns[2]), sir::u4(1), sl).op};
        if (const auto* delay = g.getDelay()) {
            const auto values = delaysOf(*delay, sl);
            args.push_back(values[0].op);
            args.push_back(values[1].op);
        }
        emitVoid(connection.delayed ? sir::Op::StrengthEdgeDelay : sir::Op::StrengthEdge,
                 std::move(args), sl);
    });
    if (design_.processes.size() <= first.index()) return;
    // Static endpoint normalization leaves no executing process for an always-on edge.
    if (!conditional) dropProcesses(first);
    strengthNets_.try_emplace(connection.first.slot, connection.loc);
    strengthNets_.try_emplace(connection.second.slot, connection.loc);
    passiveConnections_.push_back(connection);
}

void Lowerer::lowerMos(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix) {
    const auto conns = g.getPortConnections();
    const auto sl = g.location;
    const std::string kind(g.primitiveType.name);
    const bool parallel = kind == "cmos" || kind == "rcmos";
    if (conns.size() != (parallel ? 4U : 3U)) return;  // slang diagnoses terminal counts.
    const auto name = primitiveName(g, prefix, "mos");
    const auto first = sir::ProcessId{static_cast<std::uint32_t>(design_.processes.size())};
    Place output, data;
    bool structural = false;
    std::vector<sir::SlotId> controlSlots, indexSlots;
    std::optional<sir::SlotId> dataSlot, positionSlot;
    std::optional<std::array<Val, 3>> delays;
    std::vector<sir::SlotId> delaySlots;
    const auto hidden = [&](sir::Type type, const std::string& suffix) {
        const auto id = sir::SlotId{static_cast<std::uint32_t>(design_.slots.size())};
        design_.slots.push_back({name + ".$" + suffix, type});
        design_.slot(id).instance = instance_;
        return id;
    };
    lowerCont(name + ".$capture", sl, g, [&] {
        output = placeOf(*outputPlace(conns[0]), sl);
        while (output.kind == Place::Kind::Concat && output.base.size() == 1) {
            Place next = output.base[0];
            output = std::move(next);
        }
        const bool fixed =
            output.home == Place::Home::Slot && output.type.width == 1 &&
            (output.kind == Place::Kind::Slot || output.kind == Place::Kind::Element ||
             output.kind == Place::Kind::Bits) &&
            !output.pos && (output.indices.empty() || output.elem.has_value());
        if (!fixed || design_.slot(output.slot).kind == sir::SlotKind::Var ||
            !sir::isFourState(output.type))
            unsupported(sl, "a non-static or non-four-state MOS output");
        const auto outRange = rangeOf(output);
        if (outRange.lsb >= design_.slot(output.slot).type.width ||
            (output.kind == Place::Kind::Bits &&
             (output.lsb < 0 || output.lsb + output.width >
                                    (output.windowWidth == 0 ? design_.slot(output.slot).type.width
                                                             : output.windowWidth))))
            unsupported(sl, "an out-of-range MOS output");
        const ast::Expression* input = conns[1];
        while (input->kind == ast::ExpressionKind::Conversion &&
               input->as<ast::ConversionExpression>().isImplicit())
            input = &input->as<ast::ConversionExpression>().operand();
        const ast::Expression* root = input;
        for (;;) {
            if (root->kind == ast::ExpressionKind::ElementSelect)
                root = &root->as<ast::ElementSelectExpression>().value();
            else if (root->kind == ast::ExpressionKind::RangeSelect)
                root = &root->as<ast::RangeSelectExpression>().value();
            else if (root->kind == ast::ExpressionKind::MemberAccess)
                root = &root->as<ast::MemberAccessExpression>().value();
            else
                break;
        }
        const auto* symbol = namedSymbol(*root);
        structural = symbol != nullptr && symbol->kind == ast::SymbolKind::Net;
        const auto capture = [&](const Val& value, const std::string& suffix) {
            const auto id = hidden(value.type, suffix);
            storePlace(placeOfSlot(id), value, false, sl);
            return id;
        };
        if (structural) {
            data = placeOf(*input, sl);
            if (data.home != Place::Home::Slot || !sir::isFourState(data.type))
                unsupported(sl, "MOS strength from this net domain");
            for (std::size_t i = 0; i < data.indices.size(); ++i)
                indexSlots.push_back(capture({data.indices[i], operandType(data.indices[i])},
                                             "index" + std::to_string(i)));
            const auto position =
                data.kind == Place::Kind::Bits
                    ? (data.pos ? *data.pos
                                : constant(sir::s2(64), static_cast<std::uint64_t>(data.lsb)))
                    : constant(sir::s2(64), 0);
            positionSlot = capture(position, "position");
        } else
            dataSlot = capture(coerce(lowerExpr(*conns[1]), sir::u4(1), sl), "data");
        for (std::size_t i = 2; i < conns.size(); ++i)
            controlSlots.push_back(capture(coerce(lowerExpr(*conns[i]), sir::u4(1), sl),
                                           "control" + std::to_string(i - 2)));
        if (g.getDelay() != nullptr) {
            delays = delaysOf(*g.getDelay(), sl);
            for (unsigned i = 0; i < 3; ++i)
                delaySlots.push_back(capture((*delays)[i], "delay" + std::to_string(i)));
        }
    });
    if (design_.processes.size() <= first.index()) return;
    sir::Process device;
    device.name = name;
    device.kind = sir::ProcessKind::Cont;
    device.instance = instance_;
    device.loc = loc(sl);
    device.blocks.resize(1);
    const auto emit = [&](sir::Op op, sir::Type type, std::vector<sir::Operand> ops) {
        const auto temp = sir::TempId{device.numTemps++};
        device.blocks[0].instrs.push_back({op, type, temp, std::move(ops), device.loc});
        return sir::Operand::temp(temp);
    };
    const auto load = [&](sir::SlotId slot) {
        device.triggers.push_back({{slot}, sir::Edge::Any});
        return emit(sir::Op::Load, design_.slot(slot).type, {sir::Operand::slot(slot)});
    };
    sir::Operand signal;
    if (structural) {
        auto target = sir::Operand::slot(data.slot, data.windowLsb, data.windowWidth);
        std::vector<sir::Operand> ops{target};
        for (const auto slot : indexSlots)
            ops.push_back(load(slot));
        ops.push_back(load(*positionSlot));
        signal = emit(sir::Op::StrengthLoad, sir::strengthType(1), std::move(ops));
        const auto physical = sir::physicalSlot(design_, data.slot);
        strengthNets_.try_emplace(physical, device.loc);
        device.strengthTriggers.push_back(sir::physicalRange(design_, rangeOf(data)));
    } else {
        if (!dataSlot) throw std::logic_error("missing MOS expression carrier");
        signal = emit(sir::Op::StrengthMake, sir::strengthType(1),
                      {load(*dataSlot), constant(sir::u2(3), 6).op, constant(sir::u2(3), 6).op});
    }
    std::vector<sir::Operand> args{sir::Operand::str(kind), signal};
    for (const auto slot : controlSlots)
        args.push_back(load(slot));
    const auto result = emit(sir::Op::StrengthMos, sir::strengthType(1), std::move(args));
    std::vector<sir::Operand> propagation{sir::Operand::strengthOrigin(sir::kNone), result};
    for (const auto slot : delaySlots)
        propagation.push_back(load(slot));
    device.blocks[0].instrs.push_back(
        {delaySlots.empty() ? sir::Op::StrengthDrive : sir::Op::StrengthDriveDelay, sir::voidType(),
         sir::kNone, std::move(propagation), device.loc});
    device.blocks[0].instrs.push_back({sir::Op::Ret, sir::voidType(), sir::kNone, {}, device.loc});
    auto target = sir::physicalRange(design_, rangeOf(output));
    target.firstElem = output.elem.value_or(0);
    target.numElems = 1;
    target.width = 1;
    const auto id = sir::ProcessId{static_cast<std::uint32_t>(design_.processes.size())};
    strengthNets_.try_emplace(target.slot, device.loc);
    mosOutputs_[id] = {target, device.loc};
    design_.processes.push_back(std::move(device));
}

sir::TypeId Lowerer::enumRow(const ast::EnumType& et, slang::SourceLocation sl) {
    // The items in declaration order, each with its value of the base type
    // (SIR_SPEC.md 4.3).
    if (const auto it = enumRows_.find(&et); it != enumRows_.end()) return it->second;
    sir::TypeEntry t;
    t.kind = sir::TypeEntryKind::Enum;
    t.name = et.toString();
    t.value = typeOf(et, sl);
    for (const ast::EnumValueSymbol& item : et.values())
        t.items.push_back(
            {std::string(item.name), constantFrom(item.getValue(), t.value, sl).op.asConst()});
    const auto id = static_cast<sir::TypeId>(design_.types.size());
    design_.types.push_back(std::move(t));
    enumRows_.emplace(&et, id);
    return id;
}

sir::TypeId Lowerer::udpTable(const ast::PrimitiveSymbol& prim, slang::SourceLocation sl) {
    // The type-table row of a UDP's table (SIR_SPEC.md 4.3), in slang's
    // form of the rows.
    if (const auto it = udpTables_.find(&prim); it != udpTables_.end()) return it->second;
    sir::TypeEntry t;
    t.kind = sir::TypeEntryKind::Udp;
    t.name = std::string(prim.name);
    if (prim.ports.size() <= 1 || prim.ports.size() - 1 > std::numeric_limits<std::uint32_t>::max())
        unsupported(sl, "a UDP input count outside the SIR range");
    t.inputs = static_cast<std::uint32_t>(prim.ports.size() - 1);
    t.sequential = prim.isSequential;
    for (const ast::PrimitiveSymbol::TableEntry& e : prim.table)
        t.rows.push_back({std::string(e.inputs), e.state, e.output});
    if (prim.initVal != nullptr && prim.initVal->isInteger()) {
        // The initial statement (29.7): 0, 1 or x.
        const slang::SVInt& v = prim.initVal->integer();
        t.initial = v.hasUnknown() ? 'x' : (v.isOdd() ? '1' : '0');
    }
    const auto id = static_cast<sir::TypeId>(design_.types.size());
    design_.types.push_back(std::move(t));
    udpTables_.emplace(&prim, id);
    return id;
}

void Lowerer::lowerUdp(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix) {
    // A UDP instance is a cont process (SIR_SPEC.md 3.5, 5.5). A sequential
    // one keeps its state and the inputs it last saw in slots of its own,
    // <instance>.$state and <instance>.$prev; the state starts at the
    // initial value before time 0 (VALUE_SEMANTICS.md 5.9). With a delay,
    // <instance>.$started says whether the output has the initial value
    // yet: only the changes after it take the delay (29.7).
    const ast::PrimitiveSymbol& prim = g.primitiveType;
    // slang leaves an empty connection out of the list and only warns
    // (-Wudp-port-empty); a wrong count it reports as an error, and then
    // nothing is lowered. So a short list is an unconnected port.
    const std::span<const ast::Expression* const> conns = g.getPortConnections();
    if (conns.size() != prim.ports.size()) {
        report(g.location, "a UDP instance with an unconnected port");
        return;
    }
    const slang::SourceLocation sl = g.location;
    const std::string name = primitiveName(g, prefix, "udp");
    sir::TypeId table{0};
    try {
        table = udpTable(prim, sl);
    } catch (const Unsupported&) {
        return;
    }
    const std::uint32_t n = design_.typeEntry(table).inputs;
    sir::SlotId state = sir::kNone;
    sir::SlotId prev = sir::kNone;
    sir::SlotId started = sir::kNone;
    if (prim.isSequential) {
        const auto slot = [&](const std::string& suffix, sir::Type t) {
            sir::Slot s;
            s.name = name + suffix;
            s.type = t;
            s.instance = instance_;
            const auto id = static_cast<sir::SlotId>(design_.slots.size());
            design_.names.push_back({s.name, id});
            design_.slots.push_back(std::move(s));
            return id;
        };
        state = slot(".$state", sir::u4(1));
        prev = slot(".$prev", sir::u4(n));
        const char init = design_.typeEntry(table).initial;
        if (init != 'x') {
            ensureStaticInit(g, sl);
            Body saved = std::move(b_);
            b_ = std::move(staticBody_[instance_]);
            emitVoid(sir::Op::Store,
                     {sir::Operand::slot(state), constant(sir::u4(1), init == '1' ? 1 : 0).op}, sl);
            noteWrite({state, 0, 0, 0, 0});
            staticBody_[instance_] = std::move(b_);
            b_ = std::move(saved);
            if (g.getDelay() != nullptr) started = slot(".$started", sir::u2(1));
        }
    }
    lowerCont(
        name, sl, g,
        [&] {
            const sir::Type bit = sir::u4(1);
            if (started != sir::kNone) {
                // The first time it runs, the instance puts the initial
                // value on its output without the delay (29.7).
                const Val done = emit(sir::Op::Load, sir::u2(1), {sir::Operand::slot(started)}, sl);
                const sir::BlockId first = newBlock();
                const sir::BlockId rest = newBlock();
                terminate(sir::Op::BrIf,
                          {done.op, sir::Operand::block(rest), sir::Operand::block(first)}, sl);
                setBlock(first);
                const Val init = emit(sir::Op::Load, bit, {sir::Operand::slot(state)}, sl);
                const Place p = placeOf(*outputPlace(conns[0]), sl);
                const auto delays = std::exchange(b_.delays, std::nullopt);
                storePlace(p, coerce(init, p.type, sl), false, sl);
                b_.delays = delays;
                emitVoid(sir::Op::Store, {sir::Operand::slot(started), constant(sir::u2(1), 1).op},
                         sl);
                noteWrite({started, 0, 0, 0, 0});
                terminate(sir::Op::Br, {sir::Operand::block(rest)}, sl);
                setBlock(rest);
            }
            std::vector<Val> in;
            in.reserve(n);
            for (std::uint32_t i = 0; i < n; ++i)
                in.push_back(coerce(lowerExpr(*conns[1 + i]), bit, sl));
            Val out;
            if (!prim.isSequential) {
                std::vector<sir::Operand> ops{sir::Operand::typeRow(table)};
                for (const Val& v : in)
                    ops.push_back(v.op);
                out = emit(sir::Op::Udp, bit, std::move(ops), sl);
            } else {
                // Its own slots are not part of what wakes it.
                std::vector<sir::Operand> ops{
                    sir::Operand::typeRow(table),
                    emit(sir::Op::Load, bit, {sir::Operand::slot(state)}, sl).op};
                for (std::uint32_t i = 0; i < n; ++i)
                    ops.push_back(
                        emit(sir::Op::Load, bit, {sir::Operand::slot(prev, i, 1)}, sl).op);
                for (const Val& v : in)
                    ops.push_back(v.op);
                out = emit(sir::Op::UdpSeq, bit, std::move(ops), sl);
                emitVoid(sir::Op::Store, {sir::Operand::slot(state), out.op}, sl);
                std::vector<sir::Operand> all;
                for (std::uint32_t i = n; i-- > 0;)
                    all.push_back(in[i].op);
                const Val now =
                    n == 1 ? in[0] : emit(sir::Op::Concat, sir::u4(n), std::move(all), sl);
                emitVoid(sir::Op::Store, {sir::Operand::slot(prev), now.op}, sl);
                noteWrite({state, 0, 0, 0, 0});
                noteWrite({prev, 0, 0, 0, 0});
            }
            const Place p = placeOf(*outputPlace(conns[0]), sl);
            storePlace(p, coerce(out, p.type, sl), false, sl);
        },
        driveLevels(g.getDriveStrength()), g.getDelay());
}

std::array<Val, 3> Lowerer::delaysOf(const ast::TimingControl& t, slang::SourceLocation sl) {
    // Nothing is a delay past the largest time, larger than any other.
    std::optional<std::uint64_t> d[3] = {0, 0, 0};
    const auto smaller = [](std::optional<std::uint64_t> a, std::optional<std::uint64_t> b) {
        return !a ? b : !b ? a : std::min(a, b);
    };
    if (t.kind == ast::TimingControlKind::Delay) {
        d[0] = d[1] = d[2] = delayTicks(t.as<ast::DelayControl>().expr, sl);
    } else if (t.kind == ast::TimingControlKind::Delay3) {
        const auto& d3 = t.as<ast::Delay3Control>();
        d[0] = delayTicks(d3.expr1, sl);
        d[1] = d3.expr2 != nullptr ? delayTicks(*d3.expr2, sl) : d[0];
        d[2] = d3.expr3 != nullptr ? delayTicks(*d3.expr3, sl) : smaller(d[0], d[1]);
    } else {
        unsupported(sl, "this delay");
    }
    return {delayConstant(d[0]), delayConstant(d[1]), delayConstant(d[2])};
}

template <typename F>
void Lowerer::lowerCont(const std::string& name, slang::SourceLocation sl,
                        const ast::Symbol& evalAt, F&& body,
                        std::pair<std::uint8_t, std::uint8_t> strength,
                        const ast::TimingControl* delay) {
    sir::Process p;
    p.name = name;
    p.kind = sir::ProcessKind::Cont;
    p.instance = instance_;
    p.loc = loc(sl);
    p.blocks.resize(1);
    const auto first = static_cast<sir::ProcessId>(design_.processes.size());
    design_.processes.push_back(std::move(p));
    b_ = Body{};
    b_.proc = first;
    b_.scale = timeScaleOf(*evalAt.getParentScope());
    b_.scope = scopeNameOf(evalAt);
    evalSymbol_ = &evalAt;
    try {
        // A delay makes the writes drive.delay (SIR_SPEC.md 5.4).
        if (delay != nullptr) b_.delays = delaysOf(*delay, sl);
        body();
        if (!b_.dead) terminate(sir::Op::Ret, {}, sl);
    } catch (const Unsupported&) {
        dropProcesses(first);
        return;
    }
    if (strength != std::pair<std::uint8_t, std::uint8_t>{6, 6}) strengths_[first] = strength;
    // Evaluated again whenever an operand of the right-hand side changes
    // (10.3.2).
    triggerOnReads(sl);
}

void Lowerer::triggerOnReads(slang::SourceLocation sl) {
    implicitLocations_[b_.proc] = sl;
    implicitOwnReads_[b_.proc] = b_.reads;
    for (sir::SlotRange r : b_.reads) {
        // @* is sensitive to each identifier, unlike the longest static
        // prefix used by always_comb and always_latch (9.4.2.2).
        if (proc().kind == sir::ProcessKind::CombAt) r = {r.slot, 0, 0, 0, 0};
        const sir::Slot& s = design_.slot(r.slot);
        if (s.subroutine != sir::kNone) continue;
        bool dup = false;
        for (const sir::Trigger& t : proc().triggers)
            dup = dup || sir::covers(s, t.range, r);
        if (!dup) proc().triggers.push_back({r, sir::Edge::Any});
    }
}

void Lowerer::ensureStaticInit(const ast::Symbol& at, slang::SourceLocation sl) {
    // Every static initializer of an instance goes, in declaration order,
    // into one process that runs before all others (6.8, SIR_SPEC.md 7.1).
    if (staticInit_.find(instance_) == staticInit_.end()) {
        sir::Process p;
        // <instance>.$static; <package>::$static and $unit::$static (SIR_SPEC.md 7.1).
        p.name = design_.instance(instance_).scopeName +
                 ((design_.instance(instance_).kind == sir::InstanceKind::Module ||
                   design_.instance(instance_).kind == sir::InstanceKind::Program)
                      ? ".$static"
                      : "::$static");
        p.kind = sir::ProcessKind::StaticInit;
        p.instance = instance_;
        p.loc = loc(sl);
        p.blocks.resize(1);
        const auto id = static_cast<sir::ProcessId>(design_.processes.size());
        design_.processes.push_back(std::move(p));
        staticInit_[instance_] = id;
        staticLoc_[instance_] = sl;
        Body body;
        body.proc = id;
        body.scale = timeScaleOf(*at.getParentScope());
        staticBody_[instance_] = std::move(body);
    }
}

void Lowerer::checkStaticInitOrder() {
    // Static-init processes run in the order of their numbers, and each
    // runs its stores in order (SIR_SPEC.md 7.1): packages and compilation
    // units first, then the instances.
    std::map<const ast::Symbol*, std::pair<sir::ProcessId, std::size_t>> when;
    for (std::size_t i = 0; i < staticInits_.size(); ++i)
        when.try_emplace(staticInits_[i].first, staticInits_[i].second, i);
    const auto instanceOf = [&](const ast::Symbol& s) -> std::optional<sir::InstanceId> {
        for (const auto* m : {&slots_, &structSlots_})
            if (const auto it = m->find(&s); it != m->end())
                return design_.slot(it->second).instance;
        return std::nullopt;
    };
    for (std::size_t i = 0; i < staticInits_.size(); ++i) {
        const ast::VariableSymbol& v = *staticInits_[i].first;
        const ast::Expression* init = v.getInitializer();
        const std::optional<sir::InstanceId> own = instanceOf(v);
        if (init == nullptr || !own) continue;
        const std::pair<sir::ProcessId, std::size_t> mine{staticInits_[i].second, i};
        // Only the variables the expression names count, not those read
        // inside the functions it calls (P1).
        std::vector<const ast::Symbol*> early;
        const auto read = [&](const ast::Symbol& s) {
            if (!isVariable(s) || &s == &v || !inSlot(s)) return;
            const std::optional<sir::InstanceId> inst = instanceOf(s);
            if (!inst) return;
            // Another instance's variable: the standard sets no order
            // between instances. Any other: when its own is set later.
            const bool other =
                *inst != *own && (design_.instance(*inst).kind == sir::InstanceKind::Module ||
                                  design_.instance(*inst).kind == sir::InstanceKind::Program);
            const auto it = when.find(&s);
            if ((other || (it != when.end() && it->second > mine)) &&
                std::find(early.begin(), early.end(), &s) == early.end())
                early.push_back(&s);
        };
        auto visitor = ast::makeVisitor(
            [&](auto&, const ast::NamedValueExpression& e) { read(e.symbol); },
            [&](auto&, const ast::HierarchicalValueExpression& e) { read(e.symbol); });
        init->visit(visitor);
        if (early.empty()) continue;
        std::string names;
        for (std::size_t k = 0; k < early.size(); ++k)
            names += (k == 0                  ? ""
                      : k + 1 == early.size() ? " and "
                                              : ", ") +
                     std::string("'") + early[k]->getHierarchicalPath() + "'";
        warning(init->sourceRange.start(),
                "the initial value of '" + std::string(v.name) + "' reads " + names + ", whose " +
                    (early.size() == 1 ? "initial value" : "initial values") +
                    " may not be set yet");
    }
}

void Lowerer::lowerStaticInit(const ast::VariableSymbol& v, sir::SlotId slot,
                              slang::SourceLocation sl, bool membersOnly) {
    ensureStaticInit(v, sl);
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
            dst.slot = slot;
            if (const ast::Expression* init = membersOnly ? nullptr : v.getInitializer())
                assignAggregate(dst, v.getType(), *init, stores, sl);
            else
                memberDefaults(dst, v.getType(), stores, sl);
            flushStores(stores, false, sl);
        } else if (!design_.slot(slot).dims.empty()) {
            Stores stores;
            assignArray(slot, {}, *v.getInitializer(), stores, sl, true);  // element by element
            flushStores(stores, false, sl);
        } else {
            const sir::Type t = design_.slot(slot).type;
            const Val val = t.kind == sir::TypeKind::Event
                                ? lowerEventCopy(*v.getInitializer())
                                : copyValue(lowerExpr(*v.getInitializer()), t, sl);
            emitVoid(sir::Op::Store, {sir::Operand::slot(slot), val.op}, sl);
            noteWrite({slot, 0, 0, 0, 0});
        }
    } catch (const Unsupported&) {
        restore();
        throw;
    }
    // In the order of the stores: a subroutine lowered while this initial
    // value is (slotOf) puts its static variables' stores first.
    if (!membersOnly) staticInits_.emplace_back(&v, staticInit_.at(instance_));
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
    const auto first = static_cast<sir::ProcessId>(design_.processes.size());
    design_.processes.push_back(std::move(p));
    b_ = Body{};
    b_.proc = first;
    b_.scale = timeScaleOf(*pb.getParentScope());
    b_.scope = prefix;
    b_.fromAssertion = pb.isFromAssertion;
    evalSymbol_ = &pb;

    const ast::Statement& bodyStmt = pb.getBody();
    associateOutermostScope(bodyStmt);
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
                combLocals_[b_.proc] = b_.locals;
                implicitLocations_[b_.proc] = pb.location;
                implicitOwnReads_[b_.proc] = b_.reads;
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
                bool expression = false;  // an item on an expression
                bool instrumentedFixed = false;
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
                    bool instrumentedEvents = false;
                    for (const ast::TimingControl* t : items) {
                        if (!fixed) break;
                        if (t->kind != ast::TimingControlKind::SignalEvent) {
                            fixed = false;
                            break;
                        }
                        const auto& se = t->as<ast::SignalEventControl>();
                        std::optional<sir::SlotRange> r = eventSlot(se.expr);
                        if (!r && hasConditionChecks(se.expr)) {
                            // Probe the old fixed-signal shape without emitting
                            // checks. Observable selectors run in the actual
                            // event watch, at arming and reevaluation time.
                            struct RestoreFlag {
                                bool& flag;
                                bool saved;
                                ~RestoreFlag() { flag = saved; }
                            } restore{condX_, condX_};
                            condX_ = false;
                            r = eventSlot(se.expr);
                            instrumentedEvents = instrumentedEvents || r.has_value();
                        }
                        if (!r) {
                            fixed = false;
                            expression = true;
                            break;
                        }
                        triggers.push_back({*r, edgeOf(se.edge)});
                    }
                    if (fixed && !instrumentedEvents) {
                        inner = &ts.stmt;
                        // The iff guards, made once the triggers are known
                        // to be fixed (SIR_SPEC.md 7.2).
                        for (std::size_t k = 0; k < items.size(); ++k)
                            if (const ast::Expression* c =
                                    items[k]->as<ast::SignalEventControl>().iffCondition;
                                c != nullptr)
                                triggers[k].guard = guardOf(*c, "iff", c->sourceRange.start());
                    } else if (fixed) {
                        // The debug form keeps the accepted fixed-event shape,
                        // but its observable event expressions need a wait
                        // loop instead of permanently fixed subscriptions.
                        instrumentedFixed = true;
                    }
                }
                if (inner != nullptr) {
                    proc().kind = ff ? sir::ProcessKind::Ff : sir::ProcessKind::FfPlain;
                    proc().triggers = std::move(triggers);
                    lowerStatement(*inner);
                    if (!b_.dead) terminate(sir::Op::Ret, {}, pb.location);
                    break;
                }
                // An always_ff waits only at its event control, whose items
                // are kept as the process's triggers (SIR_SPEC.md 7.1).
                if (ff && !instrumentedFixed)
                    unsupported(pb.location,
                                expression ? "always_ff with an event control on an expression"
                                           : "always_ff without a single event control");
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
                    triggerOnReads(pb.location);
                    break;
                }
                if (!mayWait(bodyStmt))
                    unsupported(pb.location, "always procedure without a timing control");
                // Any other always: a general process that loops forever.
                proc().kind = sir::ProcessKind::General;
                // Instrumented formerly fixed subscriptions must arm before
                // startup events, just like the ff / ff-plain representation.
                // Keep the entry as the wait itself, with no executable prefix.
                proc().armBeforeStart = instrumentedFixed;
                const sir::BlockId loop = instrumentedFixed ? sir::kEntryBlock : newBlock();
                if (!instrumentedFixed)
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
        finishLexical();
    } catch (const Unsupported&) {
        dropProcesses(first);
    }
}

CompileResult compile(std::span<const std::string> args, const CompileOptions& options) {
    CompileResult result;
    slang::driver::Driver driver;
    if (!setUpDriver(driver, args)) return result;

    driver.diagEngine.removeClient(driver.textDiagClient);
    auto client = std::make_shared<slang::TextDiagnosticClient>();
    driver.diagEngine.addClient(client);

    if (!driver.parseAllSources()) {
        result.numErrors = driver.diagEngine.getNumErrors();
        result.diagnostics = client->getString();
        return result;
    }
    std::string configError;
    if (!chooseConfigTop(driver, configError)) {
        result.numErrors = driver.diagEngine.getNumErrors() + 1;
        result.diagnostics = client->getString() + configError;
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

    const auto queries = checkQueryExpressions(*compilation);
    result.numErrors += queries.numErrors;
    result.diagnostics += queries.diagnostics;
    if (result.numErrors != 0) return result;

    Lowerer lowerer(*compilation->getSourceManager(), globalPrecision(*compilation), options.condX,
                    options.initValues);
    try {
        lowerer.lower(compilation->getRoot());
    } catch (const Unsupported&) {
        // Declaration discovery can stop before any procedure owns a catch.
        // The diagnostic is already recorded, including a low-stack stop.
        result.diagnostics += lowerer.diagnostics();
        result.numWarnings += lowerer.numWarnings();
        result.numErrors += lowerer.numErrors();
        result.unsupported = true;
        return result;
    }
    result.diagnostics += lowerer.diagnostics();
    result.numWarnings += lowerer.numWarnings();
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

const ast::Expression* outputPlace(const ast::Expression* e) {
    if (e->kind == ast::ExpressionKind::Assignment) {
        const auto& a = e->as<ast::AssignmentExpression>();
        const ast::Expression* r = &a.right();
        while (r->kind == ast::ExpressionKind::Conversion)
            r = &r->as<ast::ConversionExpression>().operand();
        if (r->kind == ast::ExpressionKind::EmptyArgument) return &a.left();
    }
    return e;
}

}  // namespace kirimasim::front
