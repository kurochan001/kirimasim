// Lowering of expressions, assignments and system task calls to SIR.
//
// Integral values of 1 to 64 bits only, so far. slang has already applied
// the SystemVerilog rules for expression widths and signedness: operands
// arrive converted (ConversionExpression), so most operators map to one
// instruction. coerce() turns a value into the type an instruction needs.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lowerer.h"
#include "slang/ast/EvalContext.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/LiteralExpressions.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/numeric/SVInt.h"

namespace haremasim::front {

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

/// Whether evaluating an expression can change anything.
bool sideEffects(const ast::Expression& e) {
    switch (e.kind) {
        case ast::ExpressionKind::Assignment: return true;
        case ast::ExpressionKind::Call: {
            const auto& c = e.as<ast::CallExpression>();
            if (!c.isSystemCall()) return true;
            const std::string_view n = c.getSubroutineName();
            if (n != "$time" && n != "$signed" && n != "$unsigned") return true;
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
        while (i < f.size() && f[i] >= '0' && f[i] <= '9')
            ++i;
        if (i >= f.size()) {
            bad = "%";
            return false;
        }
        auto conv = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
        if (conv == 'x') conv = 'h';
        if (std::string_view("bohdcst").find(conv) == std::string_view::npos) {
            bad = std::string("%") + f[i];
            return false;
        }
        convs.push_back(conv);
    }
    return true;
}

}  // namespace

// --- Constants and conversions -----------------------------------------------

std::optional<slang::ConstantValue> Lowerer::constantValue(const ast::Expression& e) {
    if (sideEffects(e)) return std::nullopt;
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

Val Lowerer::constantFrom(const slang::ConstantValue& cv, Type t, slang::SourceLocation sl) {
    if (!cv.isInteger()) unsupported(sl, "a constant that is not an integer");
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
    if (!sir::isIntegral(v.type)) unsupported(e.sourceRange.start(), "condition of this type");
    if (v.type.width == 1 && !sir::isSigned(v.type)) return v;
    return emit(Op::Truth, integral(sir::isFourState(v.type), false, 1), {v.op},
                e.sourceRange.start());
}

// --- Places ------------------------------------------------------------------

namespace {

/// An unpacked-array element access: the array variable and the index
/// expressions, outermost dimension first.
struct ArrayAccess {
    const ast::VariableSymbol* var = nullptr;
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
    if (acc.indices.empty() || cur->kind != ast::ExpressionKind::NamedValue) return std::nullopt;
    const ast::Symbol& sym = cur->as<ast::NamedValueExpression>().symbol;
    if (sym.kind != ast::SymbolKind::Variable) return std::nullopt;
    acc.var = &sym.as<ast::VariableSymbol>();
    std::reverse(acc.indices.begin(), acc.indices.end());
    return acc;
}

}  // namespace

std::optional<sir::SlotRange> Lowerer::eventSlot(const ast::Expression& e) {
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
    if (base->kind != ast::ExpressionKind::NamedValue) return std::nullopt;
    const ast::Symbol& sym = base->as<ast::NamedValueExpression>().symbol;
    if (sym.kind != ast::SymbolKind::Variable || findFrameVar(sym)) return std::nullopt;
    const auto& v = sym.as<ast::VariableSymbol>();
    if (v.lifetime != ast::VariableLifetime::Static) return std::nullopt;
    const sir::SlotId s = slotOf(v, e.sourceRange.start());
    if (!design().slots[s].dims.empty()) return std::nullopt;
    if (pos) return sir::SlotRange{s, 0, 0, static_cast<std::uint32_t>(*pos), 1};
    return sir::SlotRange{s, 0, 0, 0, 0};
}

// --- Expressions -------------------------------------------------------------

Val Lowerer::lowerExpr(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const bool pure = e.kind != ast::ExpressionKind::Assignment &&
                      e.kind != ast::ExpressionKind::Call && !sideEffects(e);
    if (pure && e.getConstant() != nullptr && e.getConstant()->isInteger() && e.type->isIntegral())
        return constantFrom(*e.getConstant(), typeOf(*e.type, sl), sl);

    switch (e.kind) {
        case ast::ExpressionKind::IntegerLiteral:
            return constantFrom(slang::ConstantValue(e.as<ast::IntegerLiteral>().getValue()),
                                typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::UnbasedUnsizedIntegerLiteral:
            return constantFrom(
                slang::ConstantValue(e.as<ast::UnbasedUnsizedIntegerLiteral>().getValue()),
                typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::StringLiteral:
            return constantFrom(e.as<ast::StringLiteral>().getIntValue(), typeOf(*e.type, sl), sl);
        case ast::ExpressionKind::NamedValue: {
            const ast::Symbol& sym = e.as<ast::NamedValueExpression>().symbol;
            if (sym.kind == ast::SymbolKind::Parameter)
                return constantFrom(sym.as<ast::ParameterSymbol>().getValue(), typeOf(*e.type, sl),
                                    sl);
            if (const std::optional<sir::FrameVarId> f = findFrameVar(sym))
                return emit(Op::Fload, proc().frame[*f].type, {Operand::frame(*f)}, sl);
            if (sym.kind != ast::SymbolKind::Variable)
                unsupported(sl, "reference to a " + std::string(ast::toString(sym.kind)));
            const auto& v = sym.as<ast::VariableSymbol>();
            if (v.lifetime != ast::VariableLifetime::Static)
                unsupported(sl, "automatic variable '" + std::string(v.name) + "' here");
            const sir::SlotId s = slotOf(v, sl);
            if (!design().slots[s].dims.empty()) unsupported(sl, "an unpacked array as a value");
            noteRead({s, 0, 0, 0, 0});
            return emit(Op::Load, design().slots[s].type, {Operand::slot(s)}, sl);
        }
        case ast::ExpressionKind::ElementSelect: return lowerElementSelect(e);
        case ast::ExpressionKind::RangeSelect: return lowerRangeSelect(e);
        case ast::ExpressionKind::UnaryOp: return lowerUnary(e);
        case ast::ExpressionKind::BinaryOp: return lowerBinary(e);
        case ast::ExpressionKind::ConditionalOp: {
            const auto& c = e.as<ast::ConditionalExpression>();
            if (c.conditions.size() != 1 || c.conditions[0].pattern != nullptr)
                unsupported(sl, "conditional operator with a pattern");
            if (sideEffects(c.left()) || sideEffects(c.right()))
                unsupported(sl, "conditional operator with side effects");
            const Type rt = typeOf(*e.type, sl);
            Val cond = lowerExpr(*c.conditions[0].expr);
            if (cond.type.width != 1 || sir::isSigned(cond.type))
                cond =
                    emit(Op::Truth, integral(sir::isFourState(cond.type), false, 1), {cond.op}, sl);
            const Val l = coerce(lowerExpr(c.left()), rt, sl);
            const Val r = coerce(lowerExpr(c.right()), rt, sl);
            return emit(Op::Select, rt, {cond.op, l.op, r.op}, sl);
        }
        case ast::ExpressionKind::Concatenation: {
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
            return coerce(lowerExpr(c.operand()), typeOf(*e.type, sl), sl);
        }
        case ast::ExpressionKind::Assignment:
            return lowerAssignment(e.as<ast::AssignmentExpression>());
        case ast::ExpressionKind::Call: {
            const auto& call = e.as<ast::CallExpression>();
            const std::string name(call.getSubroutineName());
            if (!call.isSystemCall()) unsupported(sl, "call of function '" + name + "'");
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

Val Lowerer::lowerElementSelect(const ast::Expression& e) {
    const slang::SourceLocation sl = e.sourceRange.start();
    const auto& es = e.as<ast::ElementSelectExpression>();
    const ast::Type& vt = es.value().type->getCanonicalType();
    if (vt.isUnpackedArray()) {
        const std::optional<ArrayAccess> acc = arrayAccess(e);
        if (!acc || findFrameVar(*acc->var) || acc->var->lifetime != ast::VariableLifetime::Static)
            unsupported(sl, "selecting from this array");
        const sir::SlotId s = slotOf(*acc->var, sl);
        const sir::Slot& slot = design().slots[s];
        if (slot.dims.size() != acc->indices.size())
            unsupported(sl, "selecting a part of an unpacked array");
        std::vector<Operand> ops{Operand::slot(s)};
        // Constant indices: the sensitivity is that element alone.
        std::optional<std::uint32_t> elem = 0;
        for (std::size_t d = 0; d < acc->indices.size(); ++d) {
            const ast::Expression& ix = *acc->indices[d];
            // Elements are numbered from the left bound (SIR_SPEC.md 4.2).
            const std::optional<std::int64_t> c = constInt(ix);
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
            const Val iv = lowerExpr(ix);
            if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
            ops.push_back(iv.op);
        }
        noteRead(elem ? sir::SlotRange{s, *elem, 1, 0, 0} : sir::SlotRange{s, 0, 0, 0, 0});
        const Type et = slot.type;
        return coerce(emit(Op::LoadElem, et, std::move(ops), sl), typeOf(*e.type, sl), sl);
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
            const Val old = coerce(lowerExpr(u.operand()), rt, sl);
            const Val n = emit(inc ? Op::Add : Op::Sub, rt, {old.op, constant(rt, 1).op}, sl);
            lowerStore(u.operand(), n, false, sl);
            return pre ? n : old;
        }
    }
    unsupported(sl, "unary operator");
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
        Val l = lowerExpr(b.left());
        Val r = lowerExpr(b.right());
        if (!sir::isIntegral(l.type) || !sir::isIntegral(r.type))
            unsupported(sl, "comparison of these types");
        const bool four = sir::isFourState(l.type) || sir::isFourState(r.type);
        const Type ct = integral(four, sir::isSigned(l.type) && sir::isSigned(r.type),
                                 std::max(l.type.width, r.type.width));
        l = coerce(l, ct, sl);
        r = coerce(r, ct, sl);
        const bool twoStateResult = op == Op::Ceq || op == Op::Cne;
        const Val res = emit(op, integral(four && !twoStateResult, false, 1), {l.op, r.op}, sl);
        return coerce(res, rt, sl);
    };
    const auto logical = [&](Op op) {
        if (sideEffects(b.right())) unsupported(sl, "logical operator with side effects");
        const Val l = lowerExpr(b.left());
        const Val r = lowerExpr(b.right());
        if (!sir::isIntegral(l.type) || !sir::isIntegral(r.type))
            unsupported(sl, "logical operator on these types");
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
            const Val r = lowerExpr(b.right());
            if (r.type != rt) unsupported(sl, "** with an exponent of another type");
            return emit(Op::Pow, rt, {l.op, r.op}, sl);
        }
    }
    unsupported(sl, "binary operator");
}

// --- Assignments -------------------------------------------------------------

Val Lowerer::lowerAssignment(const ast::AssignmentExpression& a) {
    const slang::SourceLocation sl = a.sourceRange.start();
    if (a.isCompound()) unsupported(sl, "compound assignment");
    if (a.timingControl != nullptr) unsupported(sl, "intra-assignment timing control");
    const Val v = coerce(lowerExpr(a.right()), typeOf(*a.left().type, sl), sl);
    lowerStore(a.left(), v, a.isNonBlocking(), sl);
    return v;
}

void Lowerer::lowerStore(const ast::Expression& lhs, const Val& v, bool nonblocking,
                         slang::SourceLocation sl) {
    // The value is computed before the indices of the target (SIR_SPEC.md
    // 7.3); the callers lower it first.
    switch (lhs.kind) {
        case ast::ExpressionKind::NamedValue: {
            const ast::Symbol& sym = lhs.as<ast::NamedValueExpression>().symbol;
            if (const std::optional<sir::FrameVarId> f = findFrameVar(sym)) {
                if (nonblocking) unsupported(sl, "nonblocking assignment to an automatic variable");
                const Val fv = coerce(v, proc().frame[*f].type, sl);
                emitVoid(Op::Fstore, {Operand::frame(*f), fv.op}, sl);
                return;
            }
            if (sym.kind != ast::SymbolKind::Variable)
                unsupported(sl, "assignment to a " + std::string(ast::toString(sym.kind)));
            const auto& var = sym.as<ast::VariableSymbol>();
            if (var.lifetime != ast::VariableLifetime::Static)
                unsupported(sl, "automatic variable '" + std::string(var.name) + "' here");
            const sir::SlotId s = slotOf(var, sl);
            if (!design().slots[s].dims.empty())
                unsupported(sl, "assignment to a whole unpacked array");
            const Val sv = coerce(v, design().slots[s].type, sl);
            emitVoid(nonblocking ? Op::Nba : Op::Store, {Operand::slot(s), sv.op}, sl);
            noteWrite({s, 0, 0, 0, 0});
            return;
        }
        case ast::ExpressionKind::ElementSelect:
        case ast::ExpressionKind::RangeSelect: {
            const bool element = lhs.kind == ast::ExpressionKind::ElementSelect;
            const ast::Expression& base = element ? lhs.as<ast::ElementSelectExpression>().value()
                                                  : lhs.as<ast::RangeSelectExpression>().value();
            if (element && base.type->isUnpackedArray()) {
                const std::optional<ArrayAccess> acc = arrayAccess(lhs);
                if (!acc || findFrameVar(*acc->var) ||
                    acc->var->lifetime != ast::VariableLifetime::Static)
                    unsupported(sl, "assignment to an element of this array");
                const sir::SlotId s = slotOf(*acc->var, sl);
                const sir::Slot& slot = design().slots[s];
                if (slot.dims.size() != acc->indices.size())
                    unsupported(sl, "assignment to a part of an unpacked array");
                const Val ev = coerce(v, slot.type, sl);
                std::vector<Operand> ops{Operand::slot(s)};
                for (const ast::Expression* ix : acc->indices) {
                    const Val iv = lowerExpr(*ix);
                    if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
                    ops.push_back(iv.op);
                }
                ops.push_back(ev.op);
                emitVoid(nonblocking ? Op::NbaElem : Op::StoreElem, std::move(ops), sl);
                noteWrite({s, 0, 0, 0, 0});
                return;
            }
            // Bits of a packed variable, or of an element of an array.
            const ast::Type& bt = base.type->getCanonicalType();
            if (!bt.isIntegral()) unsupported(sl, "assignment to a part of this type");
            std::vector<Operand> ops;
            sir::SlotId s = 0;
            if (base.kind == ast::ExpressionKind::NamedValue) {
                const ast::Symbol& sym = base.as<ast::NamedValueExpression>().symbol;
                if (sym.kind != ast::SymbolKind::Variable || findFrameVar(sym) ||
                    sym.as<ast::VariableSymbol>().lifetime != ast::VariableLifetime::Static)
                    unsupported(sl, "assignment to a part of this variable");
                s = slotOf(sym.as<ast::VariableSymbol>(), sl);
                if (!design().slots[s].dims.empty())
                    unsupported(sl, "assignment to a part of an unpacked array");
                ops.push_back(Operand::slot(s));
            } else if (const std::optional<ArrayAccess> acc = arrayAccess(base)) {
                if (findFrameVar(*acc->var) || acc->var->lifetime != ast::VariableLifetime::Static)
                    unsupported(sl, "assignment to a part of this variable");
                s = slotOf(*acc->var, sl);
                if (design().slots[s].dims.size() != acc->indices.size())
                    unsupported(sl, "assignment to a part of an unpacked array");
                ops.push_back(Operand::slot(s));
                for (const ast::Expression* ix : acc->indices) {
                    const Val iv = lowerExpr(*ix);
                    if (!sir::isIntegral(iv.type)) unsupported(sl, "index of this type");
                    ops.push_back(iv.op);
                }
            } else {
                unsupported(sl, "assignment to a part of this expression");
            }
            const sir::Slot& slot = design().slots[s];
            const std::uint32_t w = lhs.type->getBitWidth();
            Part p;
            if (element) {
                const Val pos = bitPosition(lhs.as<ast::ElementSelectExpression>().selector(),
                                            bt.getFixedRange(), w);
                if (pos.op.kind == Operand::Kind::Const) {
                    const sir::Constant& c = design().constants[pos.op.id];
                    p = {std::nullopt, static_cast<std::int64_t>(c.aWords[0]), w};
                } else {
                    p = {pos, 0, w};
                }
            } else {
                p = partOf(lhs);
            }
            const Val pv = coerce(v, integral(sir::isFourState(slot.type), false, w), sl);
            if (!p.pos && ops.size() == 1) {
                // A constant part of a whole variable: an ordinary store to a
                // slot range. A part wholly outside writes nothing (3.11).
                if (p.lsb >= static_cast<std::int64_t>(slot.type.width) ||
                    p.lsb + static_cast<std::int64_t>(w) <= 0)
                    return;
                if (p.lsb >= 0 && p.lsb + w <= slot.type.width) {
                    emitVoid(nonblocking ? Op::Nba : Op::Store,
                             {Operand::slot(s, static_cast<std::uint32_t>(p.lsb), w), pv.op}, sl);
                    noteWrite({s, 0, 0, static_cast<std::uint32_t>(p.lsb), w});
                    return;
                }
            }
            const Val pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            ops.push_back(pos.op);
            ops.push_back(pv.op);
            emitVoid(nonblocking ? Op::NbaBits : Op::StoreBits, std::move(ops), sl);
            noteWrite({s, 0, 0, 0, 0});
            return;
        }
        default: unsupported(sl, "assignment to this kind of target");
    }
}

// --- System tasks ------------------------------------------------------------

void Lowerer::lowerSystemTask(const ast::CallExpression& call) {
    const slang::SourceLocation sl = call.sourceRange.start();
    const std::string name(call.getSubroutineName());
    const auto args = call.arguments();
    if (name == "$display" || name == "$write") {
        // A string literal not taken by an earlier specifier is a format
        // (SYSTEM_TASKS.md 3.1); the VM parses it once when it loads the
        // design. A %t value is scaled from the caller's time unit to ticks
        // of the global precision here, so the VM needs no unit.
        std::vector<Operand> ops{Operand::str(name)};
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
            const char conv = pending.empty() ? 'd' : pending.front();
            if (!pending.empty()) pending.pop_front();
            Val v = lowerExpr(*arg);
            if (!sir::isIntegral(v.type))
                unsupported(arg->sourceRange.start(), "display of a value of this type");
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

}  // namespace haremasim::front
