// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/value_abi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "kirimasim/kernel/wide.h"

namespace {
namespace w = kirimasim::kernel::wide;
using W4 = kirima_w4;
bool equal(W4 a, W4 b) { return a.a == b.a && a.b == b.b; }
bool crossCheck(const kirima_value_api& api, unsigned width, bool unknown, bool sign) {
    const auto n = w::chunks(width);
    std::vector<W4> x(n, W4{0x8081223344556687ULL, unknown ? 0x90ULL : 0ULL});
    std::vector<W4> y(n, W4{0x1234567890abcdefULL, 0}), expected(n), actual(n);
    const auto mask = width % 64 ? (UINT64_C(1) << (width % 64)) - 1 : UINT64_MAX;
    for (auto* v : {&x, &y}) {
        v->back().a &= mask;
        v->back().b &= mask;
    }
    const kirima_value_in a{x.data(), width, n}, b{y.data(), width, n};
    const kirima_value_out dst{actual.data(), width, n};
    const auto matches = [&] {
        return std::equal(actual.begin(), actual.end(), expected.begin(), equal);
    };
    for (const auto& [op, native] : {std::pair{KIRIMA_TO_TWO, &w::to2},
                                     {KIRIMA_BIT_NOT, &w::bitNot},
                                     {KIRIMA_NEGATE, &w::neg}}) {
        native(expected.data(), x.data(), width);
        if (api.unary(op, dst, a) || !matches()) return false;
    }
    for (const auto& [op, native] : {std::pair{KIRIMA_BIT_AND, &w::bitAnd},
                                     {KIRIMA_BIT_OR, &w::bitOr},
                                     {KIRIMA_BIT_XOR, &w::bitXor},
                                     {KIRIMA_BIT_XNOR, &w::bitXnor},
                                     {KIRIMA_ADD, &w::add},
                                     {KIRIMA_SUBTRACT, &w::sub},
                                     {KIRIMA_MULTIPLY, &w::mul}}) {
        native(expected.data(), x.data(), y.data(), width);
        if (api.binary(op, dst, a, b, sign) || !matches()) return false;
    }
    for (const auto& [op, native] : {std::pair{KIRIMA_DIVIDE, &w::div}, {KIRIMA_MODULO, &w::mod}}) {
        native(expected.data(), x.data(), y.data(), width, sign);
        if (api.binary(op, dst, a, b, sign) || !matches()) return false;
    }
    W4 scalar{};
    for (const auto& [op, native] : {std::pair{KIRIMA_LT, &w::lt},
                                     {KIRIMA_LE, &w::le},
                                     {KIRIMA_GT, &w::gt},
                                     {KIRIMA_GE, &w::ge}}) {
        if (api.compare(op, &scalar, a, b, sign) ||
            !equal(scalar, native(x.data(), y.data(), width, sign)))
            return false;
    }
    for (const auto& [op, native] : {std::pair{KIRIMA_EQ, &w::eq},
                                     {KIRIMA_NE, &w::ne},
                                     {KIRIMA_CASE_EQ, &w::ceq},
                                     {KIRIMA_CASE_NE, &w::cne},
                                     {KIRIMA_WILD_EQ, &w::weq},
                                     {KIRIMA_WILD_NE, &w::wne}}) {
        if (api.compare(op, &scalar, a, b, sign) ||
            !equal(scalar, native(x.data(), y.data(), width)))
            return false;
    }
    for (const bool ignoreX : {false, true})
        if (api.compare(ignoreX ? KIRIMA_CASE_X : KIRIMA_CASE_Z, &scalar, a, b, sign) ||
            !equal(scalar, w::caseMatch(x.data(), y.data(), width, ignoreX)))
            return false;
    for (const auto& [op, native] : {std::pair{KIRIMA_RED_AND, &w::redAnd},
                                     {KIRIMA_RED_OR, &w::redOr},
                                     {KIRIMA_RED_XOR, &w::redXor},
                                     {KIRIMA_TRUTH, &w::truth}})
        if (api.reduce(op, &scalar, a) || !equal(scalar, native(x.data(), width))) return false;
    if (api.reduce(KIRIMA_HAS_UNKNOWN, &scalar, a) ||
        !equal(scalar, {w::hasUnknown(x.data(), width), 0}))
        return false;
    for (unsigned m = 0; m < 16; ++m) {
        std::uint32_t count = 0;
        if (api.count_bits(&count, a, m) ||
            count != w::countBits(x.data(), width, m & 1, m & 2, m & 4, m & 8))
            return false;
    }
    const W4 operand{3, 0};
    const kirima_value_in amount{&operand, 4, 1};
    w::pow(expected.data(), x.data(), width, sign, &operand, 4, false);
    if (api.power(dst, a, sign, amount, 0) || !matches()) return false;
    for (const auto& [op, native] :
         {std::pair{KIRIMA_SHIFT_LEFT, &w::shl}, {KIRIMA_SHIFT_RIGHT, &w::shr}}) {
        native(expected.data(), x.data(), width, &operand, 4);
        if (api.shift(op, dst, a, amount, sign) || !matches()) return false;
    }
    w::ashr(expected.data(), x.data(), width, &operand, 4, sign);
    if (api.shift(KIRIMA_SHIFT_ARITH_RIGHT, dst, a, amount, sign) || !matches()) return false;
    w::resize(expected.data(), width, &operand, 4, sign);
    if (api.resize(dst, amount, sign) || !matches()) return false;
    for (auto c : {W4{0, 0}, W4{1, 0}, W4{0, 1}, W4{1, 1}}) {
        w::select(expected.data(), c, x.data(), y.data(), width);
        if (api.select(dst, c, a, b) || !matches()) return false;
    }
    for (auto pos : {std::int64_t{-2}, std::int64_t{4}, std::numeric_limits<std::int64_t>::max()}) {
        w::bits(expected.data(), width, x.data(), width, pos);
        if (api.slice(dst, a, pos) || !matches()) return false;
        w::writeBits(expected.data(), width, pos, &operand, 4);
        if (api.write_bits(dst, pos, amount) || !matches()) return false;
    }
    std::int64_t pos = 0;
    std::uint32_t known = 0;
    const auto position = w::position(x.data(), width, sign);
    if (api.position(&pos, &known, a, sign) || known != position.has_value() ||
        pos != position.value_or(0))
        return false;
    for (bool shortReal : {false, true}) {
        double out = 0;
        const double ref = shortReal ? static_cast<double>(w::toShortReal(x.data(), width, sign))
                                     : w::toReal(x.data(), width, sign);
        if (api.to_real(&out, a, sign, shortReal) || out != ref) return false;
    }
    for (double number : {-12.5, 9.75, std::numeric_limits<double>::infinity()}) {
        w::fromReal(expected.data(), width, number, true, sign);
        if (api.from_real(dst, number, 1, sign) || !matches()) return false;
    }
    auto text = w::toText(x.data(), width);
    std::string copy(text.size(), '\0');
    std::uint64_t size = 0;
    if (api.to_text(copy.data(), copy.size(), &size, a) || size != text.size() || copy != text)
        return false;
    w::fromText(expected.data(), width, text);
    return !api.from_text(dst, text.data(), text.size()) && matches();
}
}  // namespace

bool checkNativeValues(const kirima_value_api& api) {
    for (unsigned width : {1U, 31U, 32U, 63U, 64U, 65U, 127U, 129U})
        for (bool unknown : {false, true})
            for (bool sign : {false, true})
                if (!crossCheck(api, width, unknown, sign)) return false;
    return true;
}
