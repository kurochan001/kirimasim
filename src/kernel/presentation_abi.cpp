// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/presentation_abi.h"

#include "abi_boundary.h"
#include "abi_views.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/memfile.h"
#include "kirimasim/kernel/strength_value.h"
#include "service_tables.h"
#include <algorithm>
#include <limits>

namespace kirimasim::kernel {
namespace {
bool capture(const kirima_format_arg& a, FormatValue& v, std::vector<std::uint64_t>& packed) {
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

std::uint32_t format(kirima_bytes fmt, const kirima_format_arg* args, std::uint32_t n,
                     const kirima_format_context* c, kirima_byte_sink formatted,
                     kirima_byte_sink diagnostic, void* user, std::uint32_t* mismatch) {
    if (!c || !formatted || !diagnostic || !mismatch || (n && !args) || !abi::valid(fmt) ||
        !abi::valid(c->scope) || !abi::valid(c->cell) || !abi::valid(c->suffix) || !c->tick_scale ||
        c->precision < -15 || c->precision > 2)
        return KIRIMA_VALUE_INVALID;
    const TimeFormat tf{c->unit, c->decimals, std::string(abi::view(c->suffix)), c->min_width};
    if (!timeFormatError(tf).empty()) return KIRIMA_VALUE_INVALID;
    std::vector<FormatValue> values(n);
    std::vector<std::vector<std::uint64_t>> packed(n);
    for (std::uint32_t i = 0; i < n; ++i)
        if (!capture(args[i], values[i], packed[i])) return KIRIMA_VALUE_INVALID;
    const auto result = formatStringCall(abi::view(fmt), values, abi::view(c->scope),
                                         abi::view(c->cell), c->tick_scale, c->precision, tf);
    *mismatch = result.argumentMismatch;
    if (result.error.empty())
        formatted(user, abi::bytesOf(result.text));
    else
        diagnostic(user, abi::bytesOf(result.error));
    return KIRIMA_VALUE_OK;
}

struct Selection {
    std::vector<MemDim> dims;
    std::optional<std::int64_t> start, finish;
    bool decode(const kirima_memory_range* r) {
        if (!r || !r->dimensions || !r->dimension_count || r->reserved || r->start.reserved ||
            r->finish.reserved || r->start.known > 1 || r->finish.known > r->start.known)
            return false;
        if (r->start.known) start = r->start.value;
        if (r->finish.known) finish = r->finish.value;
        std::uint64_t elements = 1;
        for (const auto d : std::span(r->dimensions, r->dimension_count)) {
            const auto distance = static_cast<std::uint64_t>(std::max(d.left, d.right)) -
                                  static_cast<std::uint64_t>(std::min(d.left, d.right));
            if (distance >= std::numeric_limits<std::uint32_t>::max() / elements) return false;
            elements *= distance + 1;
            dims.push_back({d.left, d.right});
        }
        return true;
    }
};

std::uint32_t load(kirima_bytes text, const kirima_memory_range* range, std::uint32_t width,
                   std::uint32_t hex, std::uint32_t two, const kirima_value_in* items,
                   std::uint32_t count, kirima_scan_sink word,
                   kirima_mem_diagnostic_sink diagnostic, void* user) {
    Selection s;
    if (!word || !diagnostic || !width || two > 1 || hex > 1 || (count && !items) ||
        !abi::valid(text) || !s.decode(range))
        return KIRIMA_VALUE_INVALID;
    std::vector<std::vector<W4>> enums;
    for (const auto v : std::span(items, count)) {
        if (v.width != width || !abi::input(v)) return KIRIMA_VALUE_INVALID;
        enums.emplace_back(v.words, v.words + v.count);
    }
    const auto result =
        loadMemory(abi::view(text), hex, s.dims, width, s.start, s.finish, two, enums);
    for (const auto& w : result.words)
        word(user, w.elem, w.value.data(), static_cast<std::uint32_t>(w.value.size()));
    if (!result.range.empty()) diagnostic(user, KIRIMA_MEM_RANGE, abi::bytesOf(result.range));
    if (!result.count.empty()) diagnostic(user, KIRIMA_MEM_COUNT, abi::bytesOf(result.count));
    if (!result.error.empty()) diagnostic(user, KIRIMA_MEM_INPUT, abi::bytesOf(result.error));
    return KIRIMA_VALUE_OK;
}
std::uint32_t order(const kirima_memory_range* range, kirima_element_sink sink, void* user) {
    Selection s;
    if (!sink || !s.decode(range)) return KIRIMA_VALUE_INVALID;
    for (const auto elem : dumpOrder(s.dims, s.start, s.finish))
        sink(user, elem);
    return KIRIMA_VALUE_OK;
}
}  // namespace
const void* presentationService() noexcept {
    static constexpr kirima_presentation_api table{Boundary<format>::call, Boundary<load>::call,
                                                   Boundary<order>::call};
    return &table;
}
}  // namespace kirimasim::kernel
