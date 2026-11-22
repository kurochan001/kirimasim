// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "abi_views.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/presentation_abi.h"
#include "kirimasim/kernel/strength_value.h"
namespace kirimasim::kernel::abi {
inline bool captureFormat(const kirima_format_arg& a, FormatValue& v,
                          std::vector<std::uint64_t>& packed) {
    if (a.kind > KIRIMA_FORMAT_PATTERN || a.is_signed > 1 || a.four_state > 1 ||
        a.has_pattern > 1 || !abi::valid(a.text) || !abi::valid(a.named_pattern) ||
        !abi::valid(a.positional_pattern) ||
        a.strength_count != strengthWordCount(a.strength_width) ||
        (a.strength_count &&
         (!a.strength || a.kind != KIRIMA_FORMAT_INTEGRAL || a.strength_width != a.integral.width)))
        return false;
    v.hasPattern = a.has_pattern;
    v.namedPattern = abi::view(a.named_pattern);
    v.positionalPattern = abi::view(a.positional_pattern);
    v.strength = {a.strength, a.strength_count};
    v.strengthWidth = a.strength_width;
    switch (a.kind) {
        case KIRIMA_FORMAT_INTEGRAL: {
            if (!abi::input(a.integral)) return false;
            const bool four = a.four_state != 0;
            packed.reserve(static_cast<std::size_t>(a.integral.count) * (four ? 2 : 1));
            for (const W4 w : std::span(a.integral.words, a.integral.count)) {
                if (!four && w.b) return false;
                packed.push_back(w.a);
                if (four) packed.push_back(w.b);
            }
            v.integral = {packed.data(), a.integral.width, four, a.is_signed != 0};
            break;
        }
        case KIRIMA_FORMAT_STRING:
            v.kind = FormatValue::Kind::String;
            v.text = abi::view(a.text);
            break;
        case KIRIMA_FORMAT_REAL:
            v.kind = FormatValue::Kind::Real;
            v.real = a.real;
            break;
        case KIRIMA_FORMAT_EMPTY: v.kind = FormatValue::Kind::Empty; break;
        case KIRIMA_FORMAT_PATTERN: v.kind = FormatValue::Kind::Pattern; return v.hasPattern;
        default: return false;
    }
    return true;
}
}  // namespace kirimasim::kernel::abi
