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

/// The bytes of an integral value as a string (6.16): the most significant
/// byte first, zero bytes dropped.
std::string integerToString(std::uint64_t a, std::uint64_t b, std::uint32_t width) {
    a &= ~b;
    std::string s;
    for (std::uint32_t k = (width + 7) / 8; k-- > 0;) {
        const auto c = static_cast<char>(k * 8 < 64 ? (a >> (k * 8)) & 0xffU : 0);
        if (c != 0) s.push_back(c);
    }
    return s;
}

bool isReal(Type t) { return t.kind == sir::TypeKind::Real || t.kind == sir::TypeKind::ShortReal; }

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

/// Real to integer (VALUE_SEMANTICS.md 1.5, 7.4): to the nearest, half away
/// from zero; NaN and values outside 64 bits give 0.
std::int64_t roundReal(double d) {
    if (std::isnan(d) || d >= 9223372036854775808.0 || d < -9223372036854775808.0) return 0;
    return static_cast<std::int64_t>(std::round(d));
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

/// System functions whose constant evaluation in slang is known to be wrong
/// (SIR_SPEC.md 6): expressions that contain them are computed at run time.
constexpr std::string_view kUntrustedConstants[] = {
    "$rtoi",  // negative values give 0 (slang issue #2023)
};

/// Whether an expression contains a call of one of kUntrustedConstants.
bool untrustedConstant(const ast::Expression& e) {
    const auto any = [](std::span<const ast::Expression* const> es) {
        for (const ast::Expression* x : es)
            if (untrustedConstant(*x)) return true;
        return false;
    };
    switch (e.kind) {
        case ast::ExpressionKind::Call: {
            const auto& c = e.as<ast::CallExpression>();
            if (c.isSystemCall() && among(c.getSubroutineName(), kUntrustedConstants)) return true;
            return any(c.arguments());
        }
        case ast::ExpressionKind::UnaryOp:
            return untrustedConstant(e.as<ast::UnaryExpression>().operand());
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
        case ast::ExpressionKind::Conversion:
            return untrustedConstant(e.as<ast::ConversionExpression>().operand());
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
/// false and the offending specifier for one that is not supported yet.
bool scanFormat(std::string_view f, std::vector<char>& convs, std::string& bad) {
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%') continue;
        if (++i >= f.size()) {
            bad = "%";
            return false;
        }
        if (f[i] == '%') continue;
        while (i < f.size() && std::string_view("-+ #").find(f[i]) != std::string_view::npos)
            ++i;
        while (i < f.size() && ((f[i] >= '0' && f[i] <= '9') || f[i] == '.'))
            ++i;
        if (i >= f.size()) {
            bad = "%";
            return false;
        }
        auto conv = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
        if (conv == 'x') conv = 'h';
        if (std::string_view("bohdcstefg").find(conv) == std::string_view::npos) {
            bad = std::string("%") + f[i];
            return false;
        }
        convs.push_back(conv);
    }
    return true;
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
    ast::EvalContext ctx(*evalSymbol_);
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
        if (w == 0 || w > 64) unsupported(sl, "values wider than 64 bits");
        const Type it = v.hasUnknown() ? (v.isSigned() ? sir::s4(w) : sir::u4(w))
                                       : (v.isSigned() ? sir::s2(w) : sir::u2(w));
        return coerce(constantFrom(cv, it, sl), t, sl);
    }
    const slang::SVInt& v = cv.integer();
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    const std::uint32_t have = v.getBitWidth();
    for (std::uint32_t i = 0; i < t.width; ++i) {
        // Bits beyond the constant's width extend it (they should not occur).
        const std::uint32_t k = i < have ? i : have - 1;
        const slang::logic_t bit =
            (i < have || v.isSigned()) ? v[static_cast<std::int32_t>(k)] : slang::logic_t(0);
        const bool x = bit.value == slang::logic_t::X_VALUE;
        const bool z = bit.value == slang::logic_t::Z_VALUE;
        if (bit.value == 1 || x) a |= std::uint64_t{1} << i;
        if (x || z) b |= std::uint64_t{1} << i;
    }
    if (!sir::isFourState(t)) b = 0;
    return constant(t, a, b);
}

Val Lowerer::coerce(Val v, Type want, slang::SourceLocation sl) {
    if (v.type == want) return v;
    if (isString(want) && sir::isIntegral(v.type)) {
        // Integer to string: its bytes, zero bytes dropped (6.16).
        if (v.op.kind == Operand::Kind::Const) {
            const sir::Constant& c = design().constants[v.op.id];
            return stringConstant(integerToString(c.aWords.empty() ? 0 : c.aWords[0],
                                                  c.bWords.empty() ? 0 : c.bWords[0],
                                                  c.type.width));
        }
        return emit(Op::Itos, want, {v.op}, sl);
    }
    if (isString(v.type) && sir::isIntegral(want)) return emit(Op::Stoi, want, {v.op}, sl);
    if ((isReal(v.type) || isReal(want)) && (sir::isIntegral(v.type) || isReal(v.type)) &&
        (sir::isIntegral(want) || isReal(want))) {
        // Between reals, and between reals and integers (VALUE_SEMANTICS.md
        // 1.5, 7.4). A constant is converted here.
        if (v.op.kind == Operand::Kind::Const) {
            const sir::Constant& c = design().constants[v.op.id];
            if (isReal(want)) {
                const double d = isReal(c.type)
                                     ? realValue(c)
                                     : integerToReal(c.aWords.empty() ? 0 : c.aWords[0],
                                                     c.bWords.empty() ? 0 : c.bWords[0], c.type);
                return realConstant(want, d);
            }
            return constant(want, static_cast<std::uint64_t>(roundReal(realValue(c))));
        }
        if (isReal(v.type) && isReal(want)) return emit(Op::Rconv, want, {v.op}, sl);
        return emit(isReal(want) ? Op::Itor : Op::Rtoi, want, {v.op}, sl);
    }
    if (!sir::isIntegral(v.type) || !sir::isIntegral(want))
        unsupported(sl, "conversion from " + sir::toString(v.type) + " to " + sir::toString(want));
    // A constant is converted here rather than by an instruction.
    if (v.op.kind == Operand::Kind::Const) {
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
    if (isReal(v.type))  // 0.0 is false, anything else true (VALUE_SEMANTICS.md 7.5)
        return emit(Op::Truth, sir::u2(1), {v.op}, e.sourceRange.start());
    if (!sir::isIntegral(v.type)) unsupported(e.sourceRange.start(), "condition of this type");
    if (v.type.width == 1 && !sir::isSigned(v.type)) return v;
    return emit(Op::Truth, integral(sir::isFourState(v.type), false, 1), {v.op},
                e.sourceRange.start());
}

// --- Places ------------------------------------------------------------------

namespace {

/// An unpacked-array element access: the array (a variable, a net or a
/// member of an unpacked structure) and the index expressions, outermost
/// dimension first.
struct ArrayAccess {
    const ast::Expression* base = nullptr;
    std::vector<const ast::Expression*> indices;
};

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
    if (!Lowerer::isUnpackedMember(*cur) &&
        (sym == nullptr || (!isVariable(*sym) && sym->kind != ast::SymbolKind::Net)))
        return std::nullopt;
    acc.base = cur;
    std::reverse(acc.indices.begin(), acc.indices.end());
    return acc;
}

}  // namespace

std::optional<sir::SlotRange> Lowerer::eventSlot(const ast::Expression& e) {
    // A named event is waited on, not watched for changes (SCHEDULER_SPEC.md 9.5).
    if (e.type->isEvent()) return std::nullopt;
    const ast::Expression* base = &e;
    std::optional<std::int64_t> pos;
    if (e.kind == ast::ExpressionKind::ElementSelect) {
        const auto& es = e.as<ast::ElementSelectExpression>();
        if (es.value().type->isUnpackedArray()) return std::nullopt;
        const std::optional<std::int64_t> idx = constInt(es.selector());
        if (!idx || e.type->getBitWidth() != 1) return std::nullopt;
        pos = positionOf(*idx, es.value().type->getFixedRange());
        if (!pos) return std::nullopt;
        base = &es.value();
    }
    const ast::Symbol* sym = namedSymbol(*base);
    if (unpackedStruct(*base->type) != nullptr ||
        (!isUnpackedMember(*base) && (sym == nullptr || findFrameVar(*sym) || !inSlot(*sym))))
        return std::nullopt;
    const std::optional<Storage> st = storageOf(*base, e.sourceRange.start());
    if (!st || st->frame) return std::nullopt;
    const sir::SlotId s = st->id;
    if (!design().slots[s].dims.empty()) return std::nullopt;
    if (pos) return sir::SlotRange{s, 0, 0, static_cast<std::uint32_t>(*pos), 1};
    return sir::SlotRange{s, 0, 0, 0, 0};
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

std::uint64_t Lowerer::delayTicks(const ast::Expression& e, slang::SourceLocation sl) {
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
                ticks = sir::delayTicks(static_cast<std::uint64_t>(*n), timeScale(), precision());
        }
    } else if (cv->isReal()) {
        ticks = sir::delayTicksReal(cv->real(), timeScale(), precision());
    }
    if (!ticks) unsupported(sl, "this delay value");
    return *ticks;
}

// --- Expressions -------------------------------------------------------------

Val Lowerer::lowerExpr(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
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
        case ast::ExpressionKind::StringLiteral:
            return constantFrom(e.as<ast::StringLiteral>().getIntValue(), typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::NamedValue:
        case ast::ExpressionKind::HierarchicalValue: {
            const ast::Symbol& sym = e.as<ast::ValueExpressionBase>().symbol;
            if (sym.kind == ast::SymbolKind::Parameter)
                return constantFrom(sym.as<ast::ParameterSymbol>().getValue(), typeOf(*e.type, sl),
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
            if (const std::optional<sir::FrameVarId> f = findFrameVar(sym))
                return emit(Op::Fload, frameVars()[*f].type, {Operand::frame(*f)}, sl);
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
            if (ops.empty() || width > 64) unsupported(sl, "values wider than 64 bits");
            const Val c = emit(Op::Concat, integral(four, false, width), std::move(ops), sl);
            return coerce(c, typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::MemberAccess: {
            if (isUnpackedMember(e)) {
                // A member of an unpacked structure: its own slot or frame
                // variable (SIR_SPEC.md 4.2).
                const std::optional<Storage> st = storageOf(e, sl);
                if (!st) unsupported(sl, "expression 'MemberAccess'");
                if (unpackedStruct(*e.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                if (!st->frame && !design().slots[st->id].dims.empty())
                    unsupported(sl, "an unpacked array as a value");
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
            if (width == 0 || width > 64) unsupported(sl, "values wider than 64 bits");
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
            if (width > 64) unsupported(sl, "values wider than 64 bits");
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
            if (std::optional<Val> v = lowerStringCall(call, sl)) return *v;
            if (std::optional<Val> v = lowerEnumCall(call, sl)) return *v;
            unsupported(sl, "system function " + name);
        }
        default: unsupported(sl, "expression '" + std::string(ast::toString(e.kind)) + "'");
    }
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
    const Type t64 = integral(sir::isFourState(i.type), true, 64);
    i = coerce(i, t64, sl);
    const Val right = constant(t64, static_cast<std::uint64_t>(std::int64_t{r.right}));
    Val pos = r.left >= r.right ? emit(Op::Sub, t64, {i.op, right.op}, sl)
                                : emit(Op::Sub, t64, {right.op, i.op}, sl);
    if (elemWidth != 1) pos = emit(Op::Mul, t64, {pos.op, constant(t64, elemWidth).op}, sl);
    return pos;
}

std::optional<std::uint32_t>
Lowerer::constElement(const sir::Slot& slot, const std::vector<const ast::Expression*>& indices) {
    std::optional<std::uint32_t> elem = 0;
    for (std::size_t d = 0; d < indices.size() && d < slot.dims.size(); ++d) {
        // Elements are numbered from the left bound (SIR_SPEC.md 4.2).
        const std::optional<std::int64_t> c = constInt(*indices[d]);
        const std::int64_t l = slot.dims[d].left;
        const std::int64_t r = slot.dims[d].right;
        const auto n = static_cast<std::uint32_t>(std::abs(l - r) + 1);
        std::optional<std::int64_t> pos;
        if (c) {
            const std::int64_t off = l <= r ? *c - l : l - *c;
            if (off >= 0 && off < n) pos = off;
        }
        elem = (elem && pos)
                   ? std::optional<std::uint32_t>(*elem * n + static_cast<std::uint32_t>(*pos))
                   : std::nullopt;
    }
    return elem;
}

Val Lowerer::lowerElementSelect(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& es = e.as<ast::ElementSelectExpression>();
    const ast::Type& vt = es.value().type->getCanonicalType();
    if (vt.isUnpackedArray()) {
        const std::optional<ArrayAccess> acc = arrayAccess(e);
        const std::optional<Storage> st = acc ? storageOf(*acc->base, sl) : std::nullopt;
        if (!st || st->frame) unsupported(sl, "selecting from this array");
        const sir::SlotId s = st->id;
        const sir::Slot& slot = design().slots[s];
        if (slot.dims.size() != acc->indices.size())
            unsupported(sl, "selecting a part of an unpacked array");
        std::vector<Operand> ops{Operand::slot(s)};
        // Constant indices: the sensitivity is that element alone.
        const std::optional<std::uint32_t> elem = constElement(slot, acc->indices);
        for (const ast::Expression* ixp : acc->indices) {
            const ast::Expression& ix = *ixp;
            const Val iv = lowerExpr(ix);
            if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
            ops.push_back(iv.op);
        }
        noteRead(elem ? sir::SlotRange{s, *elem, 1, 0, 0} : sir::SlotRange{s, 0, 0, 0, 0});
        const Type et = slot.type;
        return coerce(emit(Op::LoadElem, et, std::move(ops), sl), typeOf(*e.type, sl), sl);
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
    const slang::ConstantRange r = vt.getFixedRange();
    const std::uint32_t width = e.type->getBitWidth();
    const bool down = r.left >= r.right;
    if (rs.getSelectionKind() == ast::RangeSelectionKind::Simple) {
        const std::optional<std::int64_t> l = constInt(rs.left());
        const std::optional<std::int64_t> rr = constInt(rs.right());
        if (!l || !rr) unsupported(sl, "part-select with unknown bounds");
        // The bit nearest the right bound of the declared range.
        const std::int64_t near = down ? std::min(*l, *rr) : std::max(*l, *rr);
        const std::int64_t lsb = down ? near - r.right : r.right - near;
        return {std::nullopt, lsb, width};
    }
    const bool up = rs.getSelectionKind() == ast::RangeSelectionKind::IndexedUp;
    // Declared indices covered: [base, base + w - 1] going up, [base - w + 1,
    // base] going down. The one nearest the right bound gives the lsb.
    const std::int64_t w = width;
    if (const std::optional<std::int64_t> base = constInt(rs.left())) {
        const std::int64_t lo = up ? *base : *base - w + 1;
        const std::int64_t hi = up ? *base + w - 1 : *base;
        const std::int64_t lsb = down ? lo - r.right : r.right - hi;
        return {std::nullopt, lsb, width};
    }
    Val b = lowerExpr(rs.left());
    if (!sir::isIntegral(b.type)) unsupported(sl, "part-select base of this type");
    const Type t64 = integral(sir::isFourState(b.type), true, 64);
    b = coerce(b, t64, sl);
    // lsb = (up ? base : base - w + 1) - right when the range goes down,
    //       right - (up ? base + w - 1 : base) when it goes up.
    std::int64_t k = 0;
    Val pos;
    if (down) {
        k = (up ? 0 : -(w - 1)) - r.right;
        pos = emit(Op::Add, t64, {b.op, constant(t64, static_cast<std::uint64_t>(k)).op}, sl);
    } else {
        k = r.right - (up ? w - 1 : 0);
        pos = emit(Op::Sub, t64, {constant(t64, static_cast<std::uint64_t>(k)).op, b.op}, sl);
    }
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
        case ast::UnaryOperator::Plus: return coerce(lowerExpr(u.operand()), rt, sl);
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
            if (u.op == ast::UnaryOperator::LogicalNot && isReal(v.type))
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
    if (isString(l.type) || isString(r.type)) {
        // Strings compare by their characters (table 6-9).
        const Type st{sir::TypeKind::String, 0};
        l = coerce(l, st, sl);
        r = coerce(r, st, sl);
        return emit(op, sir::u2(1), {l.op, r.op}, sl);
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
        return arrayMatch(v, a->slot, a->indices, sl);
    }
    // ==? for integral values, so that x and z in the member match any
    // bit; == for others (11.4.13).
    const Val m = lowerExpr(e);
    const bool integralPair = sir::isIntegral(v.type) && sir::isIntegral(m.type);
    return compareValues(integralPair ? Op::Weq : Op::Eq, v, m, sl);
}

Val Lowerer::arrayMatch(const Val& v, sir::SlotId s, std::vector<std::int64_t> indices,
                        slang::SourceLocation sl) {
    const sir::Slot& slot = design().slots[s];
    if (indices.size() == slot.dims.size()) {
        std::vector<Operand> ops{Operand::slot(s)};
        for (const std::int64_t i : indices)
            ops.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(i)).op);
        noteRead({s, elementNumber(slot, indices), 1, 0, 0});
        const Val m = emit(Op::LoadElem, slot.type, std::move(ops), sl);
        const bool integralPair = sir::isIntegral(v.type) && sir::isIntegral(m.type);
        return compareValues(integralPair ? Op::Weq : Op::Eq, v, m, sl);
    }
    const sir::ArrayDim d = slot.dims[indices.size()];
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

namespace {

/// Whether evaluating e may read variable v: e names v, or calls a
/// subroutine, which might.
bool mayRead(const ast::Expression& e, const ast::Symbol* v) {
    struct Finder : ast::ASTVisitor<Finder, ast::VisitFlags::Expressions> {
        const ast::Symbol* target = nullptr;
        bool found = false;
        void handle(const ast::NamedValueExpression& n) { found = found || &n.symbol == target; }
        void handle(const ast::HierarchicalValueExpression& n) {
            found = found || &n.symbol == target;
        }
        void handle(const ast::CallExpression& c) {
            if (!c.isSystemCall())
                found = true;
            else
                visitDefault(c);
        }
    };
    Finder f;
    f.target = v;
    e.visit(f);
    return f.found;
}

}  // namespace

const ast::Symbol* rootSymbol(const ast::Expression& e) {
    const ast::Expression* cur = &e;
    for (;;) {
        switch (cur->kind) {
            case ast::ExpressionKind::MemberAccess:
                cur = &cur->as<ast::MemberAccessExpression>().value();
                break;
            case ast::ExpressionKind::ElementSelect:
                cur = &cur->as<ast::ElementSelectExpression>().value();
                break;
            case ast::ExpressionKind::RangeSelect:
                cur = &cur->as<ast::RangeSelectExpression>().value();
                break;
            default: return namedSymbol(*cur);
        }
    }
}

void Lowerer::checkAggregateSource(const ast::Symbol* target, const ast::Expression& rhs,
                                   slang::SourceLocation sl) {
    // Whether a pattern that reads its own target sees the old or the new
    // values of the elements written before it is not settled yet
    // (VALUE_SEMANTICS.md 8); such assignments are reported.
    const ast::Expression* r = &rhs;
    while (r->kind == ast::ExpressionKind::Conversion)
        r = &r->as<ast::ConversionExpression>().operand();
    if (isPattern(*r) && mayRead(*r, target))
        unsupported(sl, "assignment pattern that may read its own target");
}

Val Lowerer::lowerAssignment(const ast::AssignmentExpression& a) {
    const slang::SourceLocation sl = a.sourceRange.start();
    if (a.timingControl != nullptr) unsupported(sl, "intra-assignment timing control");
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
    if (unpackedStruct(*a.left().type) != nullptr) {
        // An unpacked structure is assigned member by member (7.2.2, 10.9.2).
        const std::optional<Storage> dst = storageOf(a.left(), sl);
        if (!dst) unsupported(sl, "assignment to this unpacked structure");
        checkAggregateSource(rootSymbol(a.left()), a.right(), sl);
        assignStruct(*dst, *a.left().type, a.right(), a.isNonBlocking(), sl);
        return Val{Operand::temp(0), sir::voidType()};
    }
    if (a.left().type->getCanonicalType().isUnpackedArray()) {
        // An unpacked array is assigned element by element (7.6, 10.9.1).
        const std::optional<ArrayTarget> target = arrayTarget(a.left());
        if (!target) unsupported(sl, "assignment to this unpacked array");
        checkAggregateSource(rootSymbol(a.left()), a.right(), sl);
        assignArray(target->slot, target->indices, a.right(), a.isNonBlocking(), sl);
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
    if (!e.type->getCanonicalType().isUnpackedArray()) return std::nullopt;
    const std::optional<Storage> st = storageOf(e, e.sourceRange.start());
    if (!st || st->frame || design().slots[st->id].dims.empty()) return std::nullopt;
    return ArrayTarget{st->id, {}};
}

void Lowerer::assignArray(sir::SlotId s, std::vector<std::int64_t> indices,
                          const ast::Expression& rhs, bool nonblocking, slang::SourceLocation sl) {
    const ast::Expression* r = &rhs;
    while (r->kind == ast::ExpressionKind::Conversion &&
           r->as<ast::ConversionExpression>().operand().type->getCanonicalType().isUnpackedArray())
        r = &r->as<ast::ConversionExpression>().operand();
    const std::vector<sir::ArrayDim>& dims = design().slots[s].dims;
    const std::size_t level = indices.size();
    if (level == dims.size()) {
        // One element: its value, to its constant indices (SIR_SPEC.md 5.3).
        const Val v = coerce(lowerExpr(*r), design().slots[s].type, sl);
        std::vector<Operand> ops{Operand::slot(s)};
        for (const std::int64_t i : indices)
            ops.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(i)).op);
        ops.push_back(v.op);
        emitVoid(nonblocking ? Op::NbaElem : Op::StoreElem, std::move(ops), sl);
        noteWrite({s, elementNumber(design().slots[s], indices), 1, 0, 0});
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
            assignArray(s, indices, *elems[k], nonblocking, sl);
            indices.pop_back();
        }
        return;
    }
    if (const std::optional<ArrayTarget> src = arrayTarget(*r)) {
        // Another array of the same shape: element by element, from the
        // left bounds (7.6).
        const std::vector<sir::ArrayDim>& sdims = design().slots[src->slot].dims;
        if (sdims.size() - src->indices.size() != dims.size() - level)
            unsupported(sl, "assignment of an unpacked array of another shape");
        copyArray(s, std::move(indices), src->slot, src->indices, nonblocking, sl);
        return;
    }
    unsupported(sl, "assignment of this unpacked array value");
}

void Lowerer::copyArray(sir::SlotId to, std::vector<std::int64_t> toIndices, sir::SlotId from,
                        std::vector<std::int64_t> fromIndices, bool nonblocking,
                        slang::SourceLocation sl) {
    const std::vector<sir::ArrayDim>& td = design().slots[to].dims;
    const std::vector<sir::ArrayDim>& fd = design().slots[from].dims;
    if (toIndices.size() == td.size()) {
        std::vector<Operand> load{Operand::slot(from)};
        for (const std::int64_t i : fromIndices)
            load.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(i)).op);
        noteRead({from, elementNumber(design().slots[from], fromIndices), 1, 0, 0});
        const Val v = coerce(emit(Op::LoadElem, design().slots[from].type, std::move(load), sl),
                             design().slots[to].type, sl);
        std::vector<Operand> store{Operand::slot(to)};
        for (const std::int64_t i : toIndices)
            store.push_back(constant(sir::s2(64), static_cast<std::uint64_t>(i)).op);
        store.push_back(v.op);
        emitVoid(nonblocking ? Op::NbaElem : Op::StoreElem, std::move(store), sl);
        noteWrite({to, elementNumber(design().slots[to], toIndices), 1, 0, 0});
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
        copyArray(to, toIndices, from, fromIndices, nonblocking, sl);
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
                p.kind = Place::Kind::Frame;
                p.frame = *f;
                p.type = frameVars()[*f].type;
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
                const std::optional<ArrayAccess> acc = arrayAccess(lhs);
                const std::optional<Storage> st = acc ? storageOf(*acc->base, sl) : std::nullopt;
                if (!st || st->frame) unsupported(sl, "assignment to an element of this array");
                p.kind = Place::Kind::Element;
                p.slot = st->id;
                const sir::Slot& slot = design().slots[p.slot];
                if (slot.dims.size() != acc->indices.size())
                    unsupported(sl, "assignment to a part of an unpacked array");
                for (const ast::Expression* ix : acc->indices) {
                    const Val iv = lowerExpr(*ix);
                    if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
                    p.indices.push_back(iv.op);
                }
                p.elem = constElement(slot, acc->indices);
                p.type = slot.type;
                return p;
            }
            // Bits of a packed variable, or of an element of an array.
            const ast::Type& bt = base.type->getCanonicalType();
            if (!bt.isIntegral()) unsupported(sl, "assignment to a part of this type");
            p.kind = Place::Kind::Bits;
            if (namedSymbol(base) != nullptr || isUnpackedMember(base)) {
                const std::optional<Storage> st = storageOf(base, sl);
                if (!st || st->frame) unsupported(sl, "assignment to a part of this variable");
                p.slot = st->id;
                if (!design().slots[p.slot].dims.empty())
                    unsupported(sl, "assignment to a part of an unpacked array");
            } else if (const std::optional<ArrayAccess> acc = arrayAccess(base)) {
                const std::optional<Storage> st = storageOf(*acc->base, sl);
                if (!st || st->frame) unsupported(sl, "assignment to a part of this variable");
                p.slot = st->id;
                if (design().slots[p.slot].dims.size() != acc->indices.size())
                    unsupported(sl, "assignment to a part of an unpacked array");
                p.elem = constElement(design().slots[p.slot], acc->indices);
                for (const ast::Expression* ix : acc->indices) {
                    const Val iv = lowerExpr(*ix);
                    if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
                    p.indices.push_back(iv.op);
                }
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
            p.type = integral(sir::isFourState(design().slots[p.slot].type), false, w);
            return p;
        }
        case ast::ExpressionKind::MemberAccess: {
            if (isUnpackedMember(lhs)) {
                // A member of an unpacked structure: its own slot or frame
                // variable (SIR_SPEC.md 4.2).
                const std::optional<Storage> st = storageOf(lhs, sl);
                if (!st) unsupported(sl, "assignment to a member of this structure");
                if (unpackedStruct(*lhs.type) != nullptr)
                    unsupported(sl, "unpacked structure used as a whole here");
                if (!st->frame && !design().slots[st->id].dims.empty())
                    unsupported(sl, "assignment to a whole unpacked array");
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
    base.type = integral(sir::isFourState(design().slots[base.slot].type), false, width);
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
    switch (p.kind) {
        case Place::Kind::Frame: return emit(Op::Fload, p.type, {Operand::frame(p.frame)}, sl);
        case Place::Kind::Slot:
            noteRead(rangeOf(p));
            return emit(Op::Load, p.type, {Operand::slot(p.slot)}, sl);
        case Place::Kind::StringChar: {
            // Outside the string: 0 (table 6-9).
            const Val str = loadPlace(p.base[0], sl);
            return emit(Op::StrGet, p.type, {str.op, p.indices[0]}, sl);
        }
        case Place::Kind::Element: {
            noteRead(rangeOf(p));
            std::vector<Operand> ops{Operand::slot(p.slot)};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            return emit(Op::LoadElem, p.type, std::move(ops), sl);
        }
        case Place::Kind::Concat: {
            std::vector<Operand> ops;
            ops.reserve(p.base.size());
            for (const Place& part : p.base)
                ops.push_back(loadPlace(part, sl).op);
            return emit(Op::Concat, p.type, std::move(ops), sl);
        }
        case Place::Kind::Bits: {
            noteRead(rangeOf(p));
            const Type st = design().slots[p.slot].type;
            std::vector<Operand> ops{Operand::slot(p.slot)};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            const Val whole =
                emit(p.indices.empty() ? Op::Load : Op::LoadElem, st, std::move(ops), sl);
            const Val pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            return emit(p.width == 1 ? Op::Bitsel : Op::IdxPartsel, p.type, {whole.op, pos.op}, sl);
        }
    }
    unsupported(sl, "reading this target");
}

void Lowerer::storePlace(const Place& p, const Val& v, bool nonblocking, slang::SourceLocation sl) {
    switch (p.kind) {
        case Place::Kind::Frame: {
            if (nonblocking) unsupported(sl, "nonblocking assignment to an automatic variable");
            const Val fv = coerce(v, p.type, sl);
            emitVoid(Op::Fstore, {Operand::frame(p.frame), fv.op}, sl);
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
            std::vector<Operand> ops{Operand::slot(p.slot)};
            ops.insert(ops.end(), p.indices.begin(), p.indices.end());
            ops.push_back(ev.op);
            emitVoid(nonblocking ? Op::NbaElem : Op::StoreElem, std::move(ops), sl);
            noteWrite(rangeOf(p));
            return;
        }
        case Place::Kind::Bits: {
            const sir::Slot& slot = design().slots[p.slot];
            const Val pv = coerce(v, p.type, sl);
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
            const Val pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
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
    if (args.empty() || !args[0]->type->getCanonicalType().isEnum()) return std::nullopt;
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
                 : sir::isFourState(vt) ? constant(vt, maskOf(vt.width), maskOf(vt.width))
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
        lowerFormatArgs(args, ops, sl);
        return emit(Op::Syscall, st, std::move(ops), sl);
    }
    // The string methods (6.16): the string is the first argument.
    if (args.empty() || !args[0]->type->isString()) return std::nullopt;
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
                              std::vector<Operand>& ops, slang::SourceLocation sl) {
    // A string literal not taken by an earlier specifier is a format
    // (SYSTEM_TASKS.md 3.1); the VM parses it once when it loads the
    // design. A %t value is scaled from the caller's time unit to ticks of
    // the global precision here, so the VM needs no unit.
    std::deque<char> pending;
    for (const ast::Expression* arg : args) {
        if (arg->kind == ast::ExpressionKind::EmptyArgument) {
            ops.push_back(Operand::str(" "));
            continue;
        }
        if (arg->kind == ast::ExpressionKind::StringLiteral && pending.empty()) {
            const std::string text(arg->as<ast::StringLiteral>().getValue());
            std::vector<char> convs;
            std::string bad;
            if (!scanFormat(text, convs, bad))
                unsupported(arg->sourceRange.start(), "format specifier " + bad);
            pending.assign(convs.begin(), convs.end());
            ops.push_back(Operand::str(text));
            continue;
        }
        const bool hasSpec = !pending.empty();
        const char conv = hasSpec ? pending.front() : 'd';
        if (!pending.empty()) pending.pop_front();
        Val v = lowerExpr(*arg);
        const bool realFormat = conv == 'e' || conv == 'f' || conv == 'g';
        if (isReal(v.type)) {
            // A real with an integer format goes through a 64-bit signed
            // integer; without a specifier the VM shows it like %g
            // (SYSTEM_TASKS.md 3.6).
            if (conv == 't') unsupported(arg->sourceRange.start(), "%t of a real value");
            if (hasSpec && !realFormat) v = coerce(v, sir::s2(64), sl);
        } else if (isString(v.type)) {
            // A string with %s or no specifier prints its characters.
            if (hasSpec && conv != 's')
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
}

void Lowerer::lowerSystemTask(const ast::CallExpression& call) {
    const slang::SourceLocation sl = call.sourceRange.start();
    const std::string name(call.getSubroutineName());
    const auto args = call.arguments();
    if (name == "$display" || name == "$write") {
        std::vector<Operand> ops{Operand::str(name)};
        lowerFormatArgs(args, ops, sl);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$sformat" && !args.empty()) {
        // $sformat(dest, format, ...): $sformatf into dest (21.3.3).
        std::vector<Operand> ops{Operand::str("$sformatf")};
        lowerFormatArgs(args.subspan(1), ops, sl);
        const Val v = emit(Op::Syscall, Type{sir::TypeKind::String, 0}, std::move(ops), sl);
        const ast::Expression* dest = args[0];
        if (dest->kind == ast::ExpressionKind::Assignment &&
            dest->as<ast::AssignmentExpression>().isLValueArg())
            dest = &dest->as<ast::AssignmentExpression>().left();
        lowerStore(*dest, coerce(v, typeOf(*dest->type, sl), sl), false, sl);
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
