// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/value_abi.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

#include "kirimasim/kernel/wide.h"
#include "value_services.h"

namespace kirimasim::kernel {
namespace {
static_assert(sizeof(kirima_w4) == 16 && offsetof(kirima_w4, b) == 8 &&
              std::is_standard_layout_v<kirima_w4> && std::is_trivially_copyable_v<kirima_w4>);

bool shape(kirima_value_out v) {
    return v.words != nullptr && v.width && v.count == wide::chunks(v.width);
}
bool input(kirima_value_in v) {
    if (!v.words || !v.width || v.count != wide::chunks(v.width)) return false;
    const unsigned used = v.width % 64;
    return !used || ((v.words[v.count - 1].a | v.words[v.count - 1].b) >> used) == 0;
}
bool same(kirima_value_out dst, kirima_value_in x) {
    return shape(dst) && input(x) && dst.width == x.width;
}
std::uint32_t fillX(kirima_value_out dst) {
    if (!shape(dst)) return KIRIMA_VALUE_INVALID;
    wide::allX(dst.words, dst.width);
    return KIRIMA_VALUE_OK;
}
std::uint32_t unary(std::uint32_t op, kirima_value_out dst, kirima_value_in x) {
    if (!same(dst, x)) return KIRIMA_VALUE_INVALID;
    switch (op) {
        case KIRIMA_TO_TWO: wide::to2(dst.words, x.words, x.width); break;
        case KIRIMA_BIT_NOT: wide::bitNot(dst.words, x.words, x.width); break;
        case KIRIMA_NEGATE: wide::neg(dst.words, x.words, x.width); break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t binary(std::uint32_t op, kirima_value_out dst, kirima_value_in x, kirima_value_in y,
                     std::uint32_t sign) {
    if (!same(dst, x) || !input(y) || y.width != x.width || sign > 1) return KIRIMA_VALUE_INVALID;
    switch (op) {
        case KIRIMA_BIT_AND: wide::bitAnd(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_BIT_OR: wide::bitOr(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_BIT_XOR: wide::bitXor(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_BIT_XNOR: wide::bitXnor(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_ADD: wide::add(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_SUBTRACT: wide::sub(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_MULTIPLY: wide::mul(dst.words, x.words, y.words, x.width); break;
        case KIRIMA_DIVIDE: wide::div(dst.words, x.words, y.words, x.width, sign); break;
        case KIRIMA_MODULO: wide::mod(dst.words, x.words, y.words, x.width, sign); break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t power(kirima_value_out dst, kirima_value_in x, std::uint32_t xs, kirima_value_in exp,
                    std::uint32_t es) {
    if (!same(dst, x) || !input(exp) || xs > 1 || es > 1) return KIRIMA_VALUE_INVALID;
    wide::pow(dst.words, x.words, x.width, xs, exp.words, exp.width, es);
    return KIRIMA_VALUE_OK;
}
std::uint32_t compare(std::uint32_t op, kirima_w4* dst, kirima_value_in x, kirima_value_in y,
                      std::uint32_t sign) {
    if (!dst || !input(x) || !input(y) || x.width != y.width || sign > 1)
        return KIRIMA_VALUE_INVALID;
    switch (op) {
        case KIRIMA_LT: *dst = wide::lt(x.words, y.words, x.width, sign); break;
        case KIRIMA_LE: *dst = wide::le(x.words, y.words, x.width, sign); break;
        case KIRIMA_GT: *dst = wide::gt(x.words, y.words, x.width, sign); break;
        case KIRIMA_GE: *dst = wide::ge(x.words, y.words, x.width, sign); break;
        case KIRIMA_EQ: *dst = wide::eq(x.words, y.words, x.width); break;
        case KIRIMA_NE: *dst = wide::ne(x.words, y.words, x.width); break;
        case KIRIMA_CASE_EQ: *dst = wide::ceq(x.words, y.words, x.width); break;
        case KIRIMA_CASE_NE: *dst = wide::cne(x.words, y.words, x.width); break;
        case KIRIMA_WILD_EQ: *dst = wide::weq(x.words, y.words, x.width); break;
        case KIRIMA_WILD_NE: *dst = wide::wne(x.words, y.words, x.width); break;
        case KIRIMA_CASE_Z: *dst = wide::caseMatch(x.words, y.words, x.width, false); break;
        case KIRIMA_CASE_X: *dst = wide::caseMatch(x.words, y.words, x.width, true); break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t reduce(std::uint32_t op, kirima_w4* dst, kirima_value_in x) {
    if (!dst || !input(x)) return KIRIMA_VALUE_INVALID;
    switch (op) {
        case KIRIMA_RED_AND: *dst = wide::redAnd(x.words, x.width); break;
        case KIRIMA_RED_OR: *dst = wide::redOr(x.words, x.width); break;
        case KIRIMA_RED_XOR: *dst = wide::redXor(x.words, x.width); break;
        case KIRIMA_TRUTH: *dst = wide::truth(x.words, x.width); break;
        case KIRIMA_HAS_UNKNOWN: *dst = {wide::hasUnknown(x.words, x.width), 0}; break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t count(std::uint32_t* dst, kirima_value_in x, std::uint32_t mask) {
    if (!dst || !input(x) || mask > 15) return KIRIMA_VALUE_INVALID;
    *dst = wide::countBits(x.words, x.width, mask & KIRIMA_COUNT_ZERO, mask & KIRIMA_COUNT_ONE,
                           mask & KIRIMA_COUNT_Z, mask & KIRIMA_COUNT_X);
    return KIRIMA_VALUE_OK;
}
std::uint32_t shift(std::uint32_t op, kirima_value_out dst, kirima_value_in x,
                    kirima_value_in amount, std::uint32_t sign) {
    if (!same(dst, x) || !input(amount) || sign > 1) return KIRIMA_VALUE_INVALID;
    switch (op) {
        case KIRIMA_SHIFT_LEFT:
            wide::shl(dst.words, x.words, x.width, amount.words, amount.width);
            break;
        case KIRIMA_SHIFT_RIGHT:
            wide::shr(dst.words, x.words, x.width, amount.words, amount.width);
            break;
        case KIRIMA_SHIFT_ARITH_RIGHT:
            wide::ashr(dst.words, x.words, x.width, amount.words, amount.width, sign);
            break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t resize(kirima_value_out dst, kirima_value_in x, std::uint32_t sign) {
    if (!shape(dst) || !input(x) || sign > 1) return KIRIMA_VALUE_INVALID;
    wide::resize(dst.words, dst.width, x.words, x.width, sign);
    return KIRIMA_VALUE_OK;
}
std::uint32_t select(kirima_value_out dst, kirima_w4 condition, kirima_value_in yes,
                     kirima_value_in no) {
    if (!same(dst, yes) || !input(no) || yes.width != no.width || (condition.a | condition.b) > 1)
        return KIRIMA_VALUE_INVALID;
    wide::select(dst.words, condition, yes.words, no.words, dst.width);
    return KIRIMA_VALUE_OK;
}
std::uint32_t slice(kirima_value_out dst, kirima_value_in x, std::int64_t lsb) {
    if (!shape(dst) || !input(x)) return KIRIMA_VALUE_INVALID;
    wide::bits(dst.words, dst.width, x.words, x.width, lsb);
    return KIRIMA_VALUE_OK;
}
std::uint32_t writeBits(kirima_value_out dst, std::int64_t lsb, kirima_value_in value) {
    if (!input({dst.words, dst.width, dst.count}) || !input(value)) return KIRIMA_VALUE_INVALID;
    wide::writeBits(dst.words, dst.width, lsb, value.words, value.width);
    return KIRIMA_VALUE_OK;
}
std::uint32_t position(std::int64_t* value, std::uint32_t* known, kirima_value_in x,
                       std::uint32_t sign) {
    if (!value || !known || !input(x) || sign > 1) return KIRIMA_VALUE_INVALID;
    const auto result = wide::position(x.words, x.width, sign);
    *value = result.value_or(0);
    *known = result.has_value();
    return KIRIMA_VALUE_OK;
}
std::uint32_t toReal(double* dst, kirima_value_in x, std::uint32_t sign, std::uint32_t shortReal) {
    if (!dst || !input(x) || sign > 1 || shortReal > 1) return KIRIMA_VALUE_INVALID;
    *dst = shortReal ? static_cast<double>(wide::toShortReal(x.words, x.width, sign))
                     : wide::toReal(x.words, x.width, sign);
    return KIRIMA_VALUE_OK;
}
std::uint32_t fromReal(kirima_value_out dst, double value, std::uint32_t four,
                       std::uint32_t truncate) {
    if (!shape(dst) || four > 1 || truncate > 1) return KIRIMA_VALUE_INVALID;
    wide::fromReal(dst.words, dst.width, value, four, truncate);
    return KIRIMA_VALUE_OK;
}
std::uint32_t toText(char* dst, std::uint64_t capacity, std::uint64_t* size, kirima_value_in x) {
    if ((!dst && capacity) || !size || !input(x)) return KIRIMA_VALUE_INVALID;
    const auto text = wide::toText(x.words, x.width);
    *size = text.size();
    if (!dst) return KIRIMA_VALUE_OK;
    if (text.size() > capacity) return KIRIMA_VALUE_CAPACITY;
    std::copy(text.begin(), text.end(), dst);
    return KIRIMA_VALUE_OK;
}
std::uint32_t fromText(kirima_value_out dst, const char* text, std::uint64_t size) {
    if (!shape(dst) || (!text && size) || size > std::numeric_limits<std::size_t>::max())
        return KIRIMA_VALUE_INVALID;
    wide::fromText(dst.words, dst.width, {text ? text : "", static_cast<std::size_t>(size)});
    return KIRIMA_VALUE_OK;
}

// Every C entry, including comparisons and conversions, translates allocation
// errors from the shared wide implementation before returning to the model.
template <auto Function> struct Boundary;
template <class... Args, std::uint32_t (*Function)(Args...)> struct Boundary<Function> {
    static std::uint32_t call(Args... args) noexcept {
        try {
            return Function(args...);
        } catch (const std::bad_alloc&) {
            return KIRIMA_VALUE_RESOURCE;
        } catch (const std::length_error&) {
            return KIRIMA_VALUE_RESOURCE;
        }
    }
};
constexpr kirima_value_api values{
    Boundary<fillX>::call,    Boundary<unary>::call,   Boundary<binary>::call,
    Boundary<power>::call,    Boundary<compare>::call, Boundary<reduce>::call,
    Boundary<count>::call,    Boundary<shift>::call,   Boundary<resize>::call,
    Boundary<select>::call,   Boundary<slice>::call,   Boundary<writeBits>::call,
    Boundary<position>::call, Boundary<toReal>::call,  Boundary<fromReal>::call,
    Boundary<toText>::call,   Boundary<fromText>::call};
}  // namespace

const void* queryService(std::uint32_t id, std::uint32_t version) noexcept {
    if (id == KIRIMA_SERVICE_VALUE && version == KIRIMA_VALUE_VERSION) return &values;
    return nullptr;
}
}  // namespace kirimasim::kernel
