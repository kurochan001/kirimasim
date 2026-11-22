// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/scalar_value.h"
#include <string>
#include <string_view>

namespace kirimasim::kernel {
using Math1Fn = double (*)(double);
using Math2Fn = double (*)(double, double);
struct MathName {
    std::string_view name;
    Math1Fn fn1;
    Math2Fn fn2;
};
std::span<const MathName> mathFunctions() noexcept;
std::string notOpenText(W4);
std::string notReadableText(W4);
enum class Logical { And, Or, Implies, Equivalent };
inline W4 logicalValue(Logical op, W4 x, W4 y) noexcept {
    const W4 a = truth4(x), b = truth4(y);
    switch (op) {
        case Logical::And: return and4(a, b);
        case Logical::Or: return or4(a, b);
        case Logical::Implies: return or4(lnot4(a), b);
        case Logical::Equivalent: return (a.b | b.b) ? kW4X : W4{a.a == b.a, 0};
    }
    return kW4X;
}
inline W4 selectReal(W4 condition, W4 yes, W4 no, bool shortReal) noexcept {
    const W4 truth = truth4(condition);
    if (!truth.b) return truth.a ? yes : no;
    return readReal(yes, shortReal) == readReal(no, shortReal) ? yes : W4{};
}
}  // namespace kirimasim::kernel
