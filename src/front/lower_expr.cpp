// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of expressions to SIR. Selects are in lower_select.cpp, streaming
// in lower_stream.cpp, assignments in lower_assign.cpp, the scanning tasks in
// lower_scan.cpp, places in lower_place.cpp and system tasks in
// lower_system.cpp.
//
// Integral values of 1 to 64 bits only, so far. slang has already applied
// the SystemVerilog rules for expression widths and signedness: operands
// arrive converted (ConversionExpression), so most operators map to one
// instruction. coerce() turns a value into the type an instruction needs.

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lower_expr_helpers.h"
#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/EvalContext.h"
#include "slang/ast/ValuePath.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/LiteralExpressions.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/numeric/SVInt.h"

namespace kirimasim::front {

namespace {

std::uint64_t maskOf(std::uint32_t w) {
    return w >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << w) - 1;
}

/// The real functions of 20.8 with one argument, and with two.
constexpr std::string_view kRealMath1[] = {
    "$ln",   "$log10", "$exp",  "$sqrt", "$floor", "$ceil", "$sin",   "$cos",   "$tan",
    "$asin", "$acos",  "$atan", "$sinh", "$cosh",  "$tanh", "$asinh", "$acosh", "$atanh"};

/// System functions that only compute a value.
bool isPureSystemFunction(std::string_view n) {
    static constexpr std::string_view kPure[] = {"$time",
                                                 "$stime",
                                                 "$timeunit",
                                                 "$timeprecision",
                                                 "$realtime",
                                                 "$signed",
                                                 "$unsigned",
                                                 "$itor",
                                                 "$sformatf",
                                                 "$psprintf",
                                                 "len",
                                                 "getc",
                                                 "toupper",
                                                 "tolower",
                                                 "compare",
                                                 "icompare",
                                                 "substr",
                                                 "atoi",
                                                 "atohex",
                                                 "atooct",
                                                 "atobin",
                                                 "atoreal",
                                                 "$rtoi",
                                                 "$realtobits",
                                                 "$bitstoreal",
                                                 "$shortrealtobits",
                                                 "$bitstoshortreal",
                                                 "$clog2",
                                                 "$countbits",
                                                 "$countones",
                                                 "$onehot",
                                                 "$onehot0",
                                                 "$isunknown",
                                                 "$bits",
                                                 "$isunbounded",
                                                 "$left",
                                                 "$right",
                                                 "$low",
                                                 "$high",
                                                 "$increment",
                                                 "$size",
                                                 "$dimensions",
                                                 "$unpacked_dimensions"};
    return among(n, kPure) || among(n, kRealMath1) || among(n, kRealMath2);
}

/// The bytes of an integral value as a string (6.16): the most significant
/// byte first, zero bytes dropped.
std::string integerToString(const std::vector<std::uint64_t>& a,
                            const std::vector<std::uint64_t>& b, std::uint32_t width) {
    std::string s;
    for (std::uint32_t k = sir::integralBytes(width); k-- > 0;) {
        const std::size_t word = k / 8;
        const std::uint64_t known =
            (word < a.size() ? a[word] : 0) & ~(word < b.size() ? b[word] : 0);
        const auto c = static_cast<char>((known >> (k % 8 * 8)) & 0xffU);
        if (c != 0) s.push_back(c);
    }
    return s;
}

// A surrounding real context propagates into slang's floating operators.
// Recover the precision selected by a conditional's own operands (11.4.11),
// before its result is converted to that surrounding type. Explicit casts
// and calls retain their declared result type; a predicate does not count.
std::uint32_t floatingOperandWidth(const ast::Expression& expression) {
    std::uint32_t width = 0;
    std::vector<const ast::Expression*> pending{&expression};
    while (!pending.empty()) {
        const auto& e = pending.back()->unwrapImplicitConversions();
        pending.pop_back();
        if (!e.type->isFloating()) continue;
        switch (e.kind) {
            case ast::ExpressionKind::UnaryOp:
                pending.push_back(&e.as<ast::UnaryExpression>().operand());
                break;
            case ast::ExpressionKind::BinaryOp: {
                const auto& binary = e.as<ast::BinaryExpression>();
                pending.push_back(&binary.left());
                pending.push_back(&binary.right());
                break;
            }
            case ast::ExpressionKind::ConditionalOp: {
                const auto& conditional = e.as<ast::ConditionalExpression>();
                pending.push_back(&conditional.left());
                pending.push_back(&conditional.right());
                break;
            }
            case ast::ExpressionKind::MinTypMax:
                pending.push_back(&e.as<ast::MinTypMaxExpression>().selected());
                break;
            default:
                width = std::max(width, e.type->getBitWidth());
                if (width == 64) return width;
                break;
        }
    }
    return width;
}

// shortreal is C's float and real C's double (VALUE_SEMANTICS.md 1, 7.4). A
// double out of float's range becomes an infinity, as IEEE 754 rounds it;
// C++ leaves that to the implementation only when the infinities are
// values of float, which they are with IEEE 754.
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
              "shortreal and real need IEEE 754 float and double");

/// The bits a real constant keeps: a double, or a float for shortreal
/// (SIR_SPEC.md 6).
std::uint64_t realBits(double d, Type t) {
    if (t.kind == sir::TypeKind::ShortReal)
        return std::bit_cast<std::uint32_t>(static_cast<float>(d));
    return std::bit_cast<std::uint64_t>(d);
}

double realValue(const sir::Constant& c) {
    const std::uint64_t a = c.aWords.empty() ? 0 : c.aWords[0];
    if (c.type.kind == sir::TypeKind::ShortReal)
        return std::bit_cast<float>(static_cast<std::uint32_t>(a));
    return std::bit_cast<double>(a);
}

/// Integer to real (1.5): x and z bits count as 0; signed if the type is.
double integerToReal(std::uint64_t a, std::uint64_t b, Type t) {
    a &= ~b;
    if (sir::isSigned(t)) {
        const unsigned s = 64 - t.width;
        return static_cast<double>(static_cast<std::int64_t>(a << s) >> s);
    }
    return static_cast<double>(a);
}

/// Integer to shortreal, the same way, rounded once to the nearest float.
float integerToShortReal(std::uint64_t a, std::uint64_t b, Type t) {
    a &= ~b;
    if (sir::isSigned(t)) {
        const unsigned s = 64 - t.width;
        return static_cast<float>(static_cast<std::int64_t>(a << s) >> s);
    }
    return static_cast<float>(a);
}

/// Whether an expression contains something whose slang constant is not
/// used and that is computed at run time instead, where kirimasim reads the
/// standard otherwise than slang (SIR_SPEC.md 6): a conversion of a real to
/// an integer (VALUE_SEMANTICS.md 7.4: NaN and the infinities), a unary
/// plus of a four-state value (3.2: x or z makes every bit x), and atoi,
/// atohex, atooct and atobin (7.6: an underscore before the first digit).
bool untrustedConstant(const ast::Expression& e) {
    // slang's visitor walks every operand of every kind of expression, as
    // in sideEffects below, so an operand inside a pattern or inside is
    // looked at too.
    bool found = false;
    auto finder = ast::makeVisitor(
        [&](auto& v, const ast::CallExpression& call) {
            const auto name = call.getSubroutineName();
            if (call.isSystemCall() && !call.arguments().empty() &&
                call.arguments()[0]->type->isString() &&
                (name == "atoi" || name == "atohex" || name == "atooct" || name == "atobin"))
                found = true;  // slang needs a digit before any underscore.
            else if (call.isSystemCall() && !call.arguments().empty() &&
                     call.arguments()[0]->type->isUnpackedArray() &&
                     (name == "sum" || name == "product" || name == "and" || name == "or" ||
                      name == "xor" || name == "min" || name == "max"))
                found = true;  // The approved runtime rules also govern constant call expressions.
            else
                v.visitDefault(call);
        },
        [&](auto& v, const ast::UnaryExpression& u) {
            // slang folds +v as v; x or z makes every bit x
            // (VALUE_SEMANTICS.md 3.2).
            if (u.op == ast::UnaryOperator::Plus && u.type->isFourState())
                found = true;
            else
                v.visitDefault(u);
        },
        [&](auto& v, const ast::ConversionExpression& c) {
            if (c.operand().type->isFloating() && c.type->isIntegral())
                found = true;
            else
                v.visitDefault(c);
        },
        [&](auto& v, const ast::ConditionalExpression& c) {
            if (c.type->isFloating() && c.type->getBitWidth() == 64 &&
                floatingOperandWidth(c) == 32)
                found = true;
            else
                v.visitDefault(c);
        });
    e.visit(finder);
    return found;
}

/// Whether evaluating an expression can change anything. slang's visitor
/// walks every operand of every kind of expression (inside, patterns,
/// streaming and the rest), so a kind added later is walked too: the
/// operators that skip an operand rely on this (VALUE_SEMANTICS.md 3.1,
/// 3.9).
bool sideEffects(const ast::Expression& e) {
    bool found = false;
    auto finder = ast::makeVisitor(
        [&](auto&, const ast::AssignmentExpression&) { found = true; },
        [&](auto&, const ast::NewClassExpression&) { found = true; },  // runs a constructor
        [&](auto& v, const ast::CallExpression& c) {
            if (!c.isSystemCall() || !isPureSystemFunction(c.getSubroutineName()))
                found = true;
            else
                v.visitDefault(c);
        },
        [&](auto& v, const ast::UnaryExpression& u) {
            switch (u.op) {
                case ast::UnaryOperator::Preincrement:
                case ast::UnaryOperator::Predecrement:
                case ast::UnaryOperator::Postincrement:
                case ast::UnaryOperator::Postdecrement: found = true; break;
                default: v.visitDefault(u);
            }
        });
    e.visit(finder);
    return found;
}

}  // namespace

bool isPattern(const ast::Expression& e) {
    return e.kind == ast::ExpressionKind::SimpleAssignmentPattern ||
           e.kind == ast::ExpressionKind::StructuredAssignmentPattern ||
           e.kind == ast::ExpressionKind::ReplicatedAssignmentPattern;
}

std::vector<const ast::Expression*> patternElements(const ast::Expression& e) {
    switch (e.kind) {
        case ast::ExpressionKind::SimpleAssignmentPattern: {
            const auto el = e.as<ast::SimpleAssignmentPatternExpression>().elements();
            return {el.begin(), el.end()};
        }
        case ast::ExpressionKind::StructuredAssignmentPattern: {
            const auto el = e.as<ast::StructuredAssignmentPatternExpression>().elements();
            return {el.begin(), el.end()};
        }
        default: {
            const auto& r = e.as<ast::ReplicatedAssignmentPatternExpression>();
            const ast::Expression& countExpr = r.count();
            const slang::ConstantValue* cv = countExpr.getConstant();
            std::vector<const ast::Expression*> out;
            if (cv == nullptr || !cv->isInteger()) return out;
            const std::optional<std::int64_t> n = cv->integer().as<std::int64_t>();
            for (std::int64_t k = 0; n && k < *n; ++k)
                out.insert(out.end(), r.elements().begin(), r.elements().end());
            return out;
        }
    }
}

bool isVariable(const ast::Symbol& s) {
    return s.kind == ast::SymbolKind::Variable || s.kind == ast::SymbolKind::FormalArgument ||
           s.kind == ast::SymbolKind::ClassProperty || s.kind == ast::SymbolKind::ClockVar;
}

bool expressionHasSideEffects(const ast::Expression& e) { return sideEffects(e); }

// --- Constants and conversions -----------------------------------------------

std::optional<slang::ConstantValue> Lowerer::constantValue(const ast::Expression& e) {
    if (sideEffects(e) || untrustedConstant(e)) return std::nullopt;
    if (const slang::ConstantValue* cv = e.getConstant()) return *cv;
    if (evalSymbol_ == nullptr) return std::nullopt;
    // A specparam is a constant here (6.20.5); slang already rejects it
    // where a constant expression decides a type or a range.
    ast::EvalContext ctx(*evalSymbol_, ast::EvalFlags::SpecparamsAllowed);
    slang::ConstantValue cv = e.eval(ctx);
    if (cv.bad()) return std::nullopt;
    return cv;
}

std::optional<std::int64_t> Lowerer::constInt(const ast::Expression& e) {
    const std::optional<slang::ConstantValue> cv = constantValue(e);
    if (!cv || !cv->isInteger() || cv->integer().hasUnknown()) return std::nullopt;
    return cv->integer().as<std::int64_t>();
}

bool Lowerer::hasConditionChecks(const ast::Expression& e) const {
    if (!condX_) return false;
    bool found = false;
    auto finder = ast::makeVisitor(
        [&](auto&, const ast::ConditionalExpression&) { found = true; },
        [&](auto& visitor, const ast::CallExpression& call) {
            const auto name = call.getSubroutineName();
            const auto args = call.arguments();
            if (call.isSystemCall() && !args.empty()) {
                // Type queries do not evaluate their queried expression.
                // The length of a dynamic container does evaluate its value;
                // a dimension argument, when present, is evaluated as well.
                if (name == "$bits" || name == "$typename" || name == "$isunbounded" ||
                    name == "$dimensions" || name == "$unpacked_dimensions")
                    return;
                if (name == "$left" || name == "$right" || name == "$low" || name == "$high" ||
                    name == "$size" || name == "$increment") {
                    if (isDynamicContainer(*args[0]->type) || args[0]->type->isString())
                        args[0]->visit(visitor);
                    for (const auto* arg : args.subspan(1))
                        arg->visit(visitor);
                    return;
                }
                if (isBuiltinMethod(call, ast::SymbolKind::EnumType) &&
                    (name == "first" || name == "last" || name == "num"))
                    return;
            }
            visitor.visitDefault(call);
        });
    e.visit(finder);
    return found;
}

void Lowerer::observeRuntimeConstant(const ast::Expression& e) {
    if (hasConditionChecks(e)) static_cast<void>(lowerExpr(e));
}

std::optional<slang::ConstantValue> Lowerer::runtimeConstantValue(const ast::Expression& e) {
    return hasConditionChecks(e) ? std::nullopt : constantValue(e);
}

void Lowerer::markCondition(const Val& value, std::string_view construct,
                            slang::SourceLocation at) {
    if (condX_ && !construct.empty() && sir::isIntegral(value.type))
        emitVoid(Op::CondX, {Operand::str(std::string(construct)), value.op}, at);
}

Val Lowerer::realConstant(Type t, double d) {
    sir::Constant c;
    c.type = t;
    c.aWords = {realBits(d, t)};
    const auto id = static_cast<sir::ConstId>(design().constants.size());
    design().constants.push_back(std::move(c));
    return {Operand::constant(id), t};
}

Val Lowerer::stringConstant(std::string bytes) {
    sir::Constant c;
    c.type = Type{sir::TypeKind::String, 0};
    c.bytes = std::move(bytes);
    const auto id = static_cast<sir::ConstId>(design().constants.size());
    design().constants.push_back(std::move(c));
    return {Operand::constant(id), Type{sir::TypeKind::String, 0}};
}

Val Lowerer::constantFrom(const slang::ConstantValue& cv, Type t, slang::SourceLocation sl) {
    if (sir::isClass(t) && cv.isNullHandle()) return constant(t, 0);
    if (sir::isContainer(t)) {
        const auto element = design_.types[t.detail.index()].value;
        std::vector<Operand> values;
        if (cv.isUnpacked()) {
            for (const auto& value : cv.elements())
                values.push_back(constantFrom(value, element, sl).op);
        } else if (cv.isQueue()) {
            for (const auto& value : *cv.queue())
                values.push_back(constantFrom(value, element, sl).op);
        } else {
            unsupported(sl, "container constant of this kind");
        }
        return emit(Op::ContainerPack, t, std::move(values), sl);
    }
    if (cv.isString()) {
        if (!isString(t)) unsupported(sl, "a string constant as a value of this type");
        return stringConstant(cv.str());
    }
    if (cv.isReal() || cv.isShortReal()) {
        const double d =
            cv.isReal() ? static_cast<double>(cv.real()) : static_cast<double>(cv.shortReal());
        return coerce(realConstant(Type{sir::TypeKind::Real, 0}, d), t, sl);
    }
    if (!cv.isInteger()) unsupported(sl, "a constant of this kind");
    if (isReal(t)) {
        // An integer constant used as a real: the integer first, then 1.5.
        const slang::SVInt& v = cv.integer();
        const std::uint32_t w = v.getBitWidth();
        if (w == 0) unsupported(sl, "a value of no bits");
        const Type it = v.hasUnknown() ? (v.isSigned() ? sir::s4(w) : sir::u4(w))
                                       : (v.isSigned() ? sir::s2(w) : sir::u2(w));
        return coerce(constantFrom(cv, it, sl), t, sl);
    }
    const slang::SVInt& v = cv.integer();
    std::vector<std::uint64_t> a(sir::integralWords(t.width), 0);
    std::vector<std::uint64_t> b(a.size(), 0);
    const std::uint32_t have = v.getBitWidth();
    for (std::uint32_t i = 0; i < t.width; ++i) {
        // Bits beyond the constant's width extend it (they should not occur).
        const std::uint32_t k = i < have ? i : have - 1;
        const slang::logic_t bit =
            (i < have || v.isSigned()) ? v[static_cast<std::int32_t>(k)] : slang::logic_t(0);
        const bool x = bit.value == slang::logic_t::X_VALUE;
        const bool z = bit.value == slang::logic_t::Z_VALUE;
        if (bit.value == 1 || x) a[i / 64] |= std::uint64_t{1} << (i % 64);
        if (x || z) b[i / 64] |= std::uint64_t{1} << (i % 64);
    }
    if (!sir::isFourState(t)) std::fill(b.begin(), b.end(), 0);
    return constantWords(t, std::move(a), std::move(b));
}

Val Lowerer::coerce(Val v, Type want, slang::SourceLocation sl) {
    if (v.type == want) return v;
    if (sir::isClass(want)) {
        if (v.type.kind == sir::TypeKind::Chandle && v.op.kind == Operand::Kind::Const)
            return constant(want, 0);
        if (sir::isClass(v.type)) return emit(Op::ObjectView, want, {v.op}, sl);
    }
    if (sir::isContainer(v.type) && sir::isContainer(want))
        return emit(Op::ContainerCopy, want, {v.op}, sl);
    if (isString(want) && sir::isIntegral(v.type)) {
        // Integer to string: its bytes, zero bytes dropped (6.16).
        if (v.op.kind == Operand::Kind::Const) {
            const sir::Constant& c = design().constants[v.op.id];
            return stringConstant(integerToString(c.aWords, c.bWords, c.type.width));
        }
        return emit(Op::Itos, want, {v.op}, sl);
    }
    if (isString(v.type) && sir::isIntegral(want)) return emit(Op::Stoi, want, {v.op}, sl);
    if ((isReal(v.type) || isReal(want)) && (sir::isIntegral(v.type) || isReal(v.type)) &&
        (sir::isIntegral(want) || isReal(want))) {
        // Between reals, and between reals and integers (VALUE_SEMANTICS.md
        // 1.5, 7.4). A constant to a real is converted here; a real to an
        // integer always by the instruction, whose meaning the kernel and
        // valsem keep (7.4).
        if (v.op.kind == Operand::Kind::Const && isReal(want) &&
            (isReal(v.type) || v.type.width <= 64)) {
            const sir::Constant& c = design().constants[v.op.id];
            const std::uint64_t a = c.aWords.empty() ? 0 : c.aWords[0];
            const std::uint64_t b = c.bWords.empty() ? 0 : c.bWords[0];
            // An integer to shortreal rounds once, to the nearest float
            // (1.5), not through a double.
            const double d = isReal(c.type) ? realValue(c)
                             : want.kind == sir::TypeKind::ShortReal
                                 ? integerToShortReal(a, b, c.type)
                                 : integerToReal(a, b, c.type);
            return realConstant(want, d);
        }
        if (isReal(v.type) && isReal(want)) return emit(Op::Rconv, want, {v.op}, sl);
        return emit(isReal(want) ? Op::Itor : Op::Rtoi, want, {v.op}, sl);
    }
    if (!sir::isIntegral(v.type) || !sir::isIntegral(want))
        unsupported(sl, "conversion from " + sir::toString(v.type) + " to " + sir::toString(want));
    // A constant is converted here rather than by an instruction, when it
    // fits in a word.
    if (v.op.kind == Operand::Kind::Const && v.type.width <= 64 && want.width <= 64) {
        const sir::Constant& c = design().constants[v.op.id];
        std::uint64_t a = c.aWords.empty() ? 0 : c.aWords[0];
        std::uint64_t b = c.bWords.empty() ? 0 : c.bWords[0];
        const std::uint32_t w = v.type.width;
        if (want.width > w && sir::isSigned(v.type)) {
            const std::uint64_t fill = maskOf(want.width) & ~maskOf(w);
            if (((a >> (w - 1)) & 1U) != 0) a |= fill;
            if (((b >> (w - 1)) & 1U) != 0) b |= fill;
        }
        if (!sir::isFourState(want)) {
            a &= ~b;  // x and z become 0
            b = 0;
        }
        return constant(want, a, b);
    }
    // Width and signedness first, in the value's domain; then the domain.
    const Type sized = integral(sir::isFourState(v.type), sir::isSigned(want), want.width);
    if (sized != v.type) {
        const Op op = want.width < v.type.width                            ? Op::Trunc
                      : want.width > v.type.width && sir::isSigned(v.type) ? Op::Sext
                                                                           : Op::Zext;
        v = emit(op, sized, {v.op}, sl);
    }
    if (v.type != want) v = emit(sir::isFourState(want) ? Op::To4 : Op::To2, want, {v.op}, sl);
    return v;
}

Val Lowerer::propagate(Val v, Type want, slang::SourceLocation sl) {
    // An integral operand takes the sign of the type before it is
    // extended, so that it is sign-extended only when that type is
    // signed (11.8.2).
    if (sir::isIntegral(v.type) && sir::isIntegral(want))
        v = coerce(v, integral(sir::isFourState(v.type), sir::isSigned(want), v.type.width), sl);
    return coerce(v, want, sl);
}

Val Lowerer::lowerCondition(const ast::Expression& e, std::string_view construct,
                            slang::SourceLocation at) {
    Val v = lowerExpr(e);
    markCondition(v, construct, at);
    // 0.0 is false, anything else true (VALUE_SEMANTICS.md 7.5); a chandle
    // is false when null (7.8).
    if (isReal(v.type) || v.type.kind == sir::TypeKind::Chandle || sir::isClass(v.type))
        return emit(Op::Truth, sir::u2(1), {v.op}, e.sourceRange.start());
    if (!sir::isIntegral(v.type)) unsupported(e.sourceRange.start(), "condition of this type");
    if (v.type.width == 1 && !sir::isSigned(v.type)) return v;
    return emit(Op::Truth, integral(sir::isFourState(v.type), false, 1), {v.op},
                e.sourceRange.start());
}

std::optional<sir::SlotRange> Lowerer::eventSlot(const ast::Expression& e) {
    // A named event is waited on, not watched for changes (SCHEDULER_SPEC.md 9.5).
    if (e.type->isEvent() || hasConditionChecks(e)) return std::nullopt;
    // A fixed signal: a static variable or net, or a part of one at a
    // constant position (an element, a bit, a part or a member), whose
    // bits a subscription watches (SCHEDULER_SPEC.md 9.5). Anything else,
    // a run-time index included, is an expression.
    if (unpackedStruct(*e.type) != nullptr || e.type->isUnpackedArray()) return std::nullopt;
    const ast::Expression* x = &e;
    for (;;) {
        if (x->kind == ast::ExpressionKind::ElementSelect) {
            const auto& es = x->as<ast::ElementSelectExpression>();
            if (!constInt(es.selector())) return std::nullopt;
            x = &es.value();
        } else if (x->kind == ast::ExpressionKind::RangeSelect) {
            const auto& rs = x->as<ast::RangeSelectExpression>();
            if (!constInt(rs.left()) || !constInt(rs.right())) return std::nullopt;
            x = &rs.value();
        } else if (x->kind == ast::ExpressionKind::MemberAccess) {
            x = &x->as<ast::MemberAccessExpression>().value();
        } else {
            break;
        }
    }
    const ast::Symbol* sym = namedSymbol(*x);
    if (sym == nullptr || findFrameVar(*sym) || !inSlot(*sym)) return std::nullopt;
    // Its place, found as for a write: the meaning of the selects is the
    // same (SIR_SPEC.md 1).
    const Place p = placeOf(e, e.sourceRange.start());
    if (p.home != Place::Home::Slot || p.pos || (!p.indices.empty() && !p.elem))
        return std::nullopt;
    const sir::Slot& slot = design().slot(p.slot);
    switch (p.kind) {
        case Place::Kind::Slot:
            if (!slot.dims.empty()) return std::nullopt;
            break;
        case Place::Kind::Element: break;
        case Place::Kind::Bits:
            if (p.lsb < 0 || p.lsb + p.width > slot.type.width) return std::nullopt;
            break;
        default: return std::nullopt;
    }
    return rangeOf(p);
}

Operand Lowerer::eventOperand(const ast::Expression& e, slang::SourceLocation sl) {
    // Keep compact legacy slot operands for constant static selections.
    // Other selections use the shared typed-place/index rules (contract 5).
    const auto loaded = [&] { return loadPlace(placeOf(e, sl), sl).op; };
    std::vector<std::int64_t> indices;
    std::vector<const ast::Expression*> selectors;
    const ast::Expression* base = &e;
    while (base->kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = base->as<ast::ElementSelectExpression>();
        const std::optional<std::int64_t> idx = constInt(es.selector());
        if (!idx) return loaded();
        indices.insert(indices.begin(), *idx);
        selectors.insert(selectors.begin(), &es.selector());
        base = &es.value();
    }
    const ast::Symbol* sym = namedSymbol(*base);
    if (sym == nullptr || findFrameVar(*sym) || !inSlot(*sym)) return loaded();
    const sir::SlotId s = slotOf(sym->as<ast::ValueSymbol>(), sl);
    const sir::Slot& slot = design().slot(s);
    if (indices.size() != slot.dims.size()) unsupported(sl, "event given by this expression");
    for (std::size_t d = 0; d < indices.size(); ++d) {
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        if (indices[d] < std::min(l, r) || indices[d] > std::max(l, r)) return loaded();
    }
    Operand o = Operand::slot(s);
    if (!slot.dims.empty()) {
        o.firstElem = elementNumber(slot, indices);
        o.numElems = 1;
    }
    for (const auto* selector : selectors)
        observeRuntimeConstant(*selector);
    return o;
}

Val Lowerer::lowerTriggered(const ast::Expression& ev, Type rt, slang::SourceLocation sl) {
    // A trigger changes no slot, so an implicit sensitivity (always_comb,
    // always_latch, always @*, a continuous assignment, and the functions
    // they may call) would not see it.
    const bool implicit =
        !b_.isSub &&
        (proc().kind == sir::ProcessKind::Comb || proc().kind == sir::ProcessKind::Latch ||
         proc().kind == sir::ProcessKind::CombAt || proc().kind == sir::ProcessKind::Cont);
    if (implicit) unsupported(sl, "triggered in a function or under an implicit sensitivity");
    if (b_.isSub) triggeredReaders_.insert(b_.sub);
    const Operand o = eventOperand(ev, sl);
    const Type event{sir::TypeKind::Event, 0};
    const Val h = o.kind == Operand::Kind::Slot ? emit(Op::Load, event, {o}, sl) : Val{o, event};
    return coerce(emit(Op::Builtin, sir::u2(1), {Operand::str("triggered"), h.op}, sl), rt, sl);
}

std::optional<std::uint64_t> Lowerer::delayTicks(const ast::Expression& e,
                                                 slang::SourceLocation sl) {
    // Nothing for a delay of 2^64 ticks or more, which takes the time past
    // the largest time (SCHEDULER_SPEC.md 8).
    const std::optional<slang::ConstantValue> cv = constantValue(e);
    if (!cv) unsupported(sl, "delay that is not a constant");
    if (cv->isInteger()) {
        // x or z: 0. Negative: read as an unsigned 64-bit time (9.4.1).
        const slang::SVInt& v = cv->integer();
        if (v.hasUnknown()) return 0;
        std::optional<std::uint64_t> n;
        if (v.isSigned() && v.isNegative())
            n = (v.getBitWidth() < 64 ? v.sext(64) : v.trunc(64)).as<std::uint64_t>();
        else if (v.getActiveBits() <= 64)
            n = v.as<std::uint64_t>();
        if (!n) return std::nullopt;
        return sir::delayTicks(*n, timeScale(), precision());
    }
    if (!cv->isReal() && !cv->isShortReal()) unsupported(sl, "this delay value");
    const double r = cv->isReal() ? double(cv->real()) : double(cv->shortReal());
    // NaN and the infinities convert to x (VALUE_SEMANTICS.md 7.4), and a
    // delay of x is 0.
    if (std::isnan(r) || std::isinf(r)) return 0;
    const sir::TimeScale ts = timeScale();
    if (r >= 0) return sir::delayTicksReal(r, ts, precision());
    // Negative: the steps of the precision as delayValue counts them at run
    // time, rounded half away from zero, their low 64 bits read as an
    // unsigned count (9.4.1), then scaled; 2^64 ticks or more is past the
    // largest time.
    double unitSteps = 1;
    for (int i = ts.precision; i < ts.unit; ++i)
        unitSteps *= 10;
    const double low = std::fmod(-std::round(r * unitSteps), 18446744073709551616.0);
    const std::uint64_t n = std::uint64_t{0} - static_cast<std::uint64_t>(low);
    return sir::delayTicks(n, sir::TimeScale{ts.precision, ts.precision}, precision());
}

Val Lowerer::delayConstant(std::optional<std::uint64_t> ticks) {
    // 2^64 for a delay past the largest time: a run-time error when it
    // runs (SIR_SPEC.md 1).
    if (!ticks) return constantWords(sir::u2(65), {0, 1}, {0, 0});
    return constant(sir::u2(64), *ticks);
}

Val Lowerer::delayValue(const ast::Expression& e, slang::SourceLocation sl) {
    if (constantValue(e)) {
        observeRuntimeConstant(e);
        return delayConstant(delayTicks(e, sl));
    }
    // The same conversion at run time (SCHEDULER_SPEC.md 1, 8): an integral
    // delay is a count of time units, so it only scales to ticks; a real
    // one is rounded to the precision, half away from zero as any real to
    // integer conversion (VALUE_SEMANTICS.md 1.5), and then scaled. A
    // negative delay is read as a 64-bit unsigned value, and x or z as 0
    // (wait.delay, 9.4.1). Nothing is cut off: the ticks are counted in as
    // many bits as they need, and 2^64 or more takes the time past the
    // largest time (SIR_SPEC.md 1).
    const auto pow10 = [](int n) {
        std::uint64_t f = 1;
        for (int i = 0; i < n; ++i)
            f *= 10;
        return f;
    };
    const sir::TimeScale ts = timeScale();
    const sir::Type t64 = sir::u4(64);
    Val v = lowerExpr(e);
    int scale = ts.unit - precision();
    if (e.type->isFloating()) {
        const sir::Type real{sir::TypeKind::Real, 0};
        v = coerce(v, real, sl);
        v = emit(Op::Mul, real,
                 {v.op, realConstant(real, static_cast<double>(pow10(ts.unit - ts.precision))).op},
                 sl);
        // A real up to the largest one takes 1024 bits; a negative one is a
        // 64-bit unsigned value as above.
        const sir::Type wide = sir::u4(1088);
        const Val all = emit(Op::Rtoi, wide, {v.op}, sl);
        const Val low = coerce(emit(Op::Rtoi, t64, {v.op}, sl), wide, sl);
        const Val negative = compareValues(Op::Lt, v, realConstant(real, 0.0), sl);
        v = emit(Op::Select, wide, {negative.op, low.op, all.op}, sl);
        scale = ts.precision - precision();
    } else if (e.type->isIntegral()) {
        const std::uint32_t w = e.type->getBitWidth();
        v = coerce(v, w <= 64 ? t64 : sir::u4(w), sl);
    } else {
        unsupported(sl, "delay of this type");
    }
    if (scale == 0) return v;
    // pow10(scale) is below 2^64: 64 more bits hold the product.
    const sir::Type wt = sir::u4(v.type.width + 64);
    v = coerce(v, wt, sl);
    return emit(Op::Mul, wt, {v.op, constant(wt, pow10(scale)).op}, sl);
}

// --- Expressions -------------------------------------------------------------

void Lowerer::narrowRead(const ast::Expression& e, const ast::Expression& base,
                         std::size_t firstRead) {
    if (b_.reads.size() == firstRead || evalSymbol_ == nullptr) return;
    // Only comb/latch consume longest-static-prefix reads. Keep function
    // reads precise for their eventual comb caller, independent of which
    // caller first caused the function to be lowered.
    if (!b_.isSub && proc().kind != sir::ProcessKind::Comb &&
        proc().kind != sir::ProcessKind::Latch)
        return;
    auto& read = b_.reads.back();
    if (base.type->isUnpackedArray()) {
        // Unpacked arrays are flattened in declaration order. Stop at the
        // first dynamic index, retaining every element of that sub-array,
        // including when another constant index follows it.
        std::vector<const ast::Expression*> indices;
        const ast::Expression* cur = &e;
        for (;;) {
            cur = &cur->unwrapImplicitConversions();
            if (cur->kind == ast::ExpressionKind::ElementSelect) {
                const auto& select = cur->as<ast::ElementSelectExpression>();
                if (select.value().type->getCanonicalType().kind !=
                    ast::SymbolKind::FixedSizeUnpackedArrayType)
                    return;
                indices.push_back(&select.selector());
                cur = &select.value();
            } else if (isUnpackedMember(*cur)) {
                cur = &cur->as<ast::MemberAccessExpression>().value();
            } else {
                break;
            }
        }
        const auto& slot = design_.slot(read.slot);
        if (indices.size() != slot.dims.size()) return;
        std::reverse(indices.begin(), indices.end());
        std::uint64_t first = 0, count = sir::elementCount(slot);
        bool fixedPrefix = false;
        for (std::size_t d = 0; d < indices.size(); ++d) {
            const auto index = constInt(*indices[d]);
            if (!index) break;
            const std::int64_t left = slot.dims[d].left, right = slot.dims[d].right;
            const auto extent = static_cast<std::uint64_t>(std::abs(left - right) + 1);
            const std::int64_t offset = left <= right ? *index - left : left - *index;
            if (offset < 0 || static_cast<std::uint64_t>(offset) >= extent) return;
            count /= extent;
            first += static_cast<std::uint64_t>(offset) * count;
            fixedPrefix = true;
        }
        if (fixedPrefix) {
            read.firstElem = static_cast<std::uint32_t>(first);
            read.numElems = static_cast<std::uint32_t>(count);
        }
        return;
    }
    if (sideEffects(e) || untrustedConstant(e)) return;
    ast::EvalContext pathContext(*evalSymbol_, ast::EvalFlags::SpecparamsAllowed);
    const ast::ValuePath parent(base, pathContext), selected(e, pathContext);
    // A dynamic ancestor ends the static prefix: mem[i][2] still reads
    // all of mem. An expression such as (a + b)[2] reads both whole operands.
    if (!parent.isFullyStatic() || !selected.isFullyStatic() || parent.lsp == nullptr ||
        selected.lsp == nullptr)
        return;
    // Reading a modport expression can read several operands or compute
    // bits through carries. Only a connection that is itself a static
    // storage path lets the caller narrow the last underlying read.
    std::set<const ast::ValueSymbol*> indirect;
    for (const ast::ValueSymbol* root = parent.rootSymbol();
         root != nullptr && root->kind == ast::SymbolKind::ModportPort;) {
        if (!indirect.insert(root).second) return;
        const auto* connection = root->as<ast::ModportPortSymbol>().getConnectionExpr();
        if (connection == nullptr || sideEffects(*connection) || untrustedConstant(*connection))
            return;
        const ast::ValuePath connected(*connection, pathContext);
        if (!connected.isFullyStatic() || connected.lsp == nullptr) return;
        root = connected.rootSymbol();
    }
    const auto [parentLo, parentHi] = parent.lspBounds;
    const auto [lo, hi] = selected.lspBounds;
    if (lo < parentLo || hi > parentHi) return;
    // For a static path, its last read is the value's storage. Its constant
    // selectors add no reads. Keep the unpacked element range and offset
    // within a previously selected packed parent.
    read.lsb += static_cast<std::uint32_t>(lo - parentLo);
    read.width = static_cast<std::uint32_t>(hi - lo + 1);
}

Val Lowerer::lowerExpr(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    checkStack(sl);
    if (!substitutions_.empty())
        if (const auto it = substitutions_.find(&e); it != substitutions_.end()) return it->second;
    if (auto property = classPlace(e, sl)) return loadPlace(*property, sl);
    // General event value expressions need a separate value contract.
    if (e.type->isEvent()) unsupported(sl, "event used as a value");
    // slang's constant, unless something in the expression is not pure.
    // The walks over the whole expression come last, only for a constant:
    // asked at every step of a long chain they would cost its square.
    const slang::ConstantValue* cv = e.getConstant();
    if (cv != nullptr && (e.type->isIntegral() || e.type->isFloating()) &&
        (cv->isInteger() || cv->isReal() || cv->isShortReal()) &&
        e.kind != ast::ExpressionKind::Assignment && e.kind != ast::ExpressionKind::Call &&
        !sideEffects(e) && !untrustedConstant(e) && !hasConditionChecks(e))
        return constantFrom(*cv, typeOf(*e.type, sl), sl);

    switch (e.kind) {
        case ast::ExpressionKind::NewClass: return lowerClassNew(e.as<ast::NewClassExpression>());
        case ast::ExpressionKind::CopyClass:
            return emit(Op::ObjectCopy, typeOf(*e.type, sl),
                        {lowerExpr(e.as<ast::CopyClassExpression>().sourceExpr()).op}, sl);
        case ast::ExpressionKind::NewArray: return lowerContainerValue(e);
        case ast::ExpressionKind::UnboundedLiteral:
            if (!queueLast_.empty()) return queueLast_.back();
            unsupported(sl, "unbounded literal outside a queue selector");
        case ast::ExpressionKind::MinTypMax:
            // slang chooses the branch for the whole compilation; only
            // that expression is evaluated, even when it is not constant.
            return lowerExpr(e.as<ast::MinTypMaxExpression>().selected());
        case ast::ExpressionKind::IntegerLiteral:
            return constantFrom(slang::ConstantValue(e.as<ast::IntegerLiteral>().getValue()),
                                typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::UnbasedUnsizedIntegerLiteral:
            return constantFrom(
                slang::ConstantValue(e.as<ast::UnbasedUnsizedIntegerLiteral>().getValue()),
                typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::RealLiteral:
            return constantFrom(
                slang::ConstantValue(slang::real_t(e.as<ast::RealLiteral>().getValue())),
                typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::NullLiteral: {
            // null: the handle 0 (VALUE_SEMANTICS.md 7.8; class handles
            // are P4).
            if (e.type->isClass()) return constant(typeOf(*e.type, sl), 0);
            if (!e.type->isNull() && !e.type->isCHandle()) unsupported(sl, "null of this type");
            sir::Constant c;
            c.type = Type{sir::TypeKind::Chandle, 0};
            c.aWords = {0};
            const auto id = static_cast<sir::ConstId>(design().constants.size());
            design().constants.push_back(std::move(c));
            return {Operand::constant(id), Type{sir::TypeKind::Chandle, 0}};
        }
        case ast::ExpressionKind::StringLiteral:
            return constantFrom(e.as<ast::StringLiteral>().getIntValue(), typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::NamedValue:
        case ast::ExpressionKind::HierarchicalValue: {
            const ast::Symbol& sym = e.as<ast::ValueExpressionBase>().symbol;
            // The item of an array method's with clause, bound by the method.
            if (const auto it = iterators_.find(&sym); it != iterators_.end()) return it->second;
            if (sym.kind == ast::SymbolKind::Parameter)
                return constantFrom(sym.as<ast::ParameterSymbol>().getValue(), typeOf(*e.type, sl),
                                    sl);
            // A specparam is a constant like a parameter (6.20.5).
            if (sym.kind == ast::SymbolKind::Specparam)
                return constantFrom(sym.as<ast::SpecparamSymbol>().getValue(), typeOf(*e.type, sl),
                                    sl);
            if (sym.kind == ast::SymbolKind::EnumValue)
                return constantFrom(sym.as<ast::EnumValueSymbol>().getValue(), typeOf(*e.type, sl),
                                    sl);
            if (sym.kind == ast::SymbolKind::ModportPort) {
                // A modport port reads what it connects to: a member of the
                // interface instance, or the expression of a modport
                // expression (25.5, 25.5.4).
                const ast::Expression* c = sym.as<ast::ModportPortSymbol>().getConnectionExpr();
                if (c == nullptr) unsupported(sl, "modport port with no connection");
                return coerce(lowerExpr(*c), typeOf(*e.type, sl), sl);
            }
            if (unpackedStruct(*e.type) != nullptr)
                unsupported(sl, "unpacked structure used as a whole here");
            if (const std::optional<sir::FrameVarId> f = findFrameVar(sym)) {
                const sir::FrameVar& fv = frameVar(*f);
                if (!fv.dims.empty()) unsupported(sl, "an unpacked array as a value");
                // A ref argument reads what it refers to (SIR_SPEC.md 8).
                if (fv.type.kind == sir::TypeKind::Ref)
                    return emit(Op::LoadRef, fv.target, {Operand::frame(*f)}, sl);
                const auto read = emit(Op::Fload, fv.type, {Operand::frame(*f)}, sl);
                return sir::isClass(fv.type) ? coerce(read, typeOf(*e.type, sl), sl) : read;
            }
            if (!isVariable(sym) && sym.kind != ast::SymbolKind::Net)
                unsupported(sl, "reference to a " + std::string(ast::toString(sym.kind)));
            if (!inSlot(sym))
                unsupported(sl, "automatic variable '" + std::string(sym.name) + "' here");
            const sir::SlotId s = slotOf(sym.as<ast::ValueSymbol>(), sl);
            if (!design().slot(s).dims.empty()) unsupported(sl, "an unpacked array as a value");
            noteRead({s, 0, 0, 0, 0});
            return emit(Op::Load, design().slot(s).type, {Operand::slot(s)}, sl);
        }
        case ast::ExpressionKind::ElementSelect: return lowerElementSelect(e);
        case ast::ExpressionKind::RangeSelect: return lowerRangeSelect(e);
        case ast::ExpressionKind::SimpleAssignmentPattern:
        case ast::ExpressionKind::StructuredAssignmentPattern:
        case ast::ExpressionKind::ReplicatedAssignmentPattern: {
            if (isDynamicContainer(*e.type)) return lowerContainerValue(e);
            // A pattern of a packed type is the concatenation of its
            // elements, the first one most significant (10.9).
            if (!e.type->isIntegral())
                unsupported(sl, "expression '" + std::string(ast::toString(e.kind)) + "'");
            std::vector<Operand> ops;
            std::uint32_t width = 0;
            bool four = false;
            for (const ast::Expression* el : patternElements(e)) {
                const Val v = lowerExpr(*el);
                if (!sir::isIntegral(v.type)) unsupported(sl, "an element of this type");
                ops.push_back(v.op);
                width += v.type.width;
                four = four || sir::isFourState(v.type);
            }
            if (ops.empty()) unsupported(sl, "an empty pattern");
            const Val c = emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
            return coerce(c, typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::MemberAccess: {
            if (isUnpackedMember(e)) {
                // A member of an unpacked structure: its own slot or frame
                // variable (SIR_SPEC.md 4.2).
                if (structElement(*e.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                const std::optional<Storage> st = storageOf(e, sl);
                if (!st) unsupported(sl, "expression 'MemberAccess'");
                if (isArrayLeaf(*st)) unsupported(sl, "an unpacked array as a value");
                return coerce(loadPlace(storagePlace(*st), sl), typeOf(*e.type, sl), sl);
            }
            // A member of a packed structure or union: its bits, from the
            // field's offset (0 in a union).
            const auto& m = e.as<ast::MemberAccessExpression>();
            const ast::Type& vt = m.value().type->getCanonicalType();
            if (!vt.isIntegral() || m.member.kind != ast::SymbolKind::Field || vt.isTaggedUnion())
                unsupported(sl, "expression 'MemberAccess'");
            const auto firstRead = b_.reads.size();
            const Val v = lowerExpr(m.value());
            narrowRead(e, m.value(), firstRead);
            const std::uint32_t w = e.type->getBitWidth();
            const std::uint64_t lsb =
                vt.isPackedUnion() ? 0 : m.member.as<ast::FieldSymbol>().bitOffset;
            const Val r = emit(Op::Partsel, integral(sir::isFourState(v.type), false, w),
                               {v.op, constant(sir::u2(32), lsb).op}, sl);
            return coerce(r, typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::Inside: {
            const auto& in = e.as<ast::InsideExpression>();
            const Val v = lowerExpr(in.left());
            return coerce(lowerInside(v, in.rangeList(), sl), typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::Streaming:
            return lowerStream(e.as<ast::StreamingConcatenationExpression>(), sl);
        case ast::ExpressionKind::UnaryOp: return lowerUnary(e);
        case ast::ExpressionKind::BinaryOp: return lowerBinary(e);
        case ast::ExpressionKind::ConditionalOp: {
            const auto& c = e.as<ast::ConditionalExpression>();
            if (isDynamicContainer(*e.type)) return lowerContainerConditional(c, sl);
            if (c.conditions.size() != 1 || c.conditions[0].pattern != nullptr)
                unsupported(sl, "conditional operator with a pattern");
            const Type resultType = typeOf(*e.type, sl);
            const bool shortResult =
                resultType.kind == sir::TypeKind::Real && floatingOperandWidth(c) == 32;
            const Type rt = shortResult ? Type{sir::TypeKind::ShortReal, 0} : resultType;
            const auto branch = [&](const ast::Expression& expression) {
                return shortResult ? lowerFloatingExpr(expression, rt)
                                   : coerce(lowerExpr(expression), rt, sl);
            };
            Val cond = lowerExpr(*c.conditions[0].expr);
            markCondition(cond, "?:", c.opLoc);
            if (cond.type.width != 1 || sir::isSigned(cond.type))
                cond =
                    emit(Op::Truth, integral(sir::isFourState(cond.type), false, 1), {cond.op}, sl);
            if (sideEffects(c.left()) || sideEffects(c.right()) || condX_) {
                // Only the side a known condition picks is evaluated; both
                // when it is x or z (VALUE_SEMANTICS.md 3.9, 11.4.11).
                const sir::BlockId first = newBlock();
                const sir::BlockId notOne = newBlock();
                const sir::BlockId second = newBlock();
                const sir::BlockId both = newBlock();
                const sir::BlockId join = newBlock();
                const Val result = blockParam(join, rt);
                const Val isOne =
                    emit(Op::Ceq, sir::u2(1), {cond.op, constant(cond.type, 1).op}, sl);
                terminate(Op::BrIf, {isOne.op, Operand::block(first), Operand::block(notOne)}, sl);
                setBlock(notOne);
                const Val isZero =
                    emit(Op::Ceq, sir::u2(1), {cond.op, constant(cond.type, 0).op}, sl);
                terminate(Op::BrIf, {isZero.op, Operand::block(second), Operand::block(both)}, sl);
                setBlock(first);
                const Val l1 = branch(c.left());
                terminate(Op::Br, {Operand::block(join, 1), l1.op}, sl);
                setBlock(second);
                const Val r1 = branch(c.right());
                terminate(Op::Br, {Operand::block(join, 1), r1.op}, sl);
                setBlock(both);
                const Val l2 = branch(c.left());
                const Val r2 = branch(c.right());
                const Val merged = emit(Op::Select, rt, {cond.op, l2.op, r2.op}, sl);
                terminate(Op::Br, {Operand::block(join, 1), merged.op}, sl);
                setBlock(join);
                return coerce(result, resultType, sl);
            }
            const Val l = branch(c.left());
            const Val r = branch(c.right());
            return coerce(emit(Op::Select, rt, {cond.op, l.op, r.op}, sl), resultType, sl);
        }
        case ast::ExpressionKind::Concatenation: {
            if (isDynamicContainer(*e.type)) return lowerContainerValue(e);
            if (e.type->isString()) {
                // A string concatenation (11.4.12.2): every operand as a string.
                std::vector<Operand> ops;
                for (const ast::Expression* op : e.as<ast::ConcatenationExpression>().operands())
                    ops.push_back(coerce(lowerExpr(*op), Type{sir::TypeKind::String, 0}, sl).op);
                return emit(Op::Concat, Type{sir::TypeKind::String, 0}, std::move(ops), sl);
            }
            std::vector<Operand> ops;
            std::uint32_t width = 0;
            bool four = false;
            for (const ast::Expression* op : e.as<ast::ConcatenationExpression>().operands()) {
                if (op->type->isVoid()) continue;  // a zero-width replication
                const Val v = lowerExpr(*op);
                ops.push_back(v.op);
                width += v.type.width;
                four = four || sir::isFourState(v.type);
            }
            if (width == 0) unsupported(sl, "a value of no bits");
            const Val v = emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
            return coerce(v, typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::Replication: {
            const auto& r = e.as<ast::ReplicationExpression>();
            if (e.type->isString()) {
                // The count need not be constant (table 6-9).
                const Val n = lowerExpr(r.count());
                const Val v = coerce(lowerExpr(r.concat()), Type{sir::TypeKind::String, 0}, sl);
                return emit(Op::Repl, Type{sir::TypeKind::String, 0}, {n.op, v.op}, sl);
            }
            const std::optional<std::int64_t> n = constInt(r.count());
            if (!n || *n <= 0) unsupported(sl, "replication with this count");
            const Val v = lowerExpr(r.concat());
            const std::uint64_t width = static_cast<std::uint64_t>(*n) * v.type.width;
            if (width > UINT32_MAX) unsupported(sl, "a replication this wide");
            const Val count = constant(sir::u2(32), static_cast<std::uint64_t>(*n));
            const Val rv =
                emit(Op::Repl,
                     integral(sir::isFourState(v.type), false, static_cast<std::uint32_t>(width)),
                     {count.op, v.op}, sl);
            return coerce(rv, typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::Conversion: {
            if (isDynamicContainer(*e.type)) return lowerContainerValue(e);
            const auto& c = e.as<ast::ConversionExpression>();
            const Type want = typeOf(*e.type, sl);
            if (isString(want) && c.operand().type->isIntegral() && !sideEffects(c.operand())) {
                // A constant integral value as a string, string literals and
                // their concatenations among them: its bytes without the
                // zero ones (VALUE_SEMANTICS.md 7.6), at any width.
                if (const std::optional<slang::ConstantValue> cv = constantValue(c.operand());
                    cv && cv->isInteger()) {
                    if (const std::optional<std::string> text = stringBytes(cv->integer())) {
                        observeRuntimeConstant(c.operand());
                        return stringConstant(*text);
                    }
                }
            }
            if (c.conversionKind == ast::ConversionKind::StreamingConcat) {
                // A stream is left-aligned in a wider target, zeros on the
                // right, then cast to the target's type (11.4.14).
                if (c.operand().kind != ast::ExpressionKind::Streaming || !sir::isIntegral(want))
                    unsupported(sl, "streaming into this target");
                Val st = lowerStream(c.operand().as<ast::StreamingConcatenationExpression>(), sl);
                if (st.type.width > want.width) unsupported(sl, "stream wider than its target");
                if (st.type.width < want.width) {
                    const Val zeros = constant(sir::u2(want.width - st.type.width), 0);
                    st = emit(Op::Concat, integral(sir::isFourState(st.type), false, want.width),
                              {st.op, zeros.op}, sl);
                }
                return coerce(st, want, sl);
            }
            const Val v = lowerExpr(c.operand());
            // An operand of a context-determined expression takes the type
            // of the expression (11.8.2); other conversions extend by the
            // operand's own sign (11.8.3).
            return c.conversionKind == ast::ConversionKind::Propagated ? propagate(v, want, sl)
                                                                       : coerce(v, want, sl);
        }
        case ast::ExpressionKind::Assignment:
            return lowerAssignment(e.as<ast::AssignmentExpression>());
        case ast::ExpressionKind::LValueReference:
            // The target of the assignment operator being lowered.
            if (b_.lvalueRefs.empty()) unsupported(sl, "expression 'LValueReference'");
            return coerce(b_.lvalueRefs.back(), typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::Call: {
            const auto& call = e.as<ast::CallExpression>();
            if (const auto container = lowerContainerMethod(call, sl)) return *container;
            const std::string name(call.getSubroutineName());
            if (!call.isSystemCall()) return lowerCall(call);
            if (auto foreign = lowerForeignCall(call, sl)) return *foreign;
            if (name == "triggered" && call.arguments().size() == 1 &&
                call.arguments()[0]->type->isEvent())
                return lowerTriggered(*call.arguments()[0], typeOf(*e.type, sl), sl);
            if ((name == "$signed" || name == "$unsigned") && call.arguments().size() == 1)
                return coerce(lowerExpr(*call.arguments()[0]), typeOf(*e.type, sl), sl);
            if (name == "$time" || name == "$stime") {
                // $time is in the caller's time unit (SYSTEM_TASKS.md 5.1):
                // the VM divides the tick count by 10^(unit - precision).
                // $stime is its low 32 bits.
                const int shift = timeScale().unit - precision();
                if (shift < 0 || shift > 19) unsupported(sl, name + " with this time unit");
                std::uint64_t div = 1;
                for (int i = 0; i < shift; ++i)
                    div *= 10;
                const Val d = constant(sir::u2(64), div);
                const Val t = emit(Op::Syscall, sir::u4(64), {Operand::str("$time"), d.op}, sl);
                return name == "$time" ? t : coerce(t, typeOf(*e.type, sl), sl);
            }
            if ((name == "$timeunit" || name == "$timeprecision") && call.arguments().size() <= 1) {
                // The exponent of the unit or precision of the caller's design
                // element, of the instance named, of the compilation unit for
                // $unit, or of the simulation for $root, which is the global
                // precision (SYSTEM_TASKS.md 5.2, 20.4.1).
                const bool unit = name == "$timeunit";
                sir::TimeScale ts = timeScale();
                if (call.arguments().size() == 1) {
                    const ast::Expression& a = *call.arguments()[0];
                    const ast::Symbol* at =
                        a.kind == ast::ExpressionKind::ArbitrarySymbol
                            ? a.as<ast::ArbitrarySymbolExpression>().symbol.get()
                            : nullptr;
                    if (at != nullptr && at->kind == ast::SymbolKind::Instance)
                        ts = timeScaleOf(at->as<ast::InstanceSymbol>().body);
                    else if (at != nullptr && at->kind == ast::SymbolKind::CompilationUnit)
                        ts = timeScaleOf(at->as<ast::CompilationUnitSymbol>());
                    else if (at != nullptr && at->kind == ast::SymbolKind::Root)
                        ts = {precision(), precision()};
                    else
                        unsupported(sl, name + " of this argument");
                }
                const int x = unit ? ts.unit : ts.precision;
                return coerce(
                    constant(sir::s2(32), static_cast<std::uint64_t>(static_cast<std::int64_t>(x))),
                    typeOf(*e.type, sl), sl);
            }
            if (name == "$cast" && call.arguments().size() == 2 &&
                call.arguments()[0]->type->isClass())
                return lowerClassCast(call, false);
            const auto args = call.arguments();
            const Type real{sir::TypeKind::Real, 0};
            const auto arg = [&](std::size_t i, Type t) {
                return coerce(lowerExpr(*args[i]), t, sl);
            };
            const auto sys = [&](Type rt, std::vector<Operand> ops) {
                ops.insert(ops.begin(), Operand::str(name));
                return emit(Op::Syscall, rt, std::move(ops), sl);
            };
            const Type rt = typeOf(*e.type, sl);
            // Conversions (20.5): $itor and $rtoi are real and integer
            // conversions, but $rtoi truncates instead of rounding.
            if (name == "$itor" && args.size() == 1) return coerce(lowerExpr(*args[0]), rt, sl);
            if (name == "$rtoi" && args.size() == 1) return sys(rt, {arg(0, real).op});
            if (name == "$realtobits" && args.size() == 1) return sys(rt, {arg(0, real).op});
            if (name == "$bitstoreal" && args.size() == 1) return sys(rt, {arg(0, sir::u2(64)).op});
            if (name == "$shortrealtobits" && args.size() == 1)
                return sys(rt, {arg(0, Type{sir::TypeKind::ShortReal, 0}).op});
            if (name == "$bitstoshortreal" && args.size() == 1)
                return sys(rt, {arg(0, sir::u2(32)).op});
            if (name == "$realtime") {
                // In the caller's time unit, as a real (SYSTEM_TASKS.md 5.1).
                const int shift = timeScale().unit - precision();
                if (shift < 0 || shift > 19) unsupported(sl, "$realtime with this time unit");
                std::uint64_t div = 1;
                for (int i = 0; i < shift; ++i)
                    div *= 10;
                return sys(rt, {constant(sir::u2(64), div).op});
            }
            if (among(name, kRealMath1) && args.size() == 1) return sys(rt, {arg(0, real).op});
            if (among(name, kRealMath2) && args.size() == 2)
                return sys(rt, {arg(0, real).op, arg(1, real).op});
            if (name == "$typename" && args.size() == 1)
                return coerce(stringConstant(typeName(*args[0]->type, sl)), rt, sl);
            if (name == "$clog2" && args.size() == 1) {
                const Val v = lowerExpr(*args[0]);
                if (!sir::isIntegral(v.type)) unsupported(sl, "$clog2 of this type");
                return sys(rt, {v.op});
            }
            if (name == "$bits" || name == "$isunbounded" || name == "$left" || name == "$right" ||
                name == "$low" || name == "$high" || name == "$increment" || name == "$size" ||
                name == "$dimensions" || name == "$unpacked_dimensions") {
                // The queries of 20.6 and 20.7 on a fixed type are constants
                // (SYSTEM_TASKS.md 8). Dynamic dimension queries use container.size.
                // The queried value is not evaluated; an explicit dimension
                // argument is a runtime expression even when its value folds.
                if (const std::optional<slang::ConstantValue> cv = runtimeConstantValue(e))
                    return constantFrom(*cv, rt, sl);
                // $bits of an expression slang does not evaluate (a
                // hierarchical name) still comes from its fixed type alone;
                // the expression is not evaluated (20.6.2).
                const auto& c = e.as<ast::CallExpression>();
                if (name == "$bits" && c.arguments().size() == 1 &&
                    c.arguments()[0]->type->isFixedSize())
                    return constant(rt, c.arguments()[0]->type->getBitstreamWidth());
                if (name != "$bits" && name != "$isunbounded")
                    return lowerArrayQuery(*args[0], args.size() == 2 ? args[1] : nullptr, name, rt,
                                           sl);
                unsupported(sl, "system function " + name + " on this argument");
            }
            if (name == "$countbits" || name == "$countones" || name == "$onehot" ||
                name == "$onehot0" || name == "$isunknown") {
                // 20.9: $countbits counts the bits of the expression that
                // have one of the values of the control bits (the LSB of
                // each); the others are defined by it (SYSTEM_TASKS.md 8).
                if (args.empty()) unsupported(sl, "system function " + name);
                const Val v = bitstreamOf(*args[0], sl);
                if (!sir::isIntegral(v.type)) unsupported(sl, name + " of this type");
                std::vector<Operand> ops{Operand::str("$countbits"), v.op};
                const auto control = [&](std::uint64_t a, std::uint64_t b) {
                    ops.push_back(constant(sir::u4(1), a, b).op);
                };
                if (name == "$countbits") {
                    for (std::size_t i = 1; i < args.size(); ++i) {
                        Val c = lowerExpr(*args[i]);
                        if (!sir::isIntegral(c.type)) unsupported(sl, "control bit of this type");
                        c = coerce(c, integral(sir::isFourState(c.type), false, c.type.width), sl);
                        ops.push_back(coerce(c, sir::u4(1), sl).op);  // its LSB
                    }
                } else if (name == "$isunknown") {
                    control(0, 1);  // 'z
                    control(1, 1);  // 'x
                } else {
                    control(1, 0);  // '1
                }
                const Val n = emit(Op::Syscall, sir::s2(32), std::move(ops), sl);
                if (name == "$countbits" || name == "$countones") return coerce(n, rt, sl);
                const Op cmp = name == "$onehot" ? Op::Eq : name == "$onehot0" ? Op::Le : Op::Ne;
                const Val k = constant(sir::s2(32), name == "$isunknown" ? 0 : 1);
                return coerce(emit(cmp, sir::u2(1), {n.op, k.op}, sl), rt, sl);
            }
            if (name == "$sscanf" || name == "$fscanf" || name == "$value$plusargs")
                return lowerScanCall(call, rt, sl);
            if (name == "$fopen" && !args.empty() && args.size() <= 2) {
                // An mcd, or an fd with a type (SYSTEM_TASKS.md 6.1); an
                // integral name or type is read as a string.
                std::vector<Operand> ops;
                ops.reserve(args.size());
                for (std::size_t i = 0; i < args.size(); ++i)
                    ops.push_back(arg(i, Type{sir::TypeKind::String, 0}).op);
                return coerce(sys(sir::s2(32), std::move(ops)), rt, sl);
            }
            // File input (SYSTEM_TASKS.md 6.3, 6.4). An output argument goes
            // to a frame variable of its target's type, which the syscall
            // writes and front copies to the target (SIR_SPEC.md 8).
            const auto outTarget = [](const ast::Expression* t) {
                if (t->kind == ast::ExpressionKind::Assignment &&
                    t->as<ast::AssignmentExpression>().isLValueArg())
                    t = &t->as<ast::AssignmentExpression>().left();
                return t;
            };
            // Copies frame variable f to target t when the count n is above 0.
            const auto storeIfRead = [&](const ast::Expression& t, sir::FrameVarId f,
                                         const Val& n) {
                const Val read = emit(Op::Gt, sir::u2(1), {n.op, constant(sir::s2(32), 0).op}, sl);
                const sir::BlockId store = newBlock();
                const sir::BlockId next = newBlock();
                terminate(Op::BrIf, {read.op, Operand::block(store), Operand::block(next)}, sl);
                setBlock(store);
                lowerStore(t, emit(Op::Fload, frameVar(f).type, {Operand::frame(f)}, sl), false,
                           sl);
                terminate(Op::Br, {Operand::block(next)}, sl);
                setBlock(next);
            };
            if ((name == "$fgets" || name == "$ferror") && args.size() == 2) {
                // $fgets(line, fd) assigns when it read something; $ferror(fd,
                // str) always, an empty text when there is no error. A
                // string or an integral target.
                const bool gets = name == "$fgets";
                const ast::Expression* t = outTarget(args[gets ? 0 : 1]);
                const ast::Type& tt = t->type->getCanonicalType();
                if (!tt.isString() && !tt.isIntegral())
                    unsupported(sl, name + " into a target of this type");
                const sir::FrameVarId f = newFrameVar(typeOf(tt, sl));
                const Val d = lowerDescriptor(*args[gets ? 1 : 0], sl);
                const Val n =
                    sys(sir::s2(32), gets ? std::vector<Operand>{Operand::frame(f), d.op}
                                          : std::vector<Operand>{d.op, Operand::frame(f)});
                if (gets)
                    storeIfRead(*t, f, n);
                else
                    lowerStore(*t, emit(Op::Fload, frameVar(f).type, {Operand::frame(f)}, sl),
                               false, sl);
                return coerce(n, rt, sl);
            }
            if (name == "$fread" && args.size() >= 2 &&
                args[0]->type->getCanonicalType().isIntegral()) {
                // Integral destinations ignore start/count (21.3.4.4), but
                // value arguments are evaluated once, left to right. Copy-in
                // reads the actual now; a written output finds its target
                // again at copyback, as SIR_SPEC.md 8 requires.
                const ast::Expression* t = outTarget(args[0]);
                const Type tt = typeOf(*t->type, sl);
                const sir::FrameVarId f = newFrameVar(tt);
                emitVoid(Op::Fstore, {Operand::frame(f), coerce(lowerExpr(*t), tt, sl).op}, sl);
                const auto descriptor = lowerDescriptor(*args[1], sl);
                for (std::size_t i = 2; i < args.size(); ++i)
                    if (args[i]->kind != ast::ExpressionKind::EmptyArgument) lowerExpr(*args[i]);
                const Val n = sys(sir::s2(32), {Operand::frame(f), descriptor.op});
                storeIfRead(*t, f, n);
                return coerce(n, rt, sl);
            }
            if (name == "$fread" && args.size() >= 2) return lowerFreadMemory(call, rt, sl);
            if ((name == "$fgetc" || name == "$ftell" || name == "$feof" || name == "$rewind") &&
                args.size() == 1)
                return coerce(sys(sir::s2(32), {lowerDescriptor(*args[0], sl).op}), rt, sl);
            if (name == "$ungetc" && args.size() == 2) {
                const Val c = coerce(lowerExpr(*args[0]), sir::u2(8), sl);
                return coerce(sys(sir::s2(32), {c.op, lowerDescriptor(*args[1], sl).op}), rt, sl);
            }
            if (name == "$urandom" && args.size() <= 1) {
                // The next random number of the process's generator; a seed
                // starts it again first (SYSTEM_TASKS.md 13.3).
                std::vector<Operand> ops;
                if (!args.empty()) ops.push_back(coerce(lowerExpr(*args[0]), sir::u2(32), sl).op);
                return coerce(sys(sir::u2(32), std::move(ops)), rt, sl);
            }
            if (name == "$urandom_range" && (args.size() == 1 || args.size() == 2)) {
                const Val maxval = coerce(lowerExpr(*args[0]), sir::u2(32), sl);
                const Val minval = args.size() == 2 ? coerce(lowerExpr(*args[1]), sir::u2(32), sl)
                                                    : constant(sir::u2(32), 0);
                return coerce(sys(sir::u2(32), {maxval.op, minval.op}), rt, sl);
            }
            if ((name == "$random" && args.size() <= 1) ||
                (name.starts_with("$dist_") && !args.empty())) {
                // $random([seed]) and $dist_*(seed, arguments...): the seed
                // variable's low 32 bits, x and z as 0, go to a frame
                // variable that the syscall updates; the variable gets it
                // back, sign-extended, when it changed. $random alone uses
                // the Context's seed (SYSTEM_TASKS.md 12.3).
                // The arguments after the seed, as 32-bit integers.
                const auto call32 = [&](std::vector<Operand> ops) {
                    for (std::size_t i = 1; i < args.size(); ++i)
                        ops.push_back(coerce(lowerExpr(*args[i]), sir::s2(32), sl).op);
                    return sys(sir::s2(32), std::move(ops));
                };
                if (args.empty()) return coerce(call32({}), rt, sl);
                // slang gives $random its seed converted to int.
                const ast::Expression* seed = &outTarget(args[0])->unwrapImplicitConversions();
                if (seed->kind != ast::ExpressionKind::NamedValue &&
                    seed->kind != ast::ExpressionKind::HierarchicalValue &&
                    seed->kind != ast::ExpressionKind::ElementSelect &&
                    seed->kind != ast::ExpressionKind::RangeSelect &&
                    seed->kind != ast::ExpressionKind::MemberAccess)
                    error(sl, "the seed of " + name + " is to be a variable (20.14.1)");
                if (!seed->type->isIntegral() || seed->type->getBitWidth() < 32)
                    unsupported(sl, name + " with a seed variable narrower than 32 bits");
                const sir::FrameVarId f = newFrameVar(sir::s2(32));
                const Val before = coerce(lowerExpr(*seed), sir::s2(32), sl);
                emitVoid(Op::Fstore, {Operand::frame(f), before.op}, sl);
                const Val r = call32({Operand::frame(f)});
                const Val after = emit(Op::Fload, sir::s2(32), {Operand::frame(f)}, sl);
                const Val changed = emit(Op::Ne, sir::u2(1), {after.op, before.op}, sl);
                const sir::BlockId store = newBlock();
                const sir::BlockId next = newBlock();
                terminate(Op::BrIf, {changed.op, Operand::block(store), Operand::block(next)}, sl);
                setBlock(store);
                lowerStore(*seed, coerce(after, typeOf(*seed->type, sl), sl), false, sl);
                terminate(Op::Br, {Operand::block(next)}, sl);
                setBlock(next);
                return coerce(r, rt, sl);
            }
            if (name == "$fseek" && args.size() == 3) {
                const Val d = lowerDescriptor(*args[0], sl);
                const Val offset = coerce(lowerExpr(*args[1]), integral(true, true, 32), sl);
                const Val from = coerce(lowerExpr(*args[2]), integral(true, true, 32), sl);
                return coerce(sys(sir::s2(32), {d.op, offset.op, from.op}), rt, sl);
            }
            if (name == "$test$plusargs" && args.size() == 1) {
                // An integral argument is read as a string (21.6).
                const Val str = coerce(lowerExpr(*args[0]), Type{sir::TypeKind::String, 0}, sl);
                return coerce(sys(sir::u2(1), {str.op}), rt, sl);
            }
            if (std::optional<Val> v = lowerStringCall(call, sl)) return *v;
            if (std::optional<Val> v = lowerEnumCall(call, sl)) return *v;
            unsupported(sl, "system function " + name);
        }
        default: unsupported(sl, "expression '" + std::string(ast::toString(e.kind)) + "'");
    }
}

Val Lowerer::lowerFloatingExpr(const ast::Expression& expression, Type type) {
    const auto& e = expression.unwrapImplicitConversions();
    const auto sl = e.sourceRange.start();
    checkStack(sl);
    // Arithmetic inside a conditional branch received the same propagated
    // context. Restore its operand conversions and intermediate precision
    // as well. Explicit casts and calls keep their own conversion boundary.
    if (e.type->isFloating()) {
        if (e.kind == ast::ExpressionKind::UnaryOp) return lowerUnary(e, type);
        if (e.kind == ast::ExpressionKind::BinaryOp) return lowerBinary(e, type);
        if (e.kind == ast::ExpressionKind::MinTypMax)
            return lowerFloatingExpr(e.as<ast::MinTypMaxExpression>().selected(), type);
    }
    return coerce(lowerExpr(e), type, sl);
}

Val Lowerer::lowerUnary(const ast::Expression& e, std::optional<Type> floatingType) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& u = e.as<ast::UnaryExpression>();
    const Type rt = floatingType.value_or(typeOf(*e.type, sl));
    const auto operand = [&] {
        return floatingType ? lowerFloatingExpr(u.operand(), rt) : lowerExpr(u.operand());
    };
    switch (u.op) {
        case ast::UnaryOperator::Plus: {
            // An arithmetic operator: x or z in any bit makes every bit x
            // (VALUE_SEMANTICS.md 3.2), which adding 0 does.
            Val v = coerce(operand(), rt, sl);
            if (!sir::isFourState(rt)) return v;
            return emit(Op::Add, rt, {v.op, constant(rt, 0).op}, sl);
        }
        case ast::UnaryOperator::Minus:
            return emit(Op::Neg, rt, {coerce(operand(), rt, sl).op}, sl);
        case ast::UnaryOperator::BitwiseNot:
            return emit(Op::Not, rt, {coerce(lowerExpr(u.operand()), rt, sl).op}, sl);
        case ast::UnaryOperator::LogicalNot:
        case ast::UnaryOperator::BitwiseAnd:
        case ast::UnaryOperator::BitwiseOr:
        case ast::UnaryOperator::BitwiseXor:
        case ast::UnaryOperator::BitwiseNand:
        case ast::UnaryOperator::BitwiseNor:
        case ast::UnaryOperator::BitwiseXnor: {
            const Val v = lowerExpr(u.operand());
            if (u.op == ast::UnaryOperator::LogicalNot &&
                (isReal(v.type) || v.type.kind == sir::TypeKind::Chandle || sir::isClass(v.type)))
                return coerce(emit(Op::Lnot, sir::u2(1), {v.op}, sl), rt, sl);
            if (!sir::isIntegral(v.type)) unsupported(sl, "operand of this type");
            Op op = Op::Lnot;
            switch (u.op) {
                case ast::UnaryOperator::BitwiseAnd: op = Op::Rand; break;
                case ast::UnaryOperator::BitwiseOr: op = Op::Ror; break;
                case ast::UnaryOperator::BitwiseXor: op = Op::Rxor; break;
                case ast::UnaryOperator::BitwiseNand: op = Op::Rnand; break;
                case ast::UnaryOperator::BitwiseNor: op = Op::Rnor; break;
                case ast::UnaryOperator::BitwiseXnor: op = Op::Rxnor; break;
                default: break;
            }
            const Val r = emit(op, integral(sir::isFourState(v.type), false, 1), {v.op}, sl);
            return coerce(r, rt, sl);
        }
        case ast::UnaryOperator::Preincrement:
        case ast::UnaryOperator::Predecrement:
        case ast::UnaryOperator::Postincrement:
        case ast::UnaryOperator::Postdecrement: {
            const bool inc = u.op == ast::UnaryOperator::Preincrement ||
                             u.op == ast::UnaryOperator::Postincrement;
            const bool pre = u.op == ast::UnaryOperator::Preincrement ||
                             u.op == ast::UnaryOperator::Predecrement;
            // The operand is read and written as one place, its indices
            // evaluated once (11.4.1, 11.4.2); a real steps by 1.0.
            const Place p = placeOf(u.operand(), sl);
            const Val old = coerce(loadPlace(p, sl), rt, sl);
            const Val one = coerce(constant(sir::s2(32), 1), rt, sl);
            const Val n = emit(inc ? Op::Add : Op::Sub, rt, {old.op, one.op}, sl);
            storePlace(p, n, false, sl);
            return pre ? n : old;
        }
    }
    unsupported(sl, "unary operator");
}

Val Lowerer::compareValues(Op op, Val l, Val r, slang::SourceLocation sl) {
    if (sir::isClass(l.type) || sir::isClass(r.type)) {
        const auto type = sir::isClass(l.type) ? l.type : r.type;
        l = coerce(l, type, sl);
        r = coerce(r, type, sl);
        return emit(op, sir::u2(1), {l.op, r.op}, sl);
    }
    if (isString(l.type) || isString(r.type)) {
        // Strings compare by their characters (table 6-9).
        const Type st{sir::TypeKind::String, 0};
        l = coerce(l, st, sl);
        r = coerce(r, st, sl);
        return emit(op, sir::u2(1), {l.op, r.op}, sl);
    }
    if (l.type.kind == sir::TypeKind::Chandle || r.type.kind == sir::TypeKind::Chandle) {
        // Two chandles, or one and null, are equal when they are the same
        // handle; === and !== mean == and != (VALUE_SEMANTICS.md 7.8).
        if (l.type != r.type) unsupported(sl, "comparison of a chandle with this type");
        if (op != Op::Eq && op != Op::Ne && op != Op::Ceq && op != Op::Cne)
            unsupported(sl, "this operator on a chandle");
        const Op eq = op == Op::Eq || op == Op::Ceq ? Op::Eq : Op::Ne;
        return emit(eq, sir::u2(1), {l.op, r.op}, sl);
    }
    if (isReal(l.type) || isReal(r.type)) {
        // Compared as reals: real if either is, else shortreal (11.3.1).
        const bool shortOnly =
            l.type.kind != sir::TypeKind::Real && r.type.kind != sir::TypeKind::Real;
        const Type ct{shortOnly ? sir::TypeKind::ShortReal : sir::TypeKind::Real, 0};
        l = coerce(l, ct, sl);
        r = coerce(r, ct, sl);
        return emit(op, sir::u2(1), {l.op, r.op}, sl);
    }
    if (!sir::isIntegral(l.type) || !sir::isIntegral(r.type))
        unsupported(sl, "comparison of these types");
    const bool four = sir::isFourState(l.type) || sir::isFourState(r.type);
    const Type ct = integral(four, sir::isSigned(l.type) && sir::isSigned(r.type),
                             std::max(l.type.width, r.type.width));
    l = coerce(l, ct, sl);
    r = coerce(r, ct, sl);
    const bool twoStateResult = op == Op::Ceq || op == Op::Cne;
    return emit(op, integral(four && !twoStateResult, false, 1), {l.op, r.op}, sl);
}

Val Lowerer::bitOp(Op op, const Val& a, const Val& b, slang::SourceLocation sl) {
    // Two 1-bit values, four-state if either is.
    const Type t = integral(sir::isFourState(a.type) || sir::isFourState(b.type), false, 1);
    return emit(op, t, {coerce(a, t, sl).op, coerce(b, t, sl).op}, sl);
}

Val Lowerer::lowerInside(const Val& v, std::span<const ast::Expression* const> set,
                         slang::SourceLocation sl) {
    // The OR of the matches with every member of the set (11.4.13): 1 if
    // one is 1, else x if one is x (VALUE_SEMANTICS.md 3.10). Members are
    // evaluated in the order written.
    std::optional<Val> any;
    for (const ast::Expression* e : set) {
        if (sideEffects(*e)) unsupported(sl, "set member with side effects");
        const Val m = insideMatch(v, *e, sl);
        any = any ? bitOp(Op::Or, *any, m, sl) : m;
    }
    return any ? *any : constant(sir::u2(1), 0);
}

Val Lowerer::toleranceMatch(const Val& v, const ast::ValueRangeExpression& range,
                            slang::SourceLocation sl) {
    // [A +/- B] and [A +%- B] (11.4.13; VALUE_SEMANTICS.md 3.10): the bounds
    // are A minus and plus the tolerance in A's own type, not the type of
    // the set's comparison, swapped if needed. A real tolerance becomes an
    // integral one by truncation, not by rounding: $floor or $ceil leave a
    // whole number, which the conversion keeps. Only A has taken the type of
    // the comparison by now: the tolerance is self-determined.
    const Val a = lowerExpr(range.left().unwrapImplicitConversions());
    const Type real{sir::TypeKind::Real, 0};
    Val tolerance = lowerExpr(range.right());
    if (range.rangeKind == ast::ValueRangeKind::RelativeTolerance) {
        const Val product =
            emit(Op::Mul, real, {coerce(a, real, sl).op, coerce(tolerance, real, sl).op}, sl);
        tolerance = emit(Op::Div, real, {product.op, realConstant(real, 100.0).op}, sl);
    }
    if (isReal(tolerance.type) && sir::isIntegral(a.type)) {
        const Val wide = coerce(tolerance, real, sl);
        const Val down = emit(Op::Syscall, real, {Operand::str("$floor"), wide.op}, sl);
        const Val up = emit(Op::Syscall, real, {Operand::str("$ceil"), wide.op}, sl);
        const Val negative = compareValues(Op::Lt, wide, realConstant(real, 0.0), sl);
        tolerance = emit(Op::Select, real, {negative.op, up.op, down.op}, sl);
    }
    tolerance = coerce(tolerance, a.type, sl);
    const Val low = emit(Op::Sub, a.type, {a.op, tolerance.op}, sl);
    const Val high = emit(Op::Add, a.type, {a.op, tolerance.op}, sl);
    const Val swap = compareValues(Op::Gt, low, high, sl);
    const Val first = emit(Op::Select, a.type, {swap.op, high.op, low.op}, sl);
    const Val last = emit(Op::Select, a.type, {swap.op, low.op, high.op}, sl);
    // Only then at the type of the comparison, as a member is (11.8.2).
    const auto at = [&](const Val& bound) {
        return sir::isIntegral(v.type) && sir::isIntegral(bound.type) ? propagate(bound, v.type, sl)
                                                                      : bound;
    };
    return bitOp(Op::And, compareValues(Op::Ge, v, at(first), sl),
                 compareValues(Op::Le, v, at(last), sl), sl);
}

Val Lowerer::insideMatch(const Val& v, const ast::Expression& e, slang::SourceLocation sl) {
    if (e.kind == ast::ExpressionKind::ValueRange) {
        // [lo:hi] by >= and <=; a $ bound always holds (11.4.13).
        const auto& vr = e.as<ast::ValueRangeExpression>();
        if (vr.rangeKind != ast::ValueRangeKind::Simple) return toleranceMatch(v, vr, sl);
        std::optional<Val> in;
        const auto bound = [&](const ast::Expression& b, Op op) {
            if (b.unwrapImplicitConversions().kind == ast::ExpressionKind::UnboundedLiteral) return;
            const Val c = compareValues(op, v, lowerExpr(b), sl);
            in = in ? bitOp(Op::And, *in, c, sl) : c;
        };
        bound(vr.left(), Op::Ge);
        bound(vr.right(), Op::Le);
        return in ? *in : constant(sir::u2(1), 1);
    }
    if (isDynamicContainer(*e.type)) return containerMatch(v, lowerExpr(e), sl);
    if (e.type->getCanonicalType().isUnpackedArray()) {
        // Every element, down to single values (11.4.13).
        const std::optional<ArrayTarget> a = arrayTarget(e);
        if (!a) unsupported(sl, "set member of this array");
        return arrayMatch(v, baseOf(*a), a->indices, sl);
    }
    return memberMatch(v, lowerExpr(e), sl);
}

Val Lowerer::containerMatch(const Val& v, const Val& container, slang::SourceLocation sl) {
    // The OR of the matches with every element at the time of the operator,
    // as for an array of fixed size (11.4.13). An empty one matches nothing.
    const auto element = design_.types[container.type.detail.index()].value;
    const auto indexType = sir::u2(64);
    const auto count = emit(Op::ContainerSize, indexType, {container.op}, sl);
    const auto index = newFrameVar(indexType);
    emitVoid(Op::Fstore, {Operand::frame(index), constant(indexType, 0).op}, sl);
    const auto found = newFrameVar(sir::u4(1));
    emitVoid(Op::Fstore, {Operand::frame(found), constant(sir::u4(1), 0).op}, sl);
    const auto head = newBlock(), body = newBlock(), done = newBlock();
    jump(head, sl);
    setBlock(head);
    const auto current = emit(Op::Fload, indexType, {Operand::frame(index)}, sl);
    const auto more = emit(Op::Lt, sir::u2(1), {current.op, count.op}, sl);
    terminate(Op::BrIf, {more.op, Operand::block(body), Operand::block(done)}, sl);
    setBlock(body);
    const auto cell = emit(Op::ContainerRead, element, {container.op, current.op}, sl);
    const auto so_far = emit(Op::Fload, sir::u4(1), {Operand::frame(found)}, sl);
    const auto match = bitOp(Op::Or, so_far, memberMatch(v, cell, sl), sl);
    emitVoid(Op::Fstore, {Operand::frame(found), coerce(match, sir::u4(1), sl).op}, sl);
    const auto next = emit(Op::Add, indexType, {current.op, constant(indexType, 1).op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(index), next.op}, sl);
    jump(head, sl);
    setBlock(done);
    return emit(Op::Fload, sir::u4(1), {Operand::frame(found)}, sl);
}

Val Lowerer::memberMatch(const Val& v, Val m, slang::SourceLocation sl) {
    if (sir::isContainer(m.type)) return containerMatch(v, m, sl);
    // ==? for integral values, so that x and z in the member match any
    // bit; == for others (11.4.13). slang gives v the common type of the
    // set, the element types of its arrays counted (11.8.1); every member
    // is an operand of that type (11.8.2). slang converts a listed member,
    // not an element of an array, so an element takes the type here.
    const bool integralPair = sir::isIntegral(v.type) && sir::isIntegral(m.type);
    if (integralPair) m = propagate(m, v.type, sl);
    return compareValues(integralPair ? Op::Weq : Op::Eq, v, m, sl);
}

Val Lowerer::arrayMatch(const Val& v, ArrayBase s, std::vector<std::int64_t> indices,
                        slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim>& dims = dimsOf(s);
    if (indices.size() == dims.size())
        return memberMatch(v, loadPlace(elementPlace(s, indices), sl), sl);
    const sir::ArrayDim d = dims[indices.size()];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    std::optional<Val> any;
    for (std::int64_t i = d.left;; i += step) {
        indices.push_back(i);
        const Val m = arrayMatch(v, s, indices, sl);
        indices.pop_back();
        any = any ? bitOp(Op::Or, *any, m, sl) : m;
        if (i == d.right) break;
    }
    return *any;
}

Val Lowerer::lowerBinary(const ast::Expression& e, std::optional<Type> floatingType) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& b = e.as<ast::BinaryExpression>();
    const Type rt = floatingType.value_or(typeOf(*e.type, sl));
    const auto operand = [&](const ast::Expression& expression) {
        return floatingType ? lowerFloatingExpr(expression, rt) : lowerExpr(expression);
    };
    using BO = ast::BinaryOperator;
    if ((b.op == BO::Equality || b.op == BO::Inequality || b.op == BO::CaseEquality ||
         b.op == BO::CaseInequality) &&
        isAggregate(*b.left().type))
        return coerce(compareAggregates(b, sl), rt, sl);
    const auto same = [&](Op op) {
        const Val l = coerce(operand(b.left()), rt, sl);
        const Val r = coerce(operand(b.right()), rt, sl);
        return emit(op, rt, {l.op, r.op}, sl);
    };
    // Comparisons: both sides in one type; a 1-bit result.
    const auto compare = [&](Op op) {
        const Val l = lowerExpr(b.left());
        const Val r = lowerExpr(b.right());
        return coerce(compareValues(op, l, r, sl), rt, sl);
    };
    const auto logical = [&](Op op) {
        const auto ok = [](Type t) { return sir::isIntegral(t) || isReal(t); };
        if ((sideEffects(b.right()) || condX_) && op != Op::Leqv) {
            // &&, || and -> do not evaluate the right side when the left
            // one decides (VALUE_SEMANTICS.md 3.1, 11.4.7): branches, so
            // that its side effects do not happen (SIR_SPEC.md 5.2).
            const Val l = lowerExpr(b.left());
            if (!ok(l.type)) unsupported(sl, "logical operator on these types");
            const Val t = emit(Op::Truth, integral(sir::isFourState(l.type), false, 1), {l.op}, sl);
            const std::uint64_t decider = op == Op::Lor ? 1 : 0;
            const std::uint64_t decided = op == Op::Land ? 0 : 1;
            const Val stop = emit(Op::Ceq, sir::u2(1), {t.op, constant(t.type, decider).op}, sl);
            const sir::BlockId right = newBlock();
            const sir::BlockId join = newBlock();
            const Val result = blockParam(join, sir::u4(1));
            terminate(Op::BrIf,
                      {stop.op, Operand::block(join, 1), constant(sir::u4(1), decided).op,
                       Operand::block(right)},
                      sl);
            setBlock(right);
            const Val r = lowerExpr(b.right());
            if (!ok(r.type)) unsupported(sl, "logical operator on these types");
            const bool four = sir::isFourState(t.type) || sir::isFourState(r.type);
            const Val v =
                coerce(emit(op, integral(four, false, 1), {t.op, r.op}, sl), sir::u4(1), sl);
            terminate(Op::Br, {Operand::block(join, 1), v.op}, sl);
            setBlock(join);
            return coerce(result, rt, sl);
        }
        const Val l = lowerExpr(b.left());
        const Val r = lowerExpr(b.right());
        if (!ok(l.type) || !ok(r.type)) unsupported(sl, "logical operator on these types");
        const bool four = sir::isFourState(l.type) || sir::isFourState(r.type);
        return coerce(emit(op, integral(four, false, 1), {l.op, r.op}, sl), rt, sl);
    };
    const auto shift = [&](Op op) {
        const Val l = coerce(lowerExpr(b.left()), rt, sl);
        const Val r = lowerExpr(b.right());
        if (!sir::isIntegral(r.type)) unsupported(sl, "shift amount of this type");
        return emit(op, rt, {l.op, r.op}, sl);
    };
    switch (b.op) {
        case BO::Add: return same(Op::Add);
        case BO::Subtract: return same(Op::Sub);
        case BO::Multiply: return same(Op::Mul);
        case BO::Divide: return same(Op::Div);
        case BO::Mod: return same(Op::Mod);
        case BO::BinaryAnd: return same(Op::And);
        case BO::BinaryOr: return same(Op::Or);
        case BO::BinaryXor: return same(Op::Xor);
        case BO::BinaryXnor: return same(Op::Xnor);
        case BO::Equality: return compare(Op::Eq);
        case BO::Inequality: return compare(Op::Ne);
        case BO::CaseEquality: return compare(Op::Ceq);
        case BO::CaseInequality: return compare(Op::Cne);
        case BO::GreaterThanEqual: return compare(Op::Ge);
        case BO::GreaterThan: return compare(Op::Gt);
        case BO::LessThanEqual: return compare(Op::Le);
        case BO::LessThan: return compare(Op::Lt);
        case BO::WildcardEquality: return compare(Op::Weq);
        case BO::WildcardInequality: return compare(Op::Wne);
        case BO::LogicalAnd: return logical(Op::Land);
        case BO::LogicalOr: return logical(Op::Lor);
        case BO::LogicalImplication: return logical(Op::Limp);
        case BO::LogicalEquivalence: return logical(Op::Leqv);
        case BO::LogicalShiftLeft:
        case BO::ArithmeticShiftLeft: return shift(Op::Shl);
        case BO::LogicalShiftRight: return shift(Op::Shr);
        case BO::ArithmeticShiftRight: return shift(Op::Ashr);
        case BO::Power: {
            const Val l = coerce(operand(b.left()), rt, sl);
            Val r = operand(b.right());
            if (isReal(rt)) r = coerce(r, rt, sl);  // a real power takes a real exponent
            if (!isReal(rt) && !sir::isIntegral(r.type)) unsupported(sl, "** with this exponent");
            return emit(Op::Pow, rt, {l.op, r.op}, sl);
        }
    }
    unsupported(sl, "binary operator");
}

}  // namespace kirimasim::front
