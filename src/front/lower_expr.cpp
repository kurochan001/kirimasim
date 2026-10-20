// Lowering of expressions, assignments and system task calls to SIR.
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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/EvalContext.h"
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

using sir::Op;
using sir::Operand;
using sir::Type;

std::uint64_t maskOf(std::uint32_t w) {
    return w >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << w) - 1;
}

/// An integral type of the given domain, signedness and width.
Type integral(bool four, bool isSigned, std::uint32_t w) {
    if (four) return isSigned ? sir::s4(w) : sir::u4(w);
    return isSigned ? sir::s2(w) : sir::u2(w);
}

/// The real functions of 20.8 with one argument, and with two.
constexpr std::string_view kRealMath1[] = {
    "$ln",   "$log10", "$exp",  "$sqrt", "$floor", "$ceil", "$sin",   "$cos",   "$tan",
    "$asin", "$acos",  "$atan", "$sinh", "$cosh",  "$tanh", "$asinh", "$acosh", "$atanh"};
constexpr std::string_view kRealMath2[] = {"$pow", "$atan2", "$hypot"};

bool among(std::string_view n, std::span<const std::string_view> set) {
    return std::find(set.begin(), set.end(), n) != set.end();
}

/// System functions that only compute a value.
bool isPureSystemFunction(std::string_view n) {
    static constexpr std::string_view kPure[] = {"$time",
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

bool isString(Type t) { return t.kind == sir::TypeKind::String; }

/// The bytes of a known integral constant from the most significant one,
/// without the zero bytes, as a string (6.16); nothing if it has x or z.
std::optional<std::string> stringBytes(const slang::SVInt& v) {
    if (v.hasUnknown()) return std::nullopt;
    const std::uint32_t n = (v.getBitWidth() + 7) / 8;
    const slang::SVInt x = v.getBitWidth() < n * 8 ? v.zext(n * 8) : v;
    std::string out;
    for (std::uint32_t k = n; k-- > 0;) {
        const std::optional<std::uint64_t> byte =
            x.slice(static_cast<std::int32_t>(8 * k + 7), static_cast<std::int32_t>(8 * k))
                .as<std::uint64_t>();
        if (byte && *byte != 0) out.push_back(static_cast<char>(*byte));
    }
    return out;
}

/// The bytes of an integral value as a string (6.16): the most significant
/// byte first, zero bytes dropped.
std::string integerToString(const std::vector<std::uint64_t>& a,
                            const std::vector<std::uint64_t>& b, std::uint32_t width) {
    std::string s;
    for (std::uint32_t k = (width + 7) / 8; k-- > 0;) {
        const std::size_t word = k / 8;
        const std::uint64_t known =
            (word < a.size() ? a[word] : 0) & ~(word < b.size() ? b[word] : 0);
        const auto c = static_cast<char>((known >> (k % 8 * 8)) & 0xffU);
        if (c != 0) s.push_back(c);
    }
    return s;
}

bool isReal(Type t) { return t.kind == sir::TypeKind::Real || t.kind == sir::TypeKind::ShortReal; }

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

/// Whether an expression contains something whose slang constant is not
/// used and that is computed at run time instead, where kirimasim reads the
/// standard otherwise than slang (SIR_SPEC.md 6): a conversion of a real to
/// an integer (VALUE_SEMANTICS.md 7.4: NaN and the infinities) and a unary
/// plus of a four-state value (3.2: x or z makes every bit x).
bool untrustedConstant(const ast::Expression& e) {
    const auto any = [](std::span<const ast::Expression* const> es) {
        for (const ast::Expression* x : es)
            if (untrustedConstant(*x)) return true;
        return false;
    };
    switch (e.kind) {
        case ast::ExpressionKind::Call: return any(e.as<ast::CallExpression>().arguments());
        case ast::ExpressionKind::UnaryOp: {
            // slang folds +v as v; x or z makes every bit x
            // (VALUE_SEMANTICS.md 3.2).
            const auto& u = e.as<ast::UnaryExpression>();
            if (u.op == ast::UnaryOperator::Plus && e.type->isFourState()) return true;
            return untrustedConstant(u.operand());
        }
        case ast::ExpressionKind::BinaryOp: {
            const auto& b = e.as<ast::BinaryExpression>();
            return untrustedConstant(b.left()) || untrustedConstant(b.right());
        }
        case ast::ExpressionKind::ConditionalOp: {
            const auto& c = e.as<ast::ConditionalExpression>();
            for (const auto& cond : c.conditions)
                if (untrustedConstant(*cond.expr)) return true;
            return untrustedConstant(c.left()) || untrustedConstant(c.right());
        }
        case ast::ExpressionKind::Concatenation:
            return any(e.as<ast::ConcatenationExpression>().operands());
        case ast::ExpressionKind::Replication:
            return untrustedConstant(e.as<ast::ReplicationExpression>().concat());
        case ast::ExpressionKind::Conversion: {
            const ast::Expression& op = e.as<ast::ConversionExpression>().operand();
            if (op.type->isFloating() && e.type->isIntegral()) return true;
            return untrustedConstant(op);
        }
        case ast::ExpressionKind::ElementSelect: {
            const auto& s = e.as<ast::ElementSelectExpression>();
            return untrustedConstant(s.value()) || untrustedConstant(s.selector());
        }
        case ast::ExpressionKind::RangeSelect: {
            const auto& s = e.as<ast::RangeSelectExpression>();
            return untrustedConstant(s.value()) || untrustedConstant(s.left()) ||
                   untrustedConstant(s.right());
        }
        default: return false;
    }
}

/// Whether evaluating an expression can change anything.
bool sideEffects(const ast::Expression& e) {
    switch (e.kind) {
        case ast::ExpressionKind::Assignment: return true;
        case ast::ExpressionKind::Call: {
            const auto& c = e.as<ast::CallExpression>();
            if (!c.isSystemCall() || !isPureSystemFunction(c.getSubroutineName())) return true;
            for (const ast::Expression* arg : c.arguments())
                if (sideEffects(*arg)) return true;
            return false;
        }
        case ast::ExpressionKind::UnaryOp: {
            const auto& u = e.as<ast::UnaryExpression>();
            switch (u.op) {
                case ast::UnaryOperator::Preincrement:
                case ast::UnaryOperator::Predecrement:
                case ast::UnaryOperator::Postincrement:
                case ast::UnaryOperator::Postdecrement: return true;
                default: return sideEffects(u.operand());
            }
        }
        case ast::ExpressionKind::BinaryOp: {
            const auto& b = e.as<ast::BinaryExpression>();
            return sideEffects(b.left()) || sideEffects(b.right());
        }
        case ast::ExpressionKind::ConditionalOp: {
            const auto& c = e.as<ast::ConditionalExpression>();
            for (const auto& cond : c.conditions)
                if (sideEffects(*cond.expr)) return true;
            return sideEffects(c.left()) || sideEffects(c.right());
        }
        case ast::ExpressionKind::Concatenation:
            for (const ast::Expression* op : e.as<ast::ConcatenationExpression>().operands())
                if (sideEffects(*op)) return true;
            return false;
        case ast::ExpressionKind::Replication:
            return sideEffects(e.as<ast::ReplicationExpression>().concat());
        case ast::ExpressionKind::Conversion:
            return sideEffects(e.as<ast::ConversionExpression>().operand());
        case ast::ExpressionKind::MemberAccess:
            return sideEffects(e.as<ast::MemberAccessExpression>().value());
        case ast::ExpressionKind::ElementSelect: {
            const auto& s = e.as<ast::ElementSelectExpression>();
            return sideEffects(s.value()) || sideEffects(s.selector());
        }
        case ast::ExpressionKind::RangeSelect: {
            const auto& s = e.as<ast::RangeSelectExpression>();
            return sideEffects(s.value()) || sideEffects(s.left()) || sideEffects(s.right());
        }
        default: return false;
    }
}

/// The position from the least significant bit of declared index `idx` in
/// a range, or nothing if it is outside.
std::optional<std::int64_t> positionOf(std::int64_t idx, const slang::ConstantRange& r) {
    const std::int64_t pos = r.left >= r.right ? idx - r.right : r.right - idx;
    const std::int64_t width =
        std::int64_t{r.left >= r.right ? r.left - r.right : r.right - r.left} + 1;
    if (pos < 0 || pos >= width) return std::nullopt;
    return pos;
}

/// The specifiers of a $display format, in order, with %x as h. Returns
/// false and what is wrong in `bad`: "%" for a % with no conversion after
/// it, else a specifier or a flag that is not supported yet. `extra` gets
/// the first flag that the standard does not define, a - or a 0 in a
/// format other than %e, %f and %g, as "'-' with %d" (SYSTEM_TASKS.md
/// 3.12).
bool scanFormat(std::string_view f, std::vector<char>& convs, std::string& bad,
                std::string& extra) {
    const auto digit = [&](std::size_t i) { return i < f.size() && f[i] >= '0' && f[i] <= '9'; };
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%') continue;
        if (++i >= f.size()) {
            bad = "%";
            return false;
        }
        if (f[i] == '%') continue;
        std::string flags;
        while (i < f.size() && std::string_view("-+ #").find(f[i]) != std::string_view::npos)
            flags += f[i++];
        // A width that starts with 0 and is not 0 is the 0 flag; %0d is the
        // width 0 (3.3).
        const bool leadingZero = i < f.size() && f[i] == '0';
        bool zeroFlag = false;
        for (; digit(i); ++i)
            zeroFlag = zeroFlag || (leadingZero && f[i] != '0');
        const bool precision = i < f.size() && f[i] == '.';
        if (precision)
            for (++i; digit(i); ++i) {
            }
        if (i >= f.size()) {
            bad = "%";
            return false;
        }
        auto conv = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
        if (conv == 'x') conv = 'h';
        if (conv == 'm') continue;  // takes no argument (withScope)
        if (std::string_view("bohdcstefg").find(conv) == std::string_view::npos) {
            bad = std::string("format specifier %") + f[i];
            return false;
        }
        // The real formats take C's flags and a precision (3.6).
        if (std::string_view("efg").find(conv) == std::string_view::npos) {
            const std::string with = std::string(" with %") + f[i];
            for (const char c : flags)
                if (c != '-') {
                    bad = std::string("format flag '") + c + "'" + with;
                    return false;
                }
            if (precision) {
                bad = "format precision" + with;
                return false;
            }
            if (extra.empty() && flags.find('-') != std::string::npos) extra = "'-'" + with;
            if (extra.empty() && zeroFlag) extra = "'0'" + with;
        }
        convs.push_back(conv);
    }
    return true;
}

/// A format with each %m replaced by the hierarchical name of the scope
/// that calls the task (SYSTEM_TASKS.md 3.8).
std::string withScope(std::string_view f, std::string_view scope) {
    std::string out;
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%' || i + 1 >= f.size()) {
            out.push_back(f[i]);
            continue;
        }
        std::size_t j = i + 1;
        while (j < f.size() &&
               std::string_view("-+ #0123456789.").find(f[j]) != std::string_view::npos)
            ++j;
        if (j < f.size() && (f[j] == 'm' || f[j] == 'M')) {
            out += scope;
            i = j;
        } else {
            // %% and any other specifier stay as they are.
            const std::size_t end = j < f.size() ? j : f.size() - 1;
            out.append(f.substr(i, end - i + 1));
            i = end;
        }
    }
    return out;
}

/// Whether a call is the built-in method of a type of kind k (the string
/// methods of 6.16, the enumeration methods of 6.19.5), and not a system
/// task or function, whatever its first argument: slang gives the object
/// of a method as the first argument, and a system function may take a
/// string or an enumeration there too.
bool isBuiltinMethod(const ast::CallExpression& call, ast::SymbolKind k) {
    if (!call.isSystemCall()) return false;
    const auto& info = std::get<ast::CallExpression::SystemCallInfo>(call.subroutine);
    return info.subroutine.get() ==
           info.scope->getCompilation().getSystemMethod(k, call.getSubroutineName());
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
    return s.kind == ast::SymbolKind::Variable || s.kind == ast::SymbolKind::FormalArgument;
}

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
    std::vector<std::uint64_t> a((t.width + 63) / 64, 0);
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
            const double d = isReal(c.type)
                                 ? realValue(c)
                                 : integerToReal(c.aWords.empty() ? 0 : c.aWords[0],
                                                 c.bWords.empty() ? 0 : c.bWords[0], c.type);
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

Val Lowerer::lowerCondition(const ast::Expression& e) {
    Val v = lowerExpr(e);
    // 0.0 is false, anything else true (VALUE_SEMANTICS.md 7.5); a chandle
    // is false when null (7.8).
    if (isReal(v.type) || v.type.kind == sir::TypeKind::Chandle)
        return emit(Op::Truth, sir::u2(1), {v.op}, e.sourceRange.start());
    if (!sir::isIntegral(v.type)) unsupported(e.sourceRange.start(), "condition of this type");
    if (v.type.width == 1 && !sir::isSigned(v.type)) return v;
    return emit(Op::Truth, integral(sir::isFourState(v.type), false, 1), {v.op},
                e.sourceRange.start());
}

// --- Places ------------------------------------------------------------------

std::optional<ArrayAccess> arrayAccess(const ast::Expression& e) {
    ArrayAccess acc;
    const ast::Expression* cur = &e;
    while (cur->kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = cur->as<ast::ElementSelectExpression>();
        if (!es.value().type->isUnpackedArray()) break;
        acc.indices.push_back(&es.selector());
        cur = &es.value();
    }
    if (acc.indices.empty()) return std::nullopt;
    const ast::Symbol* sym = namedSymbol(*cur);
    // A parameter is read from its slot (SIR_SPEC.md 6).
    if (!Lowerer::isUnpackedMember(*cur) &&
        (sym == nullptr || (!isVariable(*sym) && sym->kind != ast::SymbolKind::Net &&
                            sym->kind != ast::SymbolKind::Parameter)))
        return std::nullopt;
    acc.base = cur;
    std::reverse(acc.indices.begin(), acc.indices.end());
    return acc;
}

std::optional<sir::SlotRange> Lowerer::eventSlot(const ast::Expression& e) {
    // A named event is waited on, not watched for changes (SCHEDULER_SPEC.md 9.5).
    if (e.type->isEvent()) return std::nullopt;
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
    const sir::Slot& slot = design().slots[p.slot];
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
    // The element of an event slot, with constant indices (SIR_SPEC.md 3.2.1).
    std::vector<std::int64_t> indices;
    const ast::Expression* base = &e;
    while (base->kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = base->as<ast::ElementSelectExpression>();
        const std::optional<std::int64_t> idx = constInt(es.selector());
        if (!idx) unsupported(sl, "event selected by an index that is not constant");
        indices.insert(indices.begin(), *idx);
        base = &es.value();
    }
    const ast::Symbol* sym = namedSymbol(*base);
    if (sym == nullptr || findFrameVar(*sym) || !inSlot(*sym))
        unsupported(sl, "event given by this expression");
    const sir::SlotId s = slotOf(sym->as<ast::ValueSymbol>(), sl);
    const sir::Slot& slot = design().slots[s];
    if (indices.size() != slot.dims.size()) unsupported(sl, "event given by this expression");
    for (std::size_t d = 0; d < indices.size(); ++d) {
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        if (indices[d] < std::min(l, r) || indices[d] > std::max(l, r))
            unsupported(sl, "event index outside the array");
    }
    Operand o = Operand::slot(s);
    if (!slot.dims.empty()) {
        o.firstElem = elementNumber(slot, indices);
        o.numElems = 1;
    }
    return o;
}

Val Lowerer::lowerTriggered(const ast::Expression& ev, Type rt, slang::SourceLocation sl) {
    // A trigger changes no slot, so an implicit sensitivity (always_comb,
    // always_latch, always @*, a continuous assignment, and the functions
    // they may call) would not see it.
    const bool implicit =
        b_.isSub
            ? design().subroutines[b_.proc].kind == sir::SubroutineKind::Function
            : proc().kind == sir::ProcessKind::Comb || proc().kind == sir::ProcessKind::Latch ||
                  proc().kind == sir::ProcessKind::CombAt || proc().kind == sir::ProcessKind::Cont;
    if (implicit) unsupported(sl, "triggered in a function or under an implicit sensitivity");
    const Operand o = eventOperand(ev, sl);
    const Val h = emit(Op::Load, Type{sir::TypeKind::Event, 0}, {o}, sl);
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
    if (constantValue(e)) return delayConstant(delayTicks(e, sl));
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

Val Lowerer::lowerExpr(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    if (!substitutions_.empty())
        if (const auto it = substitutions_.find(&e); it != substitutions_.end()) return it->second;
    // Assigning, comparing and printing events come with their handles (P3).
    if (e.type->isEvent()) unsupported(sl, "event used as a value");
    const bool pure = e.kind != ast::ExpressionKind::Assignment &&
                      e.kind != ast::ExpressionKind::Call && !sideEffects(e) &&
                      !untrustedConstant(e);
    if (pure && e.getConstant() != nullptr && (e.type->isIntegral() || e.type->isFloating()) &&
        (e.getConstant()->isInteger() || e.getConstant()->isReal() ||
         e.getConstant()->isShortReal()))
        return constantFrom(*e.getConstant(), typeOf(*e.type, sl), sl);

    switch (e.kind) {
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
            // are P3).
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
                const sir::FrameVar& fv = frameVars()[*f];
                if (!fv.dims.empty()) unsupported(sl, "an unpacked array as a value");
                // A ref argument reads what it refers to (SIR_SPEC.md 8).
                if (fv.type.kind == sir::TypeKind::Ref)
                    return emit(Op::LoadRef, fv.target, {Operand::frame(*f)}, sl);
                return emit(Op::Fload, fv.type, {Operand::frame(*f)}, sl);
            }
            if (!isVariable(sym) && sym.kind != ast::SymbolKind::Net)
                unsupported(sl, "reference to a " + std::string(ast::toString(sym.kind)));
            if (!inSlot(sym))
                unsupported(sl, "automatic variable '" + std::string(sym.name) + "' here");
            const sir::SlotId s = slotOf(sym.as<ast::ValueSymbol>(), sl);
            if (!design().slots[s].dims.empty()) unsupported(sl, "an unpacked array as a value");
            noteRead({s, 0, 0, 0, 0});
            return emit(Op::Load, design().slots[s].type, {Operand::slot(s)}, sl);
        }
        case ast::ExpressionKind::ElementSelect: return lowerElementSelect(e);
        case ast::ExpressionKind::RangeSelect: return lowerRangeSelect(e);
        case ast::ExpressionKind::SimpleAssignmentPattern:
        case ast::ExpressionKind::StructuredAssignmentPattern:
        case ast::ExpressionKind::ReplicatedAssignmentPattern: {
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
            const Val v = lowerExpr(m.value());
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
            if (c.conditions.size() != 1 || c.conditions[0].pattern != nullptr)
                unsupported(sl, "conditional operator with a pattern");
            const Type rt = typeOf(*e.type, sl);
            Val cond = lowerExpr(*c.conditions[0].expr);
            if (cond.type.width != 1 || sir::isSigned(cond.type))
                cond =
                    emit(Op::Truth, integral(sir::isFourState(cond.type), false, 1), {cond.op}, sl);
            if (sideEffects(c.left()) || sideEffects(c.right())) {
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
                const Val l1 = coerce(lowerExpr(c.left()), rt, sl);
                terminate(Op::Br, {Operand::block(join, 1), l1.op}, sl);
                setBlock(second);
                const Val r1 = coerce(lowerExpr(c.right()), rt, sl);
                terminate(Op::Br, {Operand::block(join, 1), r1.op}, sl);
                setBlock(both);
                const Val l2 = coerce(lowerExpr(c.left()), rt, sl);
                const Val r2 = coerce(lowerExpr(c.right()), rt, sl);
                const Val merged = emit(Op::Select, rt, {cond.op, l2.op, r2.op}, sl);
                terminate(Op::Br, {Operand::block(join, 1), merged.op}, sl);
                setBlock(join);
                return result;
            }
            const Val l = coerce(lowerExpr(c.left()), rt, sl);
            const Val r = coerce(lowerExpr(c.right()), rt, sl);
            return emit(Op::Select, rt, {cond.op, l.op, r.op}, sl);
        }
        case ast::ExpressionKind::Concatenation: {
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
            const auto& c = e.as<ast::ConversionExpression>();
            const Type want = typeOf(*e.type, sl);
            if (isString(want) && c.operand().type->isIntegral() && !sideEffects(c.operand())) {
                // A constant integral value as a string, string literals and
                // their concatenations among them: its bytes without the
                // zero ones (VALUE_SEMANTICS.md 7.6), at any width.
                if (const std::optional<slang::ConstantValue> cv = constantValue(c.operand());
                    cv && cv->isInteger()) {
                    if (const std::optional<std::string> text = stringBytes(cv->integer()))
                        return stringConstant(*text);
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
            Val v = lowerExpr(c.operand());
            // An operand of a context-determined expression takes the sign
            // of the expression before it is extended (11.8.2); other
            // conversions extend by the operand's own sign (11.8.3).
            if (c.conversionKind == ast::ConversionKind::Propagated && sir::isIntegral(v.type) &&
                sir::isIntegral(want))
                v = coerce(v, integral(sir::isFourState(v.type), sir::isSigned(want), v.type.width),
                           sl);
            return coerce(v, want, sl);
        }
        case ast::ExpressionKind::Assignment:
            return lowerAssignment(e.as<ast::AssignmentExpression>());
        case ast::ExpressionKind::LValueReference:
            // The target of the assignment operator being lowered.
            if (b_.lvalueRefs.empty()) unsupported(sl, "expression 'LValueReference'");
            return coerce(b_.lvalueRefs.back(), typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::Call: {
            const auto& call = e.as<ast::CallExpression>();
            const std::string name(call.getSubroutineName());
            if (!call.isSystemCall()) return lowerCall(call);
            if (name == "triggered" && call.arguments().size() == 1 &&
                call.arguments()[0]->type->isEvent())
                return lowerTriggered(*call.arguments()[0], typeOf(*e.type, sl), sl);
            if ((name == "$signed" || name == "$unsigned") && call.arguments().size() == 1)
                return coerce(lowerExpr(*call.arguments()[0]), typeOf(*e.type, sl), sl);
            if (name == "$time") {
                // $time is in the caller's time unit (SYSTEM_TASKS.md 5.1):
                // the VM divides the tick count by 10^(unit - precision).
                const int shift = timeScale().unit - precision();
                if (shift < 0 || shift > 19) unsupported(sl, "$time with this time unit");
                std::uint64_t div = 1;
                for (int i = 0; i < shift; ++i)
                    div *= 10;
                const Val d = constant(sir::u2(64), div);
                return emit(Op::Syscall, sir::u4(64), {Operand::str(name), d.op}, sl);
            }
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
                // (SYSTEM_TASKS.md 8); on dynamic data they come with it (P3).
                if (const std::optional<slang::ConstantValue> cv = constantValue(e))
                    return constantFrom(*cv, rt, sl);
                // $bits of an expression slang does not evaluate (a
                // hierarchical name) still comes from its fixed type alone;
                // the expression is not evaluated (20.6.2).
                const auto& c = e.as<ast::CallExpression>();
                if (name == "$bits" && c.arguments().size() == 1 &&
                    c.arguments()[0]->type->isFixedSize())
                    return constant(rt, c.arguments()[0]->type->getBitstreamWidth());
                unsupported(sl, "system function " + name + " on this argument");
            }
            if (name == "$countbits" || name == "$countones" || name == "$onehot" ||
                name == "$onehot0" || name == "$isunknown") {
                // 20.9: $countbits counts the bits of the expression that
                // have one of the values of the control bits (the LSB of
                // each); the others are defined by it (SYSTEM_TASKS.md 8).
                if (args.empty()) unsupported(sl, "system function " + name);
                const Val v = lowerExpr(*args[0]);
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
            if ((name == "sum" || name == "product" || name == "and" || name == "or" ||
                 name == "xor") &&
                !args.empty() && args[0]->type->getCanonicalType().isUnpackedArray())
                return lowerArrayReduction(call, name, rt, sl);
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
                lowerStore(t, emit(Op::Fload, frameVars()[f].type, {Operand::frame(f)}, sl), false,
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
                    lowerStore(*t, emit(Op::Fload, frameVars()[f].type, {Operand::frame(f)}, sl),
                               false, sl);
                return coerce(n, rt, sl);
            }
            if (name == "$fread" && args.size() == 2 &&
                args[0]->type->getCanonicalType().isIntegral()) {
                // $fread(var, fd): the bytes not read keep the target's value,
                // so the frame variable starts with it.
                const ast::Expression* t = outTarget(args[0]);
                const Type tt = typeOf(*t->type, sl);
                const sir::FrameVarId f = newFrameVar(tt);
                emitVoid(Op::Fstore, {Operand::frame(f), coerce(lowerExpr(*t), tt, sl).op}, sl);
                const Val n =
                    sys(sir::s2(32), {Operand::frame(f), lowerDescriptor(*args[1], sl).op});
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

/// The type a bit position is computed in from a run-time index, and a
/// constant of it. 64 bits for an index of 32 bits or fewer; for a wider
/// index, wide enough that the arithmetic never wraps (a declared bound
/// takes 32 bits and an element at most 2^24), so that an index outside
/// the range stays outside however large it is (VALUE_SEMANTICS.md 3.11).
Type Lowerer::positionType(Type index) {
    return integral(sir::isFourState(index), true, index.width <= 32 ? 64 : index.width + 32);
}

Val Lowerer::positionConstant(Type t, std::int64_t k) {
    const std::size_t words = (t.width + 63) / 64;
    std::vector<std::uint64_t> a(words, k < 0 ? ~std::uint64_t{0} : 0);
    a[0] = static_cast<std::uint64_t>(k);
    return constantWords(t, std::move(a), std::vector<std::uint64_t>(words, 0));
}

Val Lowerer::bitPosition(const ast::Expression& selector, const slang::ConstantRange& r,
                         std::uint32_t elemWidth) {
    const slang::SourceLocation sl = selector.sourceRange.start();
    const std::int64_t count =
        std::int64_t{r.left >= r.right ? r.left - r.right : r.right - r.left} + 1;
    if (const std::optional<std::int64_t> idx = constInt(selector)) {
        // A constant outside the range selects nothing: position `count *
        // elemWidth` is past the end, so the read gives x.
        const std::optional<std::int64_t> pos = positionOf(*idx, r);
        const std::int64_t p = pos ? *pos * elemWidth : count * elemWidth;
        return constant(sir::s2(64), static_cast<std::uint64_t>(p));
    }
    Val i = lowerExpr(selector);
    if (!sir::isIntegral(i.type)) unsupported(sl, "index of this type");
    if (r.left >= r.right && r.right == 0 && elemWidth == 1) return i;
    const Type pt = positionType(i.type);
    i = coerce(i, pt, sl);
    const Val right = positionConstant(pt, r.right);
    Val pos = r.left >= r.right ? emit(Op::Sub, pt, {i.op, right.op}, sl)
                                : emit(Op::Sub, pt, {right.op, i.op}, sl);
    if (elemWidth != 1) pos = emit(Op::Mul, pt, {pos.op, positionConstant(pt, elemWidth).op}, sl);
    return pos;
}

std::optional<std::uint32_t> Lowerer::constElementOf(const sir::Slot& slot,
                                                     const std::vector<std::int64_t>& indices) {
    // The element number of constant declared indices, or nothing when one
    // is outside its range.
    std::uint32_t elem = 0;
    if (indices.size() != slot.dims.size()) return std::nullopt;
    for (std::size_t d = 0; d < indices.size(); ++d) {
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        const auto n = static_cast<std::uint32_t>(std::abs(l - r) + 1);
        const std::int64_t off = l <= r ? indices[d] - l : l - indices[d];
        if (off < 0 || off >= n) return std::nullopt;
        elem = elem * n + static_cast<std::uint32_t>(off);
    }
    return elem;
}

Lowerer::Place Lowerer::arrayElementPlace(const ast::Expression& base,
                                          const std::vector<const ast::Expression*>& indices,
                                          slang::SourceLocation sl) {
    // The indices of the element of an array of structures come first, then
    // those written after the member (SIR_SPEC.md 4.2); all evaluated in
    // order. Constant indices make the sensitivity that element alone.
    const std::optional<Storage> st = storageOf(base, sl);
    if (!st) unsupported(sl, "selecting from this array");
    Place p;
    p.kind = Place::Kind::Element;
    p.slot = st->id;
    p.indices = st->elem;
    if (st->frame) {
        // An automatic array, or what an array ref argument refers to
        // (SIR_SPEC.md 7.4, 8).
        const bool ref = frameVars()[st->id].type.kind == sir::TypeKind::Ref;
        p.home = ref ? Place::Home::Ref : Place::Home::Frame;
        if (frameVars()[st->id].dims.size() != st->elem.size() + indices.size())
            unsupported(sl, "a part of an unpacked array");
        for (const ast::Expression* ix : indices) {
            const Val iv = lowerExpr(*ix);
            if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
            p.indices.push_back(iv.op);
        }
        p.type = homeType(p);
        return p;
    }
    if (design().slots[st->id].dims.size() != st->elem.size() + indices.size())
        unsupported(sl, "a part of an unpacked array");
    bool fixed = !st->dynamic;
    std::vector<std::int64_t> idx = st->constIdx;
    for (const ast::Expression* ix : indices) {
        const Val iv = lowerExpr(*ix);
        if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
        p.indices.push_back(iv.op);
        const std::optional<std::int64_t> c = constInt(*ix);
        if (c)
            idx.push_back(*c);
        else
            fixed = false;
    }
    // Looked up after the indices: the first call of a static function
    // among them adds its variables to the slot table.
    const sir::Slot& slot = design().slots[st->id];
    if (fixed) p.elem = constElementOf(slot, idx);
    p.type = slot.type;
    return p;
}

Val Lowerer::lowerElementSelect(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& es = e.as<ast::ElementSelectExpression>();
    const ast::Type& vt = es.value().type->getCanonicalType();
    if (vt.isUnpackedArray()) {
        if (structElement(*e.type) != nullptr)
            unsupported(sl, "unpacked structure used as a whole here");
        const std::optional<ArrayAccess> acc = arrayAccess(e);
        if (!acc) unsupported(sl, "selecting from this array");
        // An element of a parameter at constant indices is slang's constant
        // (SIR_SPEC.md 6).
        const ast::Symbol* base = namedSymbol(*acc->base);
        if (base != nullptr && base->kind == ast::SymbolKind::Parameter)
            if (const std::optional<slang::ConstantValue> cv = constantValue(e))
                return constantFrom(*cv, typeOf(*e.type, sl), sl);
        return coerce(loadPlace(arrayElementPlace(*acc->base, acc->indices, sl), sl),
                      typeOf(*e.type, sl), sl);
    }
    if (vt.isString()) {
        // s[i]: the byte, or 0 outside the string (table 6-9).
        const Val v = lowerExpr(es.value());
        const Val i = lowerExpr(es.selector());
        return coerce(emit(Op::StrGet, sir::u2(8), {v.op, i.op}, sl), typeOf(*e.type, sl), sl);
    }
    if (!vt.isIntegral()) unsupported(sl, "selecting from a value of this type");
    const Val v = lowerExpr(es.value());
    const std::uint32_t ew = e.type->getBitWidth();
    const Val pos = bitPosition(es.selector(), vt.getFixedRange(), ew);
    const bool four = sir::isFourState(v.type);
    const Val r = ew == 1 ? emit(Op::Bitsel, integral(four, false, 1), {v.op, pos.op}, sl)
                          : emit(Op::IdxPartsel, integral(four, false, ew), {v.op, pos.op}, sl);
    return coerce(r, typeOf(*e.type, sl), sl);
}

/// The position of the least significant bit of a range select, and its
/// width. Constant when the selection is.
Lowerer::Part Lowerer::partOf(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& rs = e.as<ast::RangeSelectExpression>();
    const ast::Type& vt = rs.value().type->getCanonicalType();
    // The bounds are indices of the dimension the select indexes, whose
    // elements may be several bits wide (7.4.1, 11.5.1): the element
    // number nearest the right bound times the element width gives the
    // lsb.
    const slang::ConstantRange r = vt.getFixedRange();
    const std::uint32_t width = e.type->getBitWidth();
    const std::int64_t ew = vt.getBitWidth() / r.width();
    const bool down = r.left >= r.right;
    if (rs.getSelectionKind() == ast::RangeSelectionKind::Simple) {
        const std::optional<std::int64_t> l = constInt(rs.left());
        const std::optional<std::int64_t> rr = constInt(rs.right());
        if (!l || !rr) unsupported(sl, "part-select with unknown bounds");
        // The element nearest the right bound of the declared range.
        const std::int64_t near = down ? std::min(*l, *rr) : std::max(*l, *rr);
        const std::int64_t lsb = (down ? near - r.right : r.right - near) * ew;
        return {std::nullopt, lsb, width};
    }
    const bool up = rs.getSelectionKind() == ast::RangeSelectionKind::IndexedUp;
    // Declared indices covered, n elements: [base, base + n - 1] going up,
    // [base - n + 1, base] going down. The one nearest the right bound
    // gives the lsb.
    const std::int64_t n = width / ew;
    if (const std::optional<std::int64_t> base = constInt(rs.left())) {
        const std::int64_t lo = up ? *base : *base - n + 1;
        const std::int64_t hi = up ? *base + n - 1 : *base;
        const std::int64_t lsb = (down ? lo - r.right : r.right - hi) * ew;
        return {std::nullopt, lsb, width};
    }
    Val b = lowerExpr(rs.left());
    if (!sir::isIntegral(b.type)) unsupported(sl, "part-select base of this type");
    const Type pt = positionType(b.type);
    b = coerce(b, pt, sl);
    // lsb = ((up ? base : base - n + 1) - right) * ew when the range goes
    // down, (right - (up ? base + n - 1 : base)) * ew when it goes up.
    std::int64_t k = 0;
    Val pos;
    if (down) {
        k = (up ? 0 : -(n - 1)) - r.right;
        pos = emit(Op::Add, pt, {b.op, positionConstant(pt, k).op}, sl);
    } else {
        k = r.right - (up ? n - 1 : 0);
        pos = emit(Op::Sub, pt, {positionConstant(pt, k).op, b.op}, sl);
    }
    if (ew != 1) pos = emit(Op::Mul, pt, {pos.op, positionConstant(pt, ew).op}, sl);
    return {pos, 0, width};
}

Val Lowerer::lowerRangeSelect(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& rs = e.as<ast::RangeSelectExpression>();
    const ast::Type& vt = rs.value().type->getCanonicalType();
    if (!vt.isIntegral()) unsupported(sl, "part-select of a value of this type");
    const Val v = lowerExpr(rs.value());
    const Part p = partOf(e);
    const bool four = sir::isFourState(v.type);
    const Type rt = integral(four, false, p.width);
    Val r;
    if (p.pos) {
        r = emit(Op::IdxPartsel, rt, {v.op, p.pos->op}, sl);
    } else if (p.lsb >= 0 && p.lsb + p.width <= v.type.width) {
        r = emit(Op::Partsel, rt,
                 {v.op, constant(sir::u2(32), static_cast<std::uint64_t>(p.lsb)).op}, sl);
    } else {
        r = emit(Op::IdxPartsel, rt,
                 {v.op, constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb)).op}, sl);
    }
    return coerce(r, typeOf(*e.type, sl), sl);
}

Val Lowerer::lowerUnary(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& u = e.as<ast::UnaryExpression>();
    const Type rt = typeOf(*e.type, sl);
    switch (u.op) {
        case ast::UnaryOperator::Plus: {
            // An arithmetic operator: x or z in any bit makes every bit x
            // (VALUE_SEMANTICS.md 3.2), which adding 0 does.
            Val v = coerce(lowerExpr(u.operand()), rt, sl);
            if (!sir::isFourState(rt)) return v;
            return emit(Op::Add, rt, {v.op, constant(rt, 0).op}, sl);
        }
        case ast::UnaryOperator::Minus:
            return emit(Op::Neg, rt, {coerce(lowerExpr(u.operand()), rt, sl).op}, sl);
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
                (isReal(v.type) || v.type.kind == sir::TypeKind::Chandle))
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

Val Lowerer::lowerStream(const ast::StreamingConcatenationExpression& s, slang::SourceLocation sl) {
    // The bits of the operands from the left (11.4.14.1); four-state if any
    // operand is. Then, for <<, the blocks of the slice size from the right
    // in reverse order (11.4.14.2).
    std::vector<Operand> ops;
    std::uint32_t width = 0;
    bool four = false;
    for (const auto& se : s.streams()) {
        if (se.withExpr != nullptr) unsupported(sl, "streaming with a with clause");
        const ast::Expression& op = *se.operand;
        Val v;
        if (op.kind == ast::ExpressionKind::Streaming)
            v = lowerStream(op.as<ast::StreamingConcatenationExpression>(), sl);
        else
            v = bitstreamOf(op, sl);
        ops.push_back(v.op);
        width += v.type.width;
        four = four || sir::isFourState(v.type);
    }
    if (width == 0) unsupported(sl, "a value of no bits");
    Val stream = emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
    if (s.getSliceSize() == 0) return stream;
    return reverseBlocks(stream, s.getSliceSize(), true, sl);
}

Val Lowerer::reverseBlocks(const Val& v, std::uint64_t size, bool fromRight,
                           slang::SourceLocation sl) {
    // Blocks of `size` bits, sliced from the right end (packing) or from
    // the left end (unpacking, its inverse); the last block may be short.
    // The result has them in the reverse order.
    const std::uint32_t w = v.type.width;
    if (size >= w) return v;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> blocks;  // lsb, width
    if (fromRight) {
        for (std::uint32_t lsb = 0; lsb < w; lsb += static_cast<std::uint32_t>(size))
            blocks.emplace_back(lsb,
                                static_cast<std::uint32_t>(std::min<std::uint64_t>(size, w - lsb)));
    } else {
        for (std::uint32_t msb = w; msb > 0;) {
            const auto bw = static_cast<std::uint32_t>(std::min<std::uint64_t>(size, msb));
            msb -= bw;
            blocks.emplace_back(msb, bw);
        }
        std::reverse(blocks.begin(), blocks.end());
    }
    // From the right: the lowest block becomes the most significant; from
    // the left, the reversed list puts the last block first.
    std::vector<Operand> parts;
    parts.reserve(blocks.size());
    for (const auto& [lsb, bw] : blocks)
        parts.push_back(emit(Op::Partsel, integral(sir::isFourState(v.type), false, bw),
                             {v.op, constant(sir::u2(32), lsb).op}, sl)
                            .op);
    return emit(Op::Concat, integral(sir::isFourState(v.type), false, w), std::move(parts), sl);
}

void Lowerer::lowerUnpack(const ast::StreamingConcatenationExpression& s,
                          const ast::Expression& rhs, bool nonblocking, slang::SourceLocation sl) {
    // The source's leftmost bits, as many as the targets take; for << the
    // inverse reordering; then the targets from the left like a
    // concatenation (11.4.14.3). The value comes before the targets'
    // indices, as in any assignment.
    Val src = bitstreamOf(rhs, sl);
    const std::uint64_t w = s.getBitstreamWidth();
    if (w == 0 || w > 64 || src.type.width < w) unsupported(sl, "unpacking this stream");
    if (src.type.width > w)
        src = emit(Op::Partsel,
                   integral(sir::isFourState(src.type), false, static_cast<std::uint32_t>(w)),
                   {src.op, constant(sir::u2(32), src.type.width - w).op}, sl);
    else
        src = coerce(src, integral(sir::isFourState(src.type), false, src.type.width), sl);
    // <<: the inverse of packing, also with a short last block
    // (VALUE_SEMANTICS.md 8).
    if (s.getSliceSize() != 0) src = reverseBlocks(src, s.getSliceSize(), false, sl);
    std::vector<Place> places;
    for (const auto& se : s.streams()) {
        if (se.withExpr != nullptr) unsupported(sl, "streaming with a with clause");
        const ast::Expression& op = *se.operand;
        if (op.kind == ast::ExpressionKind::Streaming)
            unsupported(sl, "unpacking into this target");
        bitstreamPlaces(op, places, sl);
    }
    Place all = concatPlace(std::move(places), src.type, sl);
    storePlace(all, src, nonblocking, sl);
}

Val Lowerer::bitstreamOf(const ast::Expression& e, slang::SourceLocation sl) {
    // An integral value as its bits; an unpacked array or structure as the
    // concatenation of its single values in order (11.4.14.1,
    // VALUE_SEMANTICS.md 8).
    std::vector<Val> vals;
    if (isAggregate(*e.type))
        aggregateValues(e, *e.type, vals, sl);
    else
        vals.push_back(lowerExpr(e));
    std::vector<Operand> ops;
    std::uint32_t width = 0;
    bool four = false;
    for (Val& v : vals) {
        if (!sir::isIntegral(v.type)) unsupported(sl, "streaming of a value of this type");
        v = coerce(v, integral(sir::isFourState(v.type), false, v.type.width), sl);
        ops.push_back(v.op);
        width += v.type.width;
        four = four || sir::isFourState(v.type);
    }
    if (width == 0) unsupported(sl, "a value of no bits");
    if (ops.size() == 1) return vals[0];
    return emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
}

void Lowerer::bitstreamPlaces(const ast::Expression& e, std::vector<Place>& out,
                              slang::SourceLocation sl) {
    // The single places of a target, in the order of bitstreamOf.
    if (structElement(*e.type) != nullptr) {
        const std::optional<Storage> st = storageOf(e, sl);
        if (!st) unsupported(sl, "unpacking into this target");
        storagePlaces(*st, *e.type, out, sl);
        return;
    }
    if (e.type->getCanonicalType().isUnpackedArray()) {
        const std::optional<ArrayTarget> a = arrayTarget(e);
        if (!a) unsupported(sl, "unpacking into this array");
        arrayPlaces(baseOf(*a), a->indices, out);
        return;
    }
    if (!e.type->isIntegral()) unsupported(sl, "unpacking into this target");
    out.push_back(placeOf(e, sl));
}

void Lowerer::arrayPlaces(ArrayBase s, std::vector<std::int64_t> indices, std::vector<Place>& out) {
    const std::vector<sir::ArrayDim>& dims = dimsOf(s);
    if (indices.size() == dims.size()) {
        out.push_back(elementPlace(s, indices));
        return;
    }
    const sir::ArrayDim d = dims[indices.size()];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    for (std::int64_t i = d.left;; i += step) {
        indices.push_back(i);
        arrayPlaces(s, indices, out);
        indices.pop_back();
        if (i == d.right) break;
    }
}

Lowerer::Place Lowerer::concatPlace(std::vector<Place> parts, Type like, slang::SourceLocation sl) {
    Place all;
    all.kind = Place::Kind::Concat;
    for (Place& part : parts) {
        if (!sir::isIntegral(part.type)) unsupported(sl, "unpacking into a target of this type");
        all.width += part.type.width;
        all.base.push_back(std::move(part));
    }
    if (all.width == 0) unsupported(sl, "a value of no bits");
    all.type = integral(sir::isFourState(like), false, all.width);
    return all;
}

Val Lowerer::lowerArrayReduction(const ast::CallExpression& call, const std::string& name, Type rt,
                                 slang::SourceLocation sl) {
    // sum, product, and, or and xor of the elements, or of the with
    // clause's value for each element, in the result's type (7.12.3); the
    // elements in the order foreach visits them.
    const std::optional<ArrayTarget> a = arrayTarget(*call.arguments()[0]);
    if (!a) unsupported(sl, "array method " + name + " on this array");
    if (!sir::isIntegral(rt) && !(isReal(rt) && (name == "sum" || name == "product")))
        unsupported(sl, "array method " + name + " of this type");
    // Each item is an element of the first remaining dimension; with more
    // than one left, an item would be an array.
    if (dimsOf(baseOf(*a)).size() != a->indices.size() + 1)
        unsupported(sl, "array method " + name + " on this array");
    const auto [with, item] = std::get<1>(call.subroutine).getIteratorInfo();
    const Op op = name == "sum"       ? Op::Add
                  : name == "product" ? Op::Mul
                  : name == "and"     ? Op::And
                  : name == "or"      ? Op::Or
                                      : Op::Xor;
    std::optional<Val> acc;
    std::vector<std::int64_t> idx = a->indices;
    const sir::ArrayDim d = dimsOf(baseOf(*a)).back();
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    for (std::int64_t i = d.left;; i += step) {
        idx.push_back(i);
        Val v = loadPlace(elementPlace(baseOf(*a), idx), sl);
        idx.pop_back();
        if (with != nullptr) {
            iterators_[item] = v;
            v = lowerExpr(*with);
            iterators_.erase(item);
        }
        v = coerce(v, rt, sl);
        acc = acc ? emit(op, rt, {acc->op, v.op}, sl) : v;
        if (i == d.right) break;
    }
    return *acc;
}

bool Lowerer::isAggregate(const ast::Type& t) {
    return unpackedStruct(t) != nullptr || t.getCanonicalType().isUnpackedArray();
}

void Lowerer::aggregateValues(const ast::Expression& e, const ast::Type& t, std::vector<Val>& out,
                              slang::SourceLocation sl) {
    // The values of an unpacked array or structure, single values in order:
    // array elements from the left bound, structure members in declaration
    // order, nested ones in turn (VALUE_SEMANTICS.md 3.14).
    const ast::Expression* r = &e;
    while (r->kind == ast::ExpressionKind::Conversion &&
           isAggregate(*r->as<ast::ConversionExpression>().operand().type))
        r = &r->as<ast::ConversionExpression>().operand();
    const ast::Type& ct = t.getCanonicalType();
    const ast::UnpackedStructType* st = unpackedStruct(ct);
    if (isPattern(*r)) {
        const std::vector<const ast::Expression*> elems = patternElements(*r);
        if (st != nullptr) {
            if (elems.size() != st->fields.size()) unsupported(sl, "pattern of this size");
            for (std::size_t i = 0; i < elems.size(); ++i)
                aggregateValues(*elems[i], st->fields[i]->getType(), out, sl);
            return;
        }
        if (ct.kind != ast::SymbolKind::FixedSizeUnpackedArrayType)
            unsupported(sl, "comparison of this array");
        const auto& at = ct.as<ast::FixedSizeUnpackedArrayType>();
        if (elems.size() != at.range.width()) unsupported(sl, "pattern of this size");
        for (const ast::Expression* el : elems)
            aggregateValues(*el, at.elementType, out, sl);
        return;
    }
    if (!isAggregate(ct)) {
        out.push_back(coerce(lowerExpr(*r), typeOf(ct, sl), sl));
        return;
    }
    if (structElement(ct) != nullptr) {
        const std::optional<Storage> src = storageOf(*r, sl);
        if (!src || !isAggregate(*r->type)) unsupported(sl, "comparison of this structure value");
        storageValues(*src, ct, out, sl);
        return;
    }
    const std::optional<ArrayTarget> a = arrayTarget(*r);
    if (!a) unsupported(sl, "comparison of this array value");
    arrayValues(baseOf(*a), a->indices, out, sl);
}

void Lowerer::arrayValues(ArrayBase s, std::vector<std::int64_t> indices, std::vector<Val>& out,
                          slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim>& dims = dimsOf(s);
    if (indices.size() == dims.size()) {
        const Place p = elementPlace(s, indices);
        out.push_back(loadPlace(p, sl));
        return;
    }
    const sir::ArrayDim d = dims[indices.size()];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    for (std::int64_t i = d.left;; i += step) {
        indices.push_back(i);
        arrayValues(s, indices, out, sl);
        indices.pop_back();
        if (i == d.right) break;
    }
}

Val Lowerer::compareAggregates(const ast::BinaryExpression& b, slang::SourceLocation sl) {
    // == is 0 if a pair is 0, else x if a pair is x, else 1: the AND of
    // the pairs' ==; === the AND of their ===; != and !== the negation
    // (VALUE_SEMANTICS.md 3.14). Both sides are evaluated, left first.
    using BO = ast::BinaryOperator;
    const bool caseEq = b.op == BO::CaseEquality || b.op == BO::CaseInequality;
    std::vector<Val> l;
    std::vector<Val> r;
    aggregateValues(b.left(), *b.left().type, l, sl);
    aggregateValues(b.right(), *b.left().type, r, sl);
    if (l.size() != r.size() || l.empty()) unsupported(sl, "comparison of these aggregates");
    const auto pair = [&](std::size_t i) {
        // Reals and strings have no x or z: their === is ==.
        const bool bits = sir::isIntegral(l[i].type) && sir::isIntegral(r[i].type);
        return compareValues(caseEq && bits ? Op::Ceq : Op::Eq, l[i], r[i], sl);
    };
    Val all = pair(0);
    for (std::size_t i = 1; i < l.size(); ++i)
        all = bitOp(Op::And, all, pair(i), sl);
    if (b.op == BO::Inequality || b.op == BO::CaseInequality)
        all = emit(Op::Not, all.type, {all.op}, sl);
    return all;
}

namespace {

/// The conversions of a $sscanf format that assign, in order, as letters
/// (%x is h; %e and %g are f); false with the specifier in `bad` for one
/// that is not supported (SIR_SPEC.md 8, SYSTEM_TASKS.md 6.3).
bool scanConversions(std::string_view f, std::vector<char>& convs, std::string& bad) {
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%') continue;
        bool assigns = true;
        ++i;
        if (i < f.size() && f[i] == '*') {
            assigns = false;
            ++i;
        }
        while (i < f.size() && f[i] >= '0' && f[i] <= '9')
            ++i;
        if (i >= f.size()) {
            bad = "%";
            return false;
        }
        auto c = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
        if (c == '%') continue;
        if (c == 'x') c = 'h';
        if (c == 'e' || c == 'g') c = 'f';
        if (std::string_view("bohdcsf").find(c) == std::string_view::npos) {
            bad = std::string("%") + f[i];
            return false;
        }
        if (assigns) convs.push_back(c);
    }
    return true;
}

}  // namespace

Val Lowerer::lowerScanCall(const ast::CallExpression& call, Type rt, slang::SourceLocation sl) {
    // $sscanf(input, format, targets...), $fscanf(fd, format, targets...)
    // and $value$plusargs(format, target): each target that a conversion
    // assigns gets a frame variable
    // of its type, which the syscall writes; the targets are assigned
    // after it, as many as it reports (SIR_SPEC.md 8).
    const std::string name(call.getSubroutineName());
    const bool plus = name == "$value$plusargs";
    const auto args = call.arguments();
    const std::size_t fmtArg = plus ? 0 : 1;
    if (args.size() <= fmtArg) unsupported(sl, name + " without a format");
    const std::optional<slang::ConstantValue> fc = constantValue(*args[fmtArg]);
    std::optional<std::string> fmt;
    if (fc && fc->isString()) fmt = fc->str();
    if (fc && fc->isInteger()) fmt = stringBytes(fc->integer());
    if (!fmt) unsupported(sl, name + " with a format that is not a constant");
    std::vector<char> convs;
    std::string bad;
    if (plus) {
        // name%conv: the one conversion after the name (21.6).
        const std::size_t pct = fmt->find('%');
        if (pct == std::string::npos || !scanConversions(fmt->substr(pct), convs, bad) ||
            convs.size() != 1)
            unsupported(sl, name + " with this format");
    } else if (!scanConversions(*fmt, convs, bad)) {
        unsupported(sl, "format specifier " + bad + " in " + name);
    }
    const std::span<const ast::Expression* const> targets = args.subspan(fmtArg + 1);
    // Fewer targets than conversions: an error (SYSTEM_TASKS.md 6.3); more
    // are ignored (21.3.4.3).
    if (targets.size() < convs.size())
        error(sl, name + " has fewer arguments than its format has conversions");
    std::vector<Operand> ops{Operand::str(name)};
    if (name == "$fscanf") {
        ops.push_back(lowerDescriptor(*args[0], sl).op);
    } else if (!plus) {
        // A literal input of any length as a string (VALUE_SEMANTICS.md 7.6).
        const ast::Expression& ie = args[0]->unwrapImplicitConversions();
        std::string text;
        if (ie.kind == ast::ExpressionKind::StringLiteral) {
            text = ie.as<ast::StringLiteral>().getValue();
            std::erase(text, '\0');
        }
        Val in = ie.kind == ast::ExpressionKind::StringLiteral ? stringConstant(text)
                                                               : lowerExpr(*args[0]);
        if (!isString(in.type) && !sir::isIntegral(in.type))
            unsupported(sl, name + " of an input of this type");
        ops.push_back(in.op);
    }
    ops.push_back(Operand::str(*fmt));
    std::vector<std::pair<const ast::Expression*, sir::FrameVarId>> outs;
    for (std::size_t i = 0; i < convs.size(); ++i) {
        const ast::Expression* t = targets[i];
        if (t->kind == ast::ExpressionKind::Assignment &&
            t->as<ast::AssignmentExpression>().isLValueArg())
            t = &t->as<ast::AssignmentExpression>().left();
        const ast::Type& tt = t->type->getCanonicalType();
        const char c = convs[i];
        const bool ok = c == 'f'   ? tt.isFloating()
                        : c == 's' ? tt.isString() || tt.isIntegral()
                                   : tt.isIntegral();
        if (!ok) unsupported(sl, std::string("%") + c + " into a target of this type");
        const sir::FrameVarId f = newFrameVar(typeOf(tt, sl));
        ops.push_back(Operand::frame(f));
        outs.emplace_back(t, f);
    }
    const Val n = emit(Op::Syscall, sir::s2(32), std::move(ops), sl);
    // Target i is assigned when the count is above i.
    for (std::size_t i = 0; i < outs.size(); ++i) {
        const Val above = emit(Op::Gt, sir::u2(1),
                               {n.op, constant(sir::s2(32), static_cast<std::uint64_t>(i)).op}, sl);
        const sir::BlockId store = newBlock();
        const sir::BlockId next = newBlock();
        terminate(Op::BrIf, {above.op, Operand::block(store), Operand::block(next)}, sl);
        setBlock(store);
        const Val v =
            emit(Op::Fload, frameVars()[outs[i].second].type, {Operand::frame(outs[i].second)}, sl);
        lowerStore(*outs[i].first, v, false, sl);
        terminate(Op::Br, {Operand::block(next)}, sl);
        setBlock(next);
    }
    return coerce(n, rt, sl);
}

Val Lowerer::lowerFreadMemory(const ast::CallExpression& call, Type rt, slang::SourceLocation sl) {
    // $fread(mem, fd[, start[, count]]): from start (the lowest address by
    // default) toward the highest, at most count elements, each read as
    // $fread of an integral variable reads it, until one is not read whole
    // (SYSTEM_TASKS.md 6.3, 21.3.4.4). The count of bytes read is returned.
    const auto args = call.arguments();
    const ast::Expression* m = args[0];
    if (m->kind == ast::ExpressionKind::Assignment &&
        m->as<ast::AssignmentExpression>().isLValueArg())
        m = &m->as<ast::AssignmentExpression>().left();
    const std::optional<ArrayTarget> a = arrayTarget(*m);
    if (!a || dimsOf(baseOf(*a)).size() != a->indices.size() + 1)
        unsupported(sl, "$fread into a memory of this kind");
    const Type et = elemTypeOf(baseOf(*a));
    if (!sir::isIntegral(et)) unsupported(sl, "$fread into a memory of this element type");
    const sir::ArrayDim d = dimsOf(baseOf(*a)).back();
    const std::int64_t lo = std::min(d.left, d.right);
    const std::int64_t hi = std::max(d.left, d.right);
    const Type i64 = sir::s2(64);
    const auto given = [&](std::size_t i) {
        return i < args.size() && args[i]->kind != ast::ExpressionKind::EmptyArgument;
    };
    const Val desc = lowerDescriptor(*args[1], sl);
    const Val start = given(2) ? coerce(lowerExpr(*args[2]), i64, sl)
                               : constant(i64, static_cast<std::uint64_t>(lo));
    const Val count = given(3) ? coerce(lowerExpr(*args[3]), i64, sl)
                               : constant(i64, static_cast<std::uint64_t>(hi - lo + 1));
    const sir::FrameVarId addr = newFrameVar(i64);
    const sir::FrameVarId left = newFrameVar(i64);
    const sir::FrameVarId total = newFrameVar(sir::s2(32));
    const sir::FrameVarId elem = newFrameVar(et);
    emitVoid(Op::Fstore, {Operand::frame(addr), start.op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(left), count.op}, sl);
    emitVoid(Op::Fstore, {Operand::frame(total), constant(sir::s2(32), 0).op}, sl);
    const sir::BlockId head = newBlock();
    const sir::BlockId body = newBlock();
    const sir::BlockId store = newBlock();
    const sir::BlockId after = newBlock();
    const sir::BlockId done = newBlock();
    terminate(Op::Br, {Operand::block(head)}, sl);
    // While the address is in the memory and the count is not used up.
    setBlock(head);
    const Val at = emit(Op::Fload, i64, {Operand::frame(addr)}, sl);
    const Val inMem = emit(
        Op::And, sir::u2(1),
        {emit(Op::Ge, sir::u2(1), {at.op, constant(i64, static_cast<std::uint64_t>(lo)).op}, sl).op,
         emit(Op::Le, sir::u2(1), {at.op, constant(i64, static_cast<std::uint64_t>(hi)).op}, sl)
             .op},
        sl);
    const Val more =
        emit(Op::And, sir::u2(1),
             {inMem.op,
              emit(Op::Gt, sir::u2(1),
                   {emit(Op::Fload, i64, {Operand::frame(left)}, sl).op, constant(i64, 0).op}, sl)
                  .op},
             sl);
    terminate(Op::BrIf, {more.op, Operand::block(body), Operand::block(done)}, sl);
    // The element starts with its value, which the bytes not read keep.
    setBlock(body);
    std::vector<std::int64_t> idx = a->indices;
    idx.push_back(lo);
    Place p = elementPlace(baseOf(*a), idx);
    p.indices.back() = emit(Op::Fload, i64, {Operand::frame(addr)}, sl).op;
    p.elem.reset();
    emitVoid(Op::Fstore, {Operand::frame(elem), loadPlace(p, sl).op}, sl);
    const Val n =
        emit(Op::Syscall, sir::s2(32), {Operand::str("$fread"), Operand::frame(elem), desc.op}, sl);
    emitVoid(Op::Fstore,
             {Operand::frame(total),
              emit(Op::Add, sir::s2(32),
                   {emit(Op::Fload, sir::s2(32), {Operand::frame(total)}, sl).op, n.op}, sl)
                  .op},
             sl);
    terminate(Op::BrIf,
              {emit(Op::Gt, sir::u2(1), {n.op, constant(sir::s2(32), 0).op}, sl).op,
               Operand::block(store), Operand::block(done)},
              sl);
    setBlock(store);
    storePlace(p, emit(Op::Fload, et, {Operand::frame(elem)}, sl), false, sl);
    // An element not read whole ends the reading.
    const Val whole =
        emit(Op::Eq, sir::u2(1), {n.op, constant(sir::s2(32), (et.width + 7) / 8).op}, sl);
    terminate(Op::BrIf, {whole.op, Operand::block(after), Operand::block(done)}, sl);
    setBlock(after);
    emitVoid(Op::Fstore,
             {Operand::frame(addr),
              emit(Op::Add, i64,
                   {emit(Op::Fload, i64, {Operand::frame(addr)}, sl).op, constant(i64, 1).op}, sl)
                  .op},
             sl);
    emitVoid(Op::Fstore,
             {Operand::frame(left),
              emit(Op::Sub, i64,
                   {emit(Op::Fload, i64, {Operand::frame(left)}, sl).op, constant(i64, 1).op}, sl)
                  .op},
             sl);
    terminate(Op::Br, {Operand::block(head)}, sl);
    setBlock(done);
    return coerce(emit(Op::Fload, sir::s2(32), {Operand::frame(total)}, sl), rt, sl);
}

Val Lowerer::compareValues(Op op, Val l, Val r, slang::SourceLocation sl) {
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

Val Lowerer::insideMatch(const Val& v, const ast::Expression& e, slang::SourceLocation sl) {
    if (e.kind == ast::ExpressionKind::ValueRange) {
        // [lo:hi] by >= and <=; a $ bound always holds (11.4.13).
        const auto& vr = e.as<ast::ValueRangeExpression>();
        if (vr.rangeKind != ast::ValueRangeKind::Simple) unsupported(sl, "tolerance range");
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
    if (e.type->getCanonicalType().isUnpackedArray()) {
        // Every element, down to single values (11.4.13).
        const std::optional<ArrayTarget> a = arrayTarget(e);
        if (!a) unsupported(sl, "set member of this array");
        return arrayMatch(v, baseOf(*a), a->indices, sl);
    }
    // ==? for integral values, so that x and z in the member match any
    // bit; == for others (11.4.13).
    const Val m = lowerExpr(e);
    const bool integralPair = sir::isIntegral(v.type) && sir::isIntegral(m.type);
    return compareValues(integralPair ? Op::Weq : Op::Eq, v, m, sl);
}

Val Lowerer::arrayMatch(const Val& v, ArrayBase s, std::vector<std::int64_t> indices,
                        slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim>& dims = dimsOf(s);
    if (indices.size() == dims.size()) {
        const Val m = loadPlace(elementPlace(s, indices), sl);
        const bool integralPair = sir::isIntegral(v.type) && sir::isIntegral(m.type);
        return compareValues(integralPair ? Op::Weq : Op::Eq, v, m, sl);
    }
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

Val Lowerer::lowerBinary(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& b = e.as<ast::BinaryExpression>();
    const Type rt = typeOf(*e.type, sl);
    using BO = ast::BinaryOperator;
    if ((b.op == BO::Equality || b.op == BO::Inequality || b.op == BO::CaseEquality ||
         b.op == BO::CaseInequality) &&
        isAggregate(*b.left().type))
        return coerce(compareAggregates(b, sl), rt, sl);
    const auto same = [&](Op op) {
        const Val l = coerce(lowerExpr(b.left()), rt, sl);
        const Val r = coerce(lowerExpr(b.right()), rt, sl);
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
        if (sideEffects(b.right()) && op != Op::Leqv) {
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
            const Val l = coerce(lowerExpr(b.left()), rt, sl);
            Val r = lowerExpr(b.right());
            if (isReal(rt)) r = coerce(r, rt, sl);  // a real power takes a real exponent
            if (!isReal(rt) && !sir::isIntegral(r.type)) unsupported(sl, "** with this exponent");
            return emit(Op::Pow, rt, {l.op, r.op}, sl);
        }
    }
    unsupported(sl, "binary operator");
}

// --- Assignments -------------------------------------------------------------

Val Lowerer::lowerDelayedAssignment(const ast::AssignmentExpression& a) {
    // a = #d b and a = @(e) b compute b first and assign it when the delay
    // or event comes, the target's indices then; a <= #d b decides both
    // now and updates in the NBA region d later (SCHEDULER_SPEC.md 8, 13;
    // 9.4.5). The value lives in a frame variable across the wait.
    const slang::SourceLocation sl = a.sourceRange.start();
    const ast::TimingControl& t = *a.timingControl;
    if (a.isCompound() || unpackedStruct(*a.left().type) != nullptr ||
        a.left().type->getCanonicalType().isUnpackedArray() ||
        a.left().kind == ast::ExpressionKind::Streaming)
        unsupported(sl, "intra-assignment timing control on this assignment");
    const Type lt = typeOf(*a.left().type, sl);
    Val v = coerce(lowerExpr(a.right()), lt, sl);
    if (a.isNonBlocking()) {
        if (t.kind != ast::TimingControlKind::Delay)
            unsupported(sl, "nonblocking assignment with an event control");
        const Val ticks = delayValue(t.as<ast::DelayControl>().expr, sl);
        // The target is a fixed range of a slot: a whole variable, an
        // element or bits at constant positions (SIR_SPEC.md 5.3).
        const Place p = placeOf(a.left(), sl);
        const sir::Slot& slot = design().slots[p.slot];
        const bool fixedElem = p.kind == Place::Kind::Element ? p.elem.has_value()
                               : p.kind == Place::Kind::Bits  ? p.indices.empty() || p.elem
                                                              : p.kind == Place::Kind::Slot;
        const bool fixedBits = p.kind != Place::Kind::Bits ||
                               (!p.pos && p.lsb >= 0 && p.lsb + p.width <= slot.type.width);
        if (!fixedElem || !fixedBits || (p.kind == Place::Kind::Slot && !slot.dims.empty()))
            unsupported(sl, "delayed nonblocking assignment to this target");
        const sir::SlotRange r = rangeOf(p);
        Operand o = Operand::slot(r.slot, r.lsb, r.width);
        o.firstElem = r.firstElem;
        o.numElems = r.numElems;
        emitVoid(Op::NbaDelay, {o, coerce(v, p.type, sl).op, ticks.op}, sl);
        noteWrite(r);
        return v;
    }
    const sir::FrameVarId f = newFrameVar(lt);
    emitVoid(Op::Fstore, {Operand::frame(f), v.op}, sl);
    lowerTiming(t);
    const Val later = emit(Op::Fload, lt, {Operand::frame(f)}, sl);
    lowerStore(a.left(), later, false, sl);
    return later;
}

Val Lowerer::lowerAssignment(const ast::AssignmentExpression& a) {
    const slang::SourceLocation sl = a.sourceRange.start();
    if (a.timingControl != nullptr) return lowerDelayedAssignment(a);
    if (a.left().kind == ast::ExpressionKind::Streaming) {
        lowerUnpack(a.left().as<ast::StreamingConcatenationExpression>(), a.right(),
                    a.isNonBlocking(), sl);
        return Val{Operand::temp(0), sir::voidType()};
    }
    if (a.isCompound()) {
        // a op= b is a = a op (b) with the indices of a evaluated once
        // (11.4.1): the target's indices, its value, then b. slang gives
        // the right side as `<lvalue> op b`, the lvalue as an
        // LValueReference.
        const Place p = placeOf(a.left(), sl);
        b_.lvalueRefs.push_back(loadPlace(p, sl));
        const Val v = coerce(lowerExpr(a.right()), typeOf(*a.left().type, sl), sl);
        b_.lvalueRefs.pop_back();
        storePlace(p, v, a.isNonBlocking(), sl);
        return v;
    }
    if (isAggregate(*a.left().type)) {
        // A stream into an unpacked array or structure: left-aligned, zeros
        // on the right, then its bits from the left into the single places
        // in order (11.4.14).
        const ast::Expression* r = &a.right();
        if (r->kind == ast::ExpressionKind::Conversion &&
            r->as<ast::ConversionExpression>().conversionKind ==
                ast::ConversionKind::StreamingConcat)
            r = &r->as<ast::ConversionExpression>().operand();
        if (r->kind == ast::ExpressionKind::Streaming) {
            Val st = lowerStream(r->as<ast::StreamingConcatenationExpression>(), sl);
            std::vector<Place> places;
            bitstreamPlaces(a.left(), places, sl);
            const Place all = concatPlace(std::move(places), st.type, sl);
            if (st.type.width > all.width) unsupported(sl, "stream wider than its target");
            if (st.type.width < all.width)
                st = emit(Op::Concat, integral(sir::isFourState(st.type), false, all.width),
                          {st.op, constant(sir::u2(all.width - st.type.width), 0).op}, sl);
            storePlace(all, st, a.isNonBlocking(), sl);
            return Val{Operand::temp(0), sir::voidType()};
        }
    }
    if (structElement(*a.left().type) != nullptr) {
        // An unpacked structure, or an array of them, is assigned member by
        // member and element by element (7.2.2, 10.9.2, SIR_SPEC.md 4.2).
        const std::optional<Storage> dst = storageOf(a.left(), sl);
        if (!dst) unsupported(sl, "assignment to this unpacked structure");
        Stores stores;
        assignAggregate(*dst, *a.left().type, a.right(), stores, sl);
        flushStores(stores, a.isNonBlocking(), sl);
        return Val{Operand::temp(0), sir::voidType()};
    }
    if (a.left().type->getCanonicalType().isUnpackedArray()) {
        // An unpacked array is assigned element by element (7.6, 10.9.1).
        const std::optional<ArrayTarget> target = arrayTarget(a.left());
        if (!target) unsupported(sl, "assignment to this unpacked array");
        Stores stores;
        assignArray(baseOf(*target), target->indices, a.right(), stores, sl);
        flushStores(stores, a.isNonBlocking(), sl);
        return Val{Operand::temp(0), sir::voidType()};
    }
    const Val v = coerce(lowerExpr(a.right()), typeOf(*a.left().type, sl), sl);
    lowerStore(a.left(), v, a.isNonBlocking(), sl);
    return v;
}

std::optional<Lowerer::ArrayTarget> Lowerer::arrayTarget(const ast::Expression& e) {
    // A whole unpacked array in a slot, or a sub-array of one selected by
    // constant indices.
    if (e.kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = e.as<ast::ElementSelectExpression>();
        if (!es.value().type->getCanonicalType().isUnpackedArray()) return std::nullopt;
        std::optional<ArrayTarget> t = arrayTarget(es.value());
        const std::optional<std::int64_t> i = constInt(es.selector());
        if (!t || !i) return std::nullopt;
        t->indices.push_back(*i);
        return t;
    }
    // An array member of an element of an array of structures is the
    // sub-array of its slot at the element's indices.
    if (!e.type->getCanonicalType().isUnpackedArray() || structElement(*e.type) != nullptr)
        return std::nullopt;
    const std::optional<Storage> st = storageOf(e, e.sourceRange.start());
    if (!st || st->dynamic || !isArrayLeaf(*st)) return std::nullopt;
    if (!st->frame) return ArrayTarget{st->id, st->constIdx};
    // An automatic array, or what an array ref argument refers to.
    const bool ref = frameVars()[st->id].type.kind == sir::TypeKind::Ref;
    return ArrayTarget{st->id, st->constIdx, ref ? Place::Home::Ref : Place::Home::Frame};
}

const std::vector<sir::ArrayDim>& Lowerer::dimsOf(ArrayBase b) {
    return b.home == Place::Home::Slot ? design().slots[b.id].dims : frameVars()[b.id].dims;
}

Type Lowerer::elemTypeOf(ArrayBase b) {
    switch (b.home) {
        case Place::Home::Slot: return design().slots[b.id].type;
        case Place::Home::Frame: return frameVars()[b.id].type;
        case Place::Home::Ref: return frameVars()[b.id].target;
    }
    return sir::voidType();
}

Type Lowerer::homeType(const Place& p) { return elemTypeOf({p.home, p.slot}); }

void Lowerer::flushStores(const Stores& stores, bool nonblocking, slang::SourceLocation sl) {
    // The writes of an aggregate assignment, after every value of its right
    // side (VALUE_SEMANTICS.md 3.13).
    for (const auto& [place, value] : stores)
        storePlace(place, value, nonblocking, sl);
}

Lowerer::Place Lowerer::elementPlace(ArrayBase s, const std::vector<std::int64_t>& indices) {
    Place p;
    p.kind = Place::Kind::Element;
    p.home = s.home;
    p.slot = s.id;
    for (const std::int64_t i : indices)
        p.indices.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(i)).op);
    if (s.home == Place::Home::Slot) p.elem = elementNumber(design().slots[s.id], indices);
    p.type = elemTypeOf(s);
    return p;
}

void Lowerer::assignArray(ArrayBase s, std::vector<std::int64_t> indices,
                          const ast::Expression& rhs, Stores& out, slang::SourceLocation sl) {
    const ast::Expression* r = &rhs;
    while (r->kind == ast::ExpressionKind::Conversion &&
           r->as<ast::ConversionExpression>().operand().type->getCanonicalType().isUnpackedArray())
        r = &r->as<ast::ConversionExpression>().operand();
    const std::vector<sir::ArrayDim> dims = dimsOf(s);
    const std::size_t level = indices.size();
    if (level == dims.size()) {
        // One element: its value, for its constant indices (SIR_SPEC.md 5.3).
        const Val v = coerce(lowerExpr(*r), elemTypeOf(s), sl);
        out.emplace_back(elementPlace(s, indices), v);
        return;
    }
    const sir::ArrayDim d = dims[level];
    const std::int64_t step = d.left <= d.right ? 1 : -1;
    const auto count =
        static_cast<std::size_t>((d.left <= d.right ? d.right - d.left : d.left - d.right) + 1);
    if (r->kind == ast::ExpressionKind::SimpleAssignmentPattern ||
        r->kind == ast::ExpressionKind::StructuredAssignmentPattern ||
        r->kind == ast::ExpressionKind::ReplicatedAssignmentPattern) {
        // The elements of the pattern, from the left bound (10.9.1).
        const std::vector<const ast::Expression*> elems = patternElements(*r);
        if (elems.size() != count) unsupported(sl, "an assignment pattern of this size");
        for (std::size_t k = 0; k < count; ++k) {
            indices.push_back(d.left + static_cast<std::int64_t>(k) * step);
            assignArray(s, indices, *elems[k], out, sl);
            indices.pop_back();
        }
        return;
    }
    if (const std::optional<ArrayTarget> src = arrayTarget(*r)) {
        // Another array of the same shape: element by element, from the
        // left bounds (7.6).
        const std::vector<sir::ArrayDim>& sdims = dimsOf(baseOf(*src));
        if (sdims.size() - src->indices.size() != dims.size() - level)
            unsupported(sl, "assignment of an unpacked array of another shape");
        copyArray(s, std::move(indices), baseOf(*src), src->indices, out, sl);
        return;
    }
    unsupported(sl, "assignment of this unpacked array value");
}

void Lowerer::copyArray(ArrayBase to, std::vector<std::int64_t> toIndices, ArrayBase from,
                        std::vector<std::int64_t> fromIndices, Stores& out,
                        slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim> td = dimsOf(to);
    const std::vector<sir::ArrayDim> fd = dimsOf(from);
    if (toIndices.size() == td.size()) {
        const Val v = coerce(loadPlace(elementPlace(from, fromIndices), sl), elemTypeOf(to), sl);
        out.emplace_back(elementPlace(to, toIndices), v);
        return;
    }
    const sir::ArrayDim t = td[toIndices.size()];
    const sir::ArrayDim f = fd[fromIndices.size()];
    const auto count = [](sir::ArrayDim a) {
        return (a.left <= a.right ? a.right - a.left : a.left - a.right) + 1;
    };
    if (count(t) != count(f)) unsupported(sl, "assignment of an unpacked array of another shape");
    for (std::int64_t k = 0; k < count(t); ++k) {
        toIndices.push_back(t.left + (t.left <= t.right ? k : -k));
        fromIndices.push_back(f.left + (f.left <= f.right ? k : -k));
        copyArray(to, toIndices, from, fromIndices, out, sl);
        toIndices.pop_back();
        fromIndices.pop_back();
    }
}

std::uint32_t Lowerer::elementNumber(const sir::Slot& slot,
                                     const std::vector<std::int64_t>& indices) const {
    // Elements are numbered from the left bound, the rightmost dimension
    // fastest (SIR_SPEC.md 4.2).
    std::uint64_t n = 0;
    for (std::size_t d = 0; d < slot.dims.size(); ++d) {
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        const auto size = static_cast<std::uint64_t>((l <= r ? r - l : l - r) + 1);
        const auto off = static_cast<std::uint64_t>(l <= r ? indices[d] - l : l - indices[d]);
        n = n * size + off;
    }
    return static_cast<std::uint32_t>(n);
}

void Lowerer::lowerStore(const ast::Expression& lhs, const Val& v, bool nonblocking,
                         slang::SourceLocation sl) {
    // The value is computed before the indices of the target (SIR_SPEC.md
    // 7.3); the callers lower it first.
    storePlace(placeOf(lhs, sl), v, nonblocking, sl);
}

Lowerer::Place Lowerer::placeOf(const ast::Expression& lhs, slang::SourceLocation sl) {
    Place p;
    switch (lhs.kind) {
        case ast::ExpressionKind::NamedValue:
        case ast::ExpressionKind::HierarchicalValue: {
            const ast::Symbol& sym = lhs.as<ast::ValueExpressionBase>().symbol;
            if (sym.kind == ast::SymbolKind::ModportPort) {
                // What the modport port connects to (25.5, 25.5.4).
                const ast::Expression* c = sym.as<ast::ModportPortSymbol>().getConnectionExpr();
                if (c == nullptr) unsupported(sl, "modport port with no connection");
                return placeOf(*c, sl);
            }
            if (const std::optional<sir::FrameVarId> f = findFrameVar(sym)) {
                const sir::FrameVar& fv = frameVars()[*f];
                if (!fv.dims.empty()) unsupported(sl, "assignment to a whole unpacked array");
                const bool ref = fv.type.kind == sir::TypeKind::Ref;
                p.kind = ref ? Place::Kind::Ref : Place::Kind::Frame;
                p.frame = *f;
                p.type = ref ? fv.target : fv.type;
                return p;
            }
            if (!isVariable(sym) && sym.kind != ast::SymbolKind::Net)
                unsupported(sl, "assignment to a " + std::string(ast::toString(sym.kind)));
            if (!inSlot(sym))
                unsupported(sl, "automatic variable '" + std::string(sym.name) + "' here");
            p.slot = slotOf(sym.as<ast::ValueSymbol>(), sl);
            if (!design().slots[p.slot].dims.empty())
                unsupported(sl, "assignment to a whole unpacked array");
            p.type = design().slots[p.slot].type;
            return p;
        }
        case ast::ExpressionKind::ElementSelect:
        case ast::ExpressionKind::RangeSelect: {
            const bool element = lhs.kind == ast::ExpressionKind::ElementSelect;
            const ast::Expression& base = element ? lhs.as<ast::ElementSelectExpression>().value()
                                                  : lhs.as<ast::RangeSelectExpression>().value();
            if (element && base.type->isString()) {
                // s[i] is written as s.putc(i, c) (6.16.3).
                p.kind = Place::Kind::StringChar;
                p.base.push_back(placeOf(base, sl));
                p.indices.push_back(
                    lowerExpr(lhs.as<ast::ElementSelectExpression>().selector()).op);
                p.type = sir::u2(8);
                return p;
            }
            if (element && base.type->isUnpackedArray()) {
                if (structElement(*lhs.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                const std::optional<ArrayAccess> acc = arrayAccess(lhs);
                if (!acc) unsupported(sl, "assignment to an element of this array");
                return arrayElementPlace(*acc->base, acc->indices, sl);
            }
            // Bits of a packed variable, or of an element of an array.
            const ast::Type& bt = base.type->getCanonicalType();
            if (!bt.isIntegral()) unsupported(sl, "assignment to a part of this type");
            p.kind = Place::Kind::Bits;
            if (namedSymbol(base) != nullptr || isUnpackedMember(base)) {
                const std::optional<Storage> st = storageOf(base, sl);
                if (!st) unsupported(sl, "assignment to a part of this variable");
                if (isArrayLeaf(*st)) unsupported(sl, "assignment to a part of an unpacked array");
                // A member of an element of an array of structures is an
                // element of its slot.
                const Place leaf = storagePlace(*st);
                if (leaf.kind == Place::Kind::Frame || leaf.kind == Place::Kind::Ref) {
                    // Bits of an automatic variable, or of what a ref
                    // argument refers to (SIR_SPEC.md 7.4, 8).
                    p.home = leaf.kind == Place::Kind::Ref ? Place::Home::Ref : Place::Home::Frame;
                    p.slot = leaf.frame;
                } else {
                    p.slot = leaf.slot;
                    p.indices = leaf.indices;
                    p.elem = leaf.elem;
                }
            } else if (const std::optional<ArrayAccess> acc = arrayAccess(base)) {
                const Place el = arrayElementPlace(*acc->base, acc->indices, sl);
                p.home = el.home;
                p.slot = el.slot;
                p.indices = el.indices;
                p.elem = el.elem;
            } else {
                // A part of a member or of another part: the bits of
                // that place (narrow).
                Place inner = placeOf(base, sl);
                const std::uint32_t w = lhs.type->getBitWidth();
                if (element) {
                    const Val pos = bitPosition(lhs.as<ast::ElementSelectExpression>().selector(),
                                                bt.getFixedRange(), w);
                    if (pos.op.kind != Operand::Kind::Const)
                        unsupported(sl, "a select known at run time of a member or a part");
                    return narrow(
                        std::move(inner),
                        static_cast<std::int64_t>(design().constants[pos.op.id].aWords[0]), w, sl);
                }
                const Part part = partOf(lhs);
                if (part.pos) unsupported(sl, "a select known at run time of a member or a part");
                return narrow(std::move(inner), part.lsb, w, sl);
            }
            const std::uint32_t w = lhs.type->getBitWidth();
            if (element) {
                const Val pos = bitPosition(lhs.as<ast::ElementSelectExpression>().selector(),
                                            bt.getFixedRange(), w);
                if (pos.op.kind == Operand::Kind::Const)
                    p.lsb = static_cast<std::int64_t>(design().constants[pos.op.id].aWords[0]);
                else
                    p.pos = pos;
            } else {
                const Part part = partOf(lhs);
                p.pos = part.pos;
                p.lsb = part.lsb;
            }
            p.width = w;
            p.type = integral(sir::isFourState(homeType(p)), false, w);
            return p;
        }
        case ast::ExpressionKind::MemberAccess: {
            if (isUnpackedMember(lhs)) {
                // A member of an unpacked structure: its own slot or frame
                // variable (SIR_SPEC.md 4.2).
                if (structElement(*lhs.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                const std::optional<Storage> st = storageOf(lhs, sl);
                if (!st) unsupported(sl, "assignment to a member of this structure");
                if (isArrayLeaf(*st)) unsupported(sl, "assignment to a whole unpacked array");
                return storagePlace(*st);
            }
            // A member of a packed structure or union: its bits (7.2.1,
            // 7.3.1; SIR_SPEC.md 4.2).
            const auto& m = lhs.as<ast::MemberAccessExpression>();
            const ast::Type& vt = m.value().type->getCanonicalType();
            if (!vt.isIntegral() || m.member.kind != ast::SymbolKind::Field || vt.isTaggedUnion())
                unsupported(sl, "assignment to a member of this type");
            const std::int64_t lsb =
                vt.isPackedUnion()
                    ? 0
                    : static_cast<std::int64_t>(m.member.as<ast::FieldSymbol>().bitOffset);
            return narrow(placeOf(m.value(), sl), lsb, lhs.type->getBitWidth(), sl);
        }
        case ast::ExpressionKind::Concatenation: {
            // One packed vector (11.4.12): the parts from the most
            // significant one, their indices evaluated from the left.
            p.kind = Place::Kind::Concat;
            for (const ast::Expression* op : lhs.as<ast::ConcatenationExpression>().operands()) {
                p.base.push_back(placeOf(*op, sl));
                if (!sir::isIntegral(p.base.back().type))
                    unsupported(sl, "a concatenation target of this type");
                p.width += p.base.back().type.width;
            }
            p.type = typeOf(*lhs.type, sl);
            return p;
        }
        default: unsupported(sl, "assignment to this kind of target");
    }
}

Lowerer::Place Lowerer::narrow(Place base, std::int64_t lsb, std::uint32_t width,
                               slang::SourceLocation sl) {
    // Bits lsb .. lsb + width - 1 of an integral place. A select outside
    // the place would reach other bits of its slot, so it is reported.
    if (base.kind == Place::Kind::Frame || base.kind == Place::Kind::Ref) {
        // Bits of an automatic variable or of what a ref argument refers to.
        base.home = base.kind == Place::Kind::Ref ? Place::Home::Ref : Place::Home::Frame;
        base.slot = base.frame;
        base.kind = Place::Kind::Slot;
    }
    if (base.kind != Place::Kind::Slot && base.kind != Place::Kind::Element &&
        base.kind != Place::Kind::Bits)
        unsupported(sl, "assignment to a part of this target");
    const std::uint32_t outer = base.kind == Place::Kind::Bits ? base.width : base.type.width;
    if (lsb < 0 || lsb + width > outer) unsupported(sl, "a select outside a member or a part");
    if (base.kind == Place::Kind::Bits) {
        if (base.pos)
            base.pos = emit(
                Op::Add, base.pos->type,
                {base.pos->op, constant(base.pos->type, static_cast<std::uint64_t>(lsb)).op}, sl);
        else
            base.lsb += lsb;
    } else {
        base.lsb = lsb;
    }
    base.kind = Place::Kind::Bits;
    base.width = width;
    base.type = integral(sir::isFourState(homeType(base)), false, width);
    return base;
}

sir::SlotRange Lowerer::rangeOf(const Place& p) const {
    // The longest static prefix (11.5.3): the bits when the element and
    // the part are constant, else the element or the whole slot.
    if (!p.indices.empty() && !p.elem) return {p.slot, 0, 0, 0, 0};
    const std::uint32_t elem = p.elem.value_or(0);
    const std::uint32_t count = p.elem ? 1 : 0;
    if (p.kind == Place::Kind::Bits && !p.pos && p.lsb >= 0 &&
        p.lsb + p.width <= design_.slots[p.slot].type.width)
        return {p.slot, elem, count, static_cast<std::uint32_t>(p.lsb), p.width};
    return {p.slot, elem, count, 0, 0};
}

Val Lowerer::loadPlace(const Place& p, slang::SourceLocation sl) {
    // The variable an Element or Bits place is in: a slot, a frame
    // variable, or what a ref argument refers to (SIR_SPEC.md 5.3, 7.4, 8).
    const auto home = [&](Op slotOp, Op frameOp, Op refOp) {
        std::vector<Operand> ops{p.home == Place::Home::Slot ? Operand::slot(p.slot)
                                                             : Operand::frame(p.slot)};
        ops.insert(ops.end(), p.indices.begin(), p.indices.end());
        if (p.home == Place::Home::Slot) noteRead(rangeOf(p));
        const Op op = p.home == Place::Home::Slot    ? slotOp
                      : p.home == Place::Home::Frame ? frameOp
                                                     : refOp;
        return emit(op, homeType(p), std::move(ops), sl);
    };
    switch (p.kind) {
        case Place::Kind::Frame: return emit(Op::Fload, p.type, {Operand::frame(p.frame)}, sl);
        case Place::Kind::Ref: {
            std::vector<Operand> ops{Operand::frame(p.frame)};
            if (p.leaf != 0) ops.push_back(constant(sir::u2(32), p.leaf).op);
            return emit(Op::LoadRef, p.type, std::move(ops), sl);
        }
        case Place::Kind::Slot:
            noteRead(rangeOf(p));
            return emit(Op::Load, p.type, {Operand::slot(p.slot)}, sl);
        case Place::Kind::StringChar: {
            // Outside the string: 0 (table 6-9).
            const Val str = loadPlace(p.base[0], sl);
            return emit(Op::StrGet, p.type, {str.op, p.indices[0]}, sl);
        }
        case Place::Kind::Element: return home(Op::LoadElem, Op::LoadElem, Op::LoadRefElem);
        case Place::Kind::Concat: {
            std::vector<Operand> ops;
            ops.reserve(p.base.size());
            for (const Place& part : p.base)
                ops.push_back(loadPlace(part, sl).op);
            return emit(Op::Concat, p.type, std::move(ops), sl);
        }
        case Place::Kind::Bits: {
            const bool whole = p.indices.empty();
            const Val all = home(whole ? Op::Load : Op::LoadElem, whole ? Op::Fload : Op::LoadElem,
                                 whole ? Op::LoadRef : Op::LoadRefElem);
            const Val pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            return emit(p.width == 1 ? Op::Bitsel : Op::IdxPartsel, p.type, {all.op, pos.op}, sl);
        }
    }
    unsupported(sl, "reading this target");
}

void Lowerer::storePlace(const Place& p, const Val& v, bool nonblocking, slang::SourceLocation sl) {
    // An automatic variable takes no NBA (10.4.2): not through a ref either,
    // unless the ref names a whole static variable (nba.ref).
    const auto noNba = [&] {
        if (nonblocking) unsupported(sl, "nonblocking assignment to an automatic variable");
    };
    // A delayed continuous assignment, gate or UDP output writes a slot
    // range with drive.delay (SIR_SPEC.md 5.4).
    if (b_.delays) {
        // A whole slot or constant bits of one; the other places have no
        // slot to look up.
        const bool whole = p.kind == Place::Kind::Slot;
        const bool bits = p.kind == Place::Kind::Bits && p.home == Place::Home::Slot && !p.pos &&
                          p.indices.empty();
        if (!whole && !bits) unsupported(sl, "a delayed write of this target");
        const sir::Slot& slot = design().slots[p.slot];
        const bool part = bits && p.lsb >= 0 && p.lsb + p.width <= slot.type.width;
        if (!whole && !part) unsupported(sl, "a delayed write of this target");
        const Val dv = coerce(v, p.type, sl);
        const Operand target =
            whole ? Operand::slot(p.slot)
                  : Operand::slot(p.slot, static_cast<std::uint32_t>(p.lsb), p.width);
        emitVoid(Op::DriveDelay,
                 {target, dv.op, (*b_.delays)[0].op, (*b_.delays)[1].op, (*b_.delays)[2].op}, sl);
        noteWrite(rangeOf(p));
        return;
    }
    switch (p.kind) {
        case Place::Kind::Frame: {
            noNba();
            const Val fv = coerce(v, p.type, sl);
            emitVoid(Op::Fstore, {Operand::frame(p.frame), fv.op}, sl);
            return;
        }
        case Place::Kind::Ref: {
            std::vector<Operand> ops{Operand::frame(p.frame)};
            if (p.leaf != 0) ops.push_back(constant(sir::u2(32), p.leaf).op);
            ops.push_back(coerce(v, p.type, sl).op);
            emitVoid(nonblocking ? Op::NbaRef : Op::StoreRef, std::move(ops), sl);
            return;
        }
        case Place::Kind::Slot: {
            const Val sv = coerce(v, p.type, sl);
            emitVoid(nonblocking ? Op::Nba : Op::Store, {Operand::slot(p.slot), sv.op}, sl);
            noteWrite(rangeOf(p));
            return;
        }
        case Place::Kind::StringChar: {
            // s[i] = c is s.putc(i, c) (6.16.3): the new string to s.
            const Type st{sir::TypeKind::String, 0};
            const Val str = loadPlace(p.base[0], sl);
            const Val c = coerce(v, sir::u2(8), sl);
            const Val n =
                emit(Op::Builtin, st, {Operand::str("putc"), str.op, p.indices[0], c.op}, sl);
            storePlace(p.base[0], n, nonblocking, sl);
            return;
        }
        case Place::Kind::Concat: {
            // Each part gets its bits of the value, the most significant
            // part the most significant bits.
            const Val all = coerce(v, p.type, sl);
            std::uint32_t lsb = p.width;
            for (const Place& part : p.base) {
                const std::uint32_t w = part.type.width;
                lsb -= w;
                const Val bits = emit(Op::Partsel, integral(sir::isFourState(all.type), false, w),
                                      {all.op, constant(sir::u2(32), lsb).op}, sl);
                storePlace(part, bits, nonblocking, sl);
            }
            return;
        }
        case Place::Kind::Element: {
            const Val ev = coerce(v, p.type, sl);
            if (p.home != Place::Home::Slot) {
                noNba();
                std::vector<Operand> ops{Operand::frame(p.slot)};
                ops.insert(ops.end(), p.indices.begin(), p.indices.end());
                ops.push_back(ev.op);
                emitVoid(p.home == Place::Home::Frame ? Op::StoreElem : Op::StoreRefElem,
                         std::move(ops), sl);
                return;
            }
            std::vector<Operand> ops{Operand::slot(p.slot)};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            ops.push_back(ev.op);
            emitVoid(nonblocking ? Op::NbaElem : Op::StoreElem, std::move(ops), sl);
            noteWrite(rangeOf(p));
            return;
        }
        case Place::Kind::Bits: {
            const Val pv = coerce(v, p.type, sl);
            const Val pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            if (p.home != Place::Home::Slot) {
                noNba();
                std::vector<Operand> ops{Operand::frame(p.slot)};
                ops.insert(ops.end(), p.indices.begin(), p.indices.end());
                ops.push_back(pos.op);
                ops.push_back(pv.op);
                emitVoid(p.home == Place::Home::Frame ? Op::StoreBits : Op::StoreRefBits,
                         std::move(ops), sl);
                return;
            }
            const sir::Slot& slot = design().slots[p.slot];
            if (!p.pos && p.indices.empty()) {
                // A constant part of a whole variable: an ordinary store to a
                // slot range. A part wholly outside writes nothing (3.11).
                if (p.lsb >= static_cast<std::int64_t>(slot.type.width) ||
                    p.lsb + static_cast<std::int64_t>(p.width) <= 0)
                    return;
                if (p.lsb >= 0 && p.lsb + p.width <= slot.type.width) {
                    emitVoid(
                        nonblocking ? Op::Nba : Op::Store,
                        {Operand::slot(p.slot, static_cast<std::uint32_t>(p.lsb), p.width), pv.op},
                        sl);
                    noteWrite(rangeOf(p));
                    return;
                }
            }
            std::vector<Operand> ops{Operand::slot(p.slot)};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            ops.push_back(pos.op);
            ops.push_back(pv.op);
            emitVoid(nonblocking ? Op::NbaBits : Op::StoreBits, std::move(ops), sl);
            noteWrite(rangeOf(p));
            return;
        }
    }
}

// --- System tasks ------------------------------------------------------------

std::optional<Val> Lowerer::lowerEnumCall(const ast::CallExpression& call,
                                          slang::SourceLocation sl) {
    // The enumeration methods (6.19.5): the value is the first argument.
    const auto args = call.arguments();
    if (!isBuiltinMethod(call, ast::SymbolKind::EnumType) || args.empty()) return std::nullopt;
    const std::string name(call.getSubroutineName());
    const ast::Type& et = args[0]->type->getCanonicalType();
    std::vector<const ast::EnumValueSymbol*> members;
    for (const ast::EnumValueSymbol& m : et.as<ast::EnumType>().values())
        members.push_back(&m);
    if (members.empty()) return std::nullopt;
    const Type vt = typeOf(et, sl);
    const auto member = [&](std::size_t i) { return constantFrom(members[i]->getValue(), vt, sl); };
    const std::size_t n = members.size();
    if (name == "first" && args.size() == 1) return member(0);
    if (name == "last" && args.size() == 1) return member(n - 1);
    if (name == "num" && args.size() == 1) return constant(typeOf(*call.type, sl), n);
    const bool isName = name == "name" && args.size() == 1;
    if (!isName && !((name == "next" || name == "prev") && args.size() <= 2)) return std::nullopt;
    // next(N) and prev(N) wrap around; N is an int unsigned.
    std::size_t step = 0;
    if (!isName) {
        std::int64_t count = 1;
        if (args.size() == 2) {
            const std::optional<std::int64_t> c = constInt(*args[1]);
            if (!c)
                unsupported(sl, "enumeration method " + name + " with a count known at run time");
            count = *c;
        }
        step = static_cast<std::size_t>(count) % n;
        if (name == "prev") step = (n - step) % n;
    }
    // A value that is not a member gives the empty string for name() and
    // the default initial value of the base type (table 6-7) for next()
    // and prev(). Members are told apart by ===, so a value with x or z
    // bits matches only a member with the same bits.
    const Val v = coerce(lowerExpr(*args[0]), vt, sl);
    Val result = isName                 ? stringConstant("")
                 : sir::isFourState(vt) ? allXConstant(vt)
                                        : constant(vt, 0);
    for (std::size_t i = n; i-- > 0;) {
        const Val m = member(i);
        const Val hit = emit(Op::Ceq, sir::u2(1), {v.op, m.op}, sl);
        const Val then =
            isName ? stringConstant(std::string(members[i]->name)) : member((i + step) % n);
        result = emit(Op::Select, result.type, {hit.op, then.op, result.op}, sl);
    }
    return result;
}

std::optional<Val> Lowerer::lowerStringCall(const ast::CallExpression& call,
                                            slang::SourceLocation sl) {
    const std::string name(call.getSubroutineName());
    const auto args = call.arguments();
    const Type st{sir::TypeKind::String, 0};
    if (name == "$sformatf" || name == "$psprintf") {
        std::vector<Operand> ops{Operand::str("$sformatf")};
        lowerFormatArgs(args, ops, sl, 'd', name);
        return emit(Op::Syscall, st, std::move(ops), sl);
    }
    // The string methods (6.16): the string is the first argument.
    if (!isBuiltinMethod(call, ast::SymbolKind::StringType) || args.empty()) return std::nullopt;
    // A method that changes the string gets it as an lvalue argument.
    const ast::Expression* selfExpr = args[0];
    if (selfExpr->kind == ast::ExpressionKind::Assignment &&
        selfExpr->as<ast::AssignmentExpression>().isLValueArg())
        selfExpr = &selfExpr->as<ast::AssignmentExpression>().left();
    // A method that changes the string reads and writes it as one place,
    // its indices evaluated once.
    std::optional<Place> place;
    if (name == "putc" || name == "itoa" || name == "hextoa" || name == "octtoa" ||
        name == "bintoa" || name == "realtoa")
        place = placeOf(*selfExpr, sl);
    const Val self = place ? loadPlace(*place, sl) : lowerExpr(*selfExpr);
    const auto arg = [&](std::size_t i, Type t) { return coerce(lowerExpr(*args[i]), t, sl); };
    const auto method = [&](Type rt, std::vector<Operand> ops) {
        ops.insert(ops.begin(), {Operand::str(name), self.op});
        return emit(Op::Builtin, rt, std::move(ops), sl);
    };
    const Type rt = call.type->isVoid() ? sir::voidType() : typeOf(*call.type, sl);
    if (name == "len" && args.size() == 1) return method(rt, {});
    if (name == "getc" && args.size() == 2)
        return coerce(method(sir::u2(8), {arg(1, sir::s2(32)).op}), rt, sl);
    if ((name == "toupper" || name == "tolower") && args.size() == 1) return method(st, {});
    if ((name == "compare" || name == "icompare") && args.size() == 2)
        return method(rt, {arg(1, st).op});
    if (name == "substr" && args.size() == 3)
        return method(st, {arg(1, sir::s2(32)).op, arg(2, sir::s2(32)).op});
    if ((name == "atoi" || name == "atohex" || name == "atooct" || name == "atobin" ||
         name == "atoreal") &&
        args.size() == 1)
        return method(rt, {});
    // The methods that change the string: the new string goes to it.
    std::optional<Val> changed;
    if (name == "putc" && args.size() == 3)
        changed = method(st, {arg(1, sir::s2(32)).op, arg(2, sir::u2(8)).op});
    if ((name == "itoa" || name == "hextoa" || name == "octtoa" || name == "bintoa") &&
        args.size() == 2) {
        const Val v = lowerExpr(*args[1]);
        if (!sir::isIntegral(v.type)) unsupported(sl, name + " of this type");
        changed = method(st, {v.op});
    }
    if (name == "realtoa" && args.size() == 2)
        changed = method(st, {arg(1, Type{sir::TypeKind::Real, 0}).op});
    if (!changed || !place) unsupported(sl, "string method " + name);
    storePlace(*place, *changed, false, sl);
    return Val{Operand::temp(0), sir::voidType()};
}

void Lowerer::lowerFormatArgs(std::span<const ast::Expression* const> args,
                              std::vector<Operand>& ops, slang::SourceLocation sl, char radix,
                              std::string_view formatOf) {
    // A string literal not taken by an earlier specifier is a format
    // (SYSTEM_TASKS.md 3.1); the VM parses it once when it loads the
    // design. The format of $sformat and of $sformatf, then the first of
    // `args`, is a format whatever its form (3.13): a constant one is read
    // as a literal is, and as slang counts the arguments of literal formats
    // only, a wrong count for it is reported here; one known only at run
    // time is not implemented yet. A %t value is scaled from the caller's
    // time unit to ticks of the global precision here, so the VM needs no
    // unit.
    std::deque<char> pending;
    const auto takeFormat = [&](const std::string& text, slang::SourceLocation at) {
        std::vector<char> convs;
        std::string bad;
        std::string extra;
        if (!scanFormat(text, convs, bad, extra)) {
            // A % with no conversion is an error of the input; any other
            // specifier or flag left is one not implemented yet
            // (SYSTEM_TASKS.md 3.1, 3.12).
            if (bad == "%")
                error(at, "format has a % without a conversion");
            else
                unsupported(at, bad);
        }
        // A flag the standard does not define: one warning for the format
        // (3.12).
        if (!extra.empty()) warning(at, "format flag " + extra + " is not in IEEE 1800-2023");
        pending.assign(convs.begin(), convs.end());
        ops.push_back(Operand::str(withScope(text, b_.scope)));
    };
    // Set by a constant format that is not a literal: every later argument
    // fills one of its specifiers (3.13).
    bool counted = false;
    for (std::size_t k = 0; k < args.size(); ++k) {
        const ast::Expression* arg = args[k];
        if (k == 0 && !formatOf.empty() && arg->kind != ast::ExpressionKind::StringLiteral) {
            const std::optional<slang::ConstantValue> cv = constantValue(*arg);
            std::optional<std::string> text;
            if (cv && cv->isString()) text = cv->str();
            if (cv && cv->isInteger()) text = stringBytes(cv->integer());
            if (!text)
                unsupported(arg->sourceRange.start(),
                            std::string(formatOf) + " with a format that is not a constant");
            takeFormat(*text, arg->sourceRange.start());
            counted = true;
            continue;
        }
        if (counted && pending.empty())
            error(arg->sourceRange.start(), "too many arguments for the format");
        if (arg->kind == ast::ExpressionKind::EmptyArgument) {
            ops.push_back(Operand::str(" "));
            continue;
        }
        if (arg->kind == ast::ExpressionKind::StringLiteral && pending.empty()) {
            takeFormat(std::string(arg->as<ast::StringLiteral>().getValue()),
                       arg->sourceRange.start());
            continue;
        }
        // A value without a specifier prints in the radix of the task's
        // form, like that specifier (SYSTEM_TASKS.md 3.1).
        const bool explicitSpec = !pending.empty();
        const bool hasSpec = explicitSpec || radix != 'd';
        const char conv = explicitSpec ? pending.front() : radix;
        if (!pending.empty()) pending.pop_front();
        if (hasSpec && conv == 's' && arg->kind == ast::ExpressionKind::StringLiteral &&
            arg->type->getBitWidth() > 64) {
            // Its characters, which %s prints (SYSTEM_TASKS.md 3.5); a literal
            // has no zero bytes in front.
            const std::string text(arg->as<ast::StringLiteral>().getValue());
            if (text.find('\0') == std::string::npos) {
                ops.push_back(stringConstant(text).op);
                continue;
            }
        }
        Val v = lowerExpr(*arg);
        const bool realFormat = conv == 'e' || conv == 'f' || conv == 'g';
        if (isReal(v.type)) {
            // A real with an integer format goes through a 64-bit signed
            // integer; without a specifier the VM shows it like %g
            // (SYSTEM_TASKS.md 3.6).
            // A real %t value, in the caller's time unit, becomes a real
            // count of ticks of the global precision; the VM rounds it once
            // (SYSTEM_TASKS.md 3.7).
            if (conv == 't') {
                const int shift = timeScale().unit - precision();
                if (shift < 0 || shift > 19) unsupported(sl, "%t with this time unit");
                v = coerce(v, Type{sir::TypeKind::Real, 0}, sl);
                if (shift > 0)
                    v = emit(Op::Mul, v.type,
                             {v.op, realConstant(v.type, std::pow(10.0, shift)).op}, sl);
                ops.push_back(v.op);
                continue;
            }
            if (hasSpec && !realFormat) v = coerce(v, sir::s2(64), sl);
        } else if (isString(v.type)) {
            // A string with %s or no specifier prints its characters, in
            // the b, o and h forms too (SYSTEM_TASKS.md 3.1).
            if (explicitSpec && conv != 's')
                unsupported(arg->sourceRange.start(), "%" + std::string(1, conv) + " of a string");
        } else if (!sir::isIntegral(v.type)) {
            unsupported(arg->sourceRange.start(), "display of a value of this type");
        } else if (realFormat) {
            v = coerce(v, Type{sir::TypeKind::Real, 0}, sl);  // integer as a real (7.4)
        }
        if (conv == 't') {
            const int shift = timeScale().unit - precision();
            if (shift < 0 || shift > 19) unsupported(sl, "%t with this time unit");
            if (shift > 0) {
                std::uint64_t f = 1;
                for (int i = 0; i < shift; ++i)
                    f *= 10;
                const Type t64 = integral(sir::isFourState(v.type), false, 64);
                v = coerce(v, t64, sl);
                v = emit(Op::Mul, t64, {v.op, constant(t64, f).op}, sl);
            }
        }
        ops.push_back(v.op);
    }
    if (counted && !pending.empty())
        error(args[0]->sourceRange.start(),
              std::string("no argument for the format's %") + pending.front());
}

bool Lowerer::readsAutomatic(const ast::Expression& e) const {
    bool automatic = false;
    auto finder = ast::makeVisitor([&](auto&, const ast::NamedValueExpression& n) {
        if (n.symbol.kind == ast::SymbolKind::Variable ||
            n.symbol.kind == ast::SymbolKind::FormalArgument)
            automatic = automatic || !inSlot(n.symbol);
    });
    e.visit(finder);
    return automatic;
}

Val Lowerer::lowerDescriptor(const ast::Expression& e, slang::SourceLocation sl) {
    // slang gives some of these tasks an int argument; the value as
    // written keeps its x and z bits, which name no file (SYSTEM_TASKS.md 6.1).
    const ast::Expression& written = e.unwrapImplicitConversions();
    const ast::Expression& d = written.type->isIntegral() ? written : e;
    return coerce(lowerExpr(d), integral(true, false, 32), sl);
}

void Lowerer::lowerPostponed(const ast::CallExpression& call, bool monitor, bool file, char radix) {
    const slang::SourceLocation sl = call.sourceRange.start();
    // $fstrobe and $fmonitor read their descriptor at the call, and the
    // rest in the Postponed region (SYSTEM_TASKS.md 6.2).
    const auto args = file ? call.arguments().subspan(1) : call.arguments();
    // The process runs apart from the caller, so it cannot read the
    // caller's automatic variables; slang rejects them already (13.3.2).
    for (const ast::Expression* a : args)
        if (readsAutomatic(*a)) unsupported(sl, "$strobe or $monitor of an automatic variable");
    const sir::InstanceId inst = b_.isSub ? design_.subroutines[b_.proc].instance : proc().instance;
    sir::Process p;
    p.name = processName(design_.instances[inst].scopeName,
                         std::string(file ? "$f" : "$") + (monitor ? "monitor" : "strobe"));
    p.kind = sir::ProcessKind::Postponed;
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
    const auto display = [&] {
        std::vector<Operand> ops{
            Operand::str(radix == 'd' ? "$display" : "$display" + std::string(1, radix))};
        lowerFormatArgs(args, ops, sl, radix);
        emitVoid(Op::Syscall, std::move(ops), sl);
    };
    try {
        if (monitor) {
            // It shows the values the first time it runs and whenever one
            // of them is not the one it showed last; the time is not
            // compared (SIR_SPEC.md 7.2, SYSTEM_TASKS.md 4).
            std::optional<Val> changed;
            std::vector<std::pair<sir::FrameVarId, Val>> shown;
            for (const ast::Expression* a : args) {
                if (a->kind == ast::ExpressionKind::StringLiteral || constantValue(*a)) continue;
                if (a->kind == ast::ExpressionKind::Call &&
                    a->as<ast::CallExpression>().isSystemCall()) {
                    const std::string_view n = a->as<ast::CallExpression>().getSubroutineName();
                    if (n == "$time" || n == "$stime" || n == "$realtime") continue;
                }
                const Val v = lowerExpr(*a);
                const sir::FrameVarId fv = newFrameVar(v.type);
                const Val last = emit(Op::Fload, v.type, {Operand::frame(fv)}, sl);
                const Val ne =
                    compareValues(sir::isIntegral(v.type) ? Op::Cne : Op::Ne, v, last, sl);
                changed = changed ? bitOp(Op::Or, *changed, ne, sl) : ne;
                shown.emplace_back(fv, v);
            }
            const sir::FrameVarId seen = newFrameVar(sir::u2(1));
            Val show = emit(Op::Lnot, sir::u2(1),
                            {emit(Op::Fload, sir::u2(1), {Operand::frame(seen)}, sl).op}, sl);
            if (changed) show = bitOp(Op::Or, show, *changed, sl);
            const sir::BlockId yes = newBlock();
            const sir::BlockId done = newBlock();
            terminate(Op::BrIf, {show.op, Operand::block(yes), Operand::block(done)}, sl);
            setBlock(yes);
            for (const auto& [fv, v] : shown)
                emitVoid(Op::Fstore, {Operand::frame(fv), v.op}, sl);
            emitVoid(Op::Fstore, {Operand::frame(seen), constant(sir::u2(1), 1).op}, sl);
            display();
            terminate(Op::Br, {Operand::block(done)}, sl);
            setBlock(done);
        } else {
            display();
        }
        terminate(Op::Ret, {}, sl);
    } catch (const Unsupported&) {
        b_ = std::move(saved);
        throw;
    }
    b_ = std::move(saved);
    std::vector<Operand> ops{Operand::process(id)};
    if (file) ops.push_back(lowerDescriptor(*call.arguments()[0], sl).op);
    emitVoid(monitor ? Op::Monitor : Op::Strobe, std::move(ops), sl);
}

void Lowerer::lowerSystemTask(const ast::CallExpression& call) {
    const slang::SourceLocation sl = call.sourceRange.start();
    const std::string name(call.getSubroutineName());
    const auto args = call.arguments();
    // The b, o and h forms print values without a specifier in that radix
    // (SYSTEM_TASKS.md 3.1).
    const char radix =
        name.size() > 1 && (name.back() == 'b' || name.back() == 'o' || name.back() == 'h')
            ? name.back()
            : 'd';
    const std::string base = radix == 'd' ? name : name.substr(0, name.size() - 1);
    if (base == "$display" || base == "$write") {
        std::vector<Operand> ops{Operand::str(name)};
        lowerFormatArgs(args, ops, sl, radix);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (base == "$strobe" || base == "$monitor" || base == "$fstrobe" || base == "$fmonitor") {
        lowerPostponed(call, base.ends_with("monitor"), base.starts_with("$f"), radix);
        return;
    }
    if ((base == "$fdisplay" || base == "$fwrite") && !args.empty()) {
        // The descriptor, then as $display and $write (SYSTEM_TASKS.md 6.2).
        std::vector<Operand> ops{Operand::str(name), lowerDescriptor(*args[0], sl).op};
        lowerFormatArgs(args.subspan(1), ops, sl, radix);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$fclose" || name == "$fflush") {
        // $fflush with no argument flushes every file (SYSTEM_TASKS.md 6.2).
        std::vector<Operand> ops{Operand::str(name)};
        if (!args.empty()) ops.push_back(lowerDescriptor(*args[0], sl).op);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$fopen" || name == "$fgets" || name == "$fgetc" || name == "$ungetc" ||
        name == "$fread" || name == "$ftell" || name == "$fseek" || name == "$rewind" ||
        name == "$feof" || name == "$ferror") {
        (void)lowerExpr(call);  // its value unused
        return;
    }
    if (name == "$monitoron" || name == "$monitoroff") {
        emitVoid(Op::Syscall, {Operand::str(name)}, sl);
        return;
    }
    if (name == "$timeformat" && (args.empty() || args.size() == 4)) {
        // The unit, digits, suffix and minimum width of %t (SYSTEM_TASKS.md
        // 5.4); no arguments restore the defaults.
        std::vector<Operand> ops{Operand::str(name)};
        for (std::size_t i = 0; i < args.size(); ++i) {
            const Val v = lowerExpr(*args[i]);
            ops.push_back((i == 2 ? coerce(v, Type{sir::TypeKind::String, 0}, sl)
                                  : coerce(v, sir::s2(32), sl))
                              .op);
        }
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$printtimescale") {
        // One line with the unit and precision of the caller's design
        // element, or of the instance named (SYSTEM_TASKS.md 5.3, 20.4.2).
        const ast::Symbol* at = nullptr;
        if (args.size() == 1 && args[0]->kind == ast::ExpressionKind::ArbitrarySymbol) {
            at = args[0]->as<ast::ArbitrarySymbolExpression>().symbol.get();
        } else if (args.empty()) {
            for (const ast::Symbol* s = evalSymbol_; s != nullptr && at == nullptr;
                 s = s->getParentScope() != nullptr ? &s->getParentScope()->asSymbol() : nullptr)
                if (s->kind == ast::SymbolKind::InstanceBody)
                    at = s->as<ast::InstanceBodySymbol>().parentInstance;
        }
        if (at == nullptr || at->kind != ast::SymbolKind::Instance)
            unsupported(sl, "$printtimescale here or of this argument");
        const sir::TimeScale ts = timeScaleOf(at->as<ast::InstanceSymbol>().body);
        const std::string line = "Time scale of (" + at->getHierarchicalPath() + ") is " +
                                 sir::timeExponentText(ts.unit) + " / " +
                                 sir::timeExponentText(ts.precision) + "\n";
        emitVoid(Op::Syscall, {Operand::str("$write"), Operand::str(line)}, sl);
        return;
    }
    if ((name == "$sformat" || base == "$swrite") && !args.empty()) {
        // $sformat(dest, format, ...) and $swrite(dest, ...): $sformatf
        // into dest (21.3.3); $swrite reads every string literal argument
        // not taken by a specifier as a format, as lowerFormatArgs does.
        // $swriteb, $swriteo and $swriteh keep their names for the radix.
        std::vector<Operand> ops{
            Operand::str(radix == 'd' || name == "$sformat" ? "$sformatf" : name)};
        if (name == "$sformat")
            lowerFormatArgs(args.subspan(1), ops, sl, 'd', name);
        else
            lowerFormatArgs(args.subspan(1), ops, sl, radix);
        const Val v = emit(Op::Syscall, Type{sir::TypeKind::String, 0}, std::move(ops), sl);
        const ast::Expression* dest = args[0];
        if (dest->kind == ast::ExpressionKind::Assignment &&
            dest->as<ast::AssignmentExpression>().isLValueArg())
            dest = &dest->as<ast::AssignmentExpression>().left();
        lowerStore(*dest, coerce(v, typeOf(*dest->type, sl), sl), false, sl);
        return;
    }
    if (name == "$sscanf" || name == "$fscanf" || name == "$value$plusargs") {
        (void)lowerExpr(call);  // its value unused
        return;
    }
    if (const std::optional<Val> v = lowerStringCall(call, sl)) return;  // putc and the like
    if (name == "$fatal" || name == "$error" || name == "$warning" || name == "$info") {
        // The message names the calling scope (SYSTEM_TASKS.md 10.1);
        // $fatal's first argument is the level of $finish (20.10).
        std::vector<Operand> ops{Operand::str(name), Operand::str(b_.scope)};
        std::span<const ast::Expression* const> message = args;
        if (name == "$fatal") {
            std::uint64_t n = 1;
            if (!args.empty()) {
                const std::optional<std::int64_t> v = constInt(*args[0]);
                if (!v || *v < 0 || *v > 2) unsupported(sl, name + " with this first argument");
                n = static_cast<std::uint64_t>(*v);
                message = args.subspan(1);
            }
            ops.push_back(constant(sir::u2(32), n).op);
        }
        lowerFormatArgs(message, ops, sl);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$finish" || name == "$stop") {
        std::uint64_t n = 1;  // the default argument (20.2)
        if (!args.empty()) {
            const std::optional<std::int64_t> v = constInt(*args[0]);
            if (!v || *v < 0 || *v > 2) unsupported(sl, name + " with this argument");
            n = static_cast<std::uint64_t>(*v);
        }
        emitVoid(Op::Syscall, {Operand::str(name), constant(sir::u2(32), n).op}, sl);
        return;
    }
    unsupported(sl, "system task " + name);
}

}  // namespace kirimasim::front
