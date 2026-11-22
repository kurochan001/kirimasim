// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/execution.h"
#include <cmath>
namespace kirimasim::kernel {
namespace {
// clang-format off
constexpr MathName kMath[] = {
    {"$ln", [](double x) { return std::log(x); }, nullptr},
    {"$log10", [](double x) { return std::log10(x); }, nullptr},
    {"$exp", [](double x) { return std::exp(x); }, nullptr},
    {"$sqrt", [](double x) { return std::sqrt(x); }, nullptr},
    {"$floor", [](double x) { return std::floor(x); }, nullptr},
    {"$ceil", [](double x) { return std::ceil(x); }, nullptr},
    {"$sin", [](double x) { return std::sin(x); }, nullptr},
    {"$cos", [](double x) { return std::cos(x); }, nullptr},
    {"$tan", [](double x) { return std::tan(x); }, nullptr},
    {"$asin", [](double x) { return std::asin(x); }, nullptr},
    {"$acos", [](double x) { return std::acos(x); }, nullptr},
    {"$atan", [](double x) { return std::atan(x); }, nullptr},
    {"$sinh", [](double x) { return std::sinh(x); }, nullptr},
    {"$cosh", [](double x) { return std::cosh(x); }, nullptr},
    {"$tanh", [](double x) { return std::tanh(x); }, nullptr},
    {"$asinh", [](double x) { return std::asinh(x); }, nullptr},
    {"$acosh", [](double x) { return std::acosh(x); }, nullptr},
    {"$atanh", [](double x) { return std::atanh(x); }, nullptr},
    {"$pow", nullptr, [](double x, double y) { return std::pow(x, y); }},
    {"$atan2", nullptr, [](double x, double y) { return std::atan2(x, y); }},
    {"$hypot", nullptr, [](double x, double y) { return std::hypot(x, y); }},
};
// clang-format on

/// What a run-time warning says of a descriptor that names a file that
/// is not open (SYSTEM_TASKS.md 6.1).
std::string descriptorText(std::uint32_t a) {
    std::string hex(8, '0');
    for (int i = 7, v = 0; i >= 0; --i, v += 4)
        hex[static_cast<std::size_t>(i)] = "0123456789abcdef"[(a >> v) & 0xFU];
    return std::string((a & 0x8000'0000U) != 0 ? "file descriptor" : "multichannel descriptor") +
           " 32'h" + hex;
}

}  // namespace
std::span<const MathName> mathFunctions() noexcept { return kMath; }
std::string notOpenText(W4 d) {
    if (static_cast<std::uint32_t>(d.b) != 0) return "the descriptor has x or z bits";
    return descriptorText(static_cast<std::uint32_t>(d.a)) + " names a file that is not open";
}

/// The same for input, which takes an fd: an mcd names no file to read
/// (SYSTEM_TASKS.md 6.3).
std::string notReadableText(W4 d) {
    const auto a = static_cast<std::uint32_t>(d.a);
    if (static_cast<std::uint32_t>(d.b) == 0 && (a & 0x8000'0000U) == 0)
        return descriptorText(a) + " is not a file descriptor, which input takes";
    return notOpenText(d);
}

}  // namespace kirimasim::kernel
