// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The files of $readmemb, $readmemh, $writememb and $writememh
// (SYSTEM_TASKS.md 7).

#include "kirimasim/kernel/memfile.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kirimasim::kernel {

namespace {

/// The addresses of the first dimension that a task's arguments take, in
/// order: from the lowest to the highest address with none, from start to
/// the highest with start alone, from start to finish (down when start is
/// larger) with both (SYSTEM_TASKS.md 7.1).
struct Range {
    std::int64_t from = 0;
    std::int64_t to = 0;
    std::int64_t step = 1;
    [[nodiscard]] bool holds(std::int64_t a) const {
        return step > 0 ? from <= a && a <= to : to <= a && a <= from;
    }
    [[nodiscard]] std::uint64_t size() const {
        if (step > 0 && from > to) return 0;
        return static_cast<std::uint64_t>(step > 0 ? to - from : from - to) + 1;
    }
};

Range rangeOf(const std::vector<MemDim>& dims, std::optional<std::int64_t> start,
              std::optional<std::int64_t> finish) {
    const std::int64_t lo = std::min(dims[0].left, dims[0].right);
    const std::int64_t hi = std::max(dims[0].left, dims[0].right);
    if (!start) return {lo, hi, 1};
    if (!finish) return {*start, hi, 1};
    return {*start, *finish, *start <= *finish ? 1 : -1};
}

/// The number of elements below one address of the first dimension.
std::uint64_t innerSize(const std::vector<MemDim>& dims) {
    std::uint64_t n = 1;
    for (std::size_t d = 1; d < dims.size(); ++d)
        n *= static_cast<std::uint64_t>(std::max(dims[d].left, dims[d].right) -
                                        std::min(dims[d].left, dims[d].right) + 1);
    return n;
}

/// The element at address a of the first dimension and place `inner`
/// below it, each lower dimension from its low to its high index
/// (21.4.3), numbered as slots number them.
std::uint32_t elementAt(const std::vector<MemDim>& dims, std::int64_t a, std::uint64_t inner) {
    std::vector<std::int64_t> index(dims.size());
    index[0] = a;
    for (std::size_t d = dims.size(); d-- > 1;) {
        const std::int64_t lo = std::min(dims[d].left, dims[d].right);
        const auto n = static_cast<std::uint64_t>(std::max(dims[d].left, dims[d].right) - lo + 1);
        index[d] = lo + static_cast<std::int64_t>(inner % n);
        inner /= n;
    }
    std::uint64_t elem = 0;
    for (std::size_t d = 0; d < dims.size(); ++d) {
        const std::int64_t l = dims[d].left;
        const std::int64_t r = dims[d].right;
        const auto n = static_cast<std::uint64_t>(std::max(l, r) - std::min(l, r) + 1);
        elem = elem * n + static_cast<std::uint64_t>(l <= r ? index[d] - l : l - index[d]);
    }
    return static_cast<std::uint32_t>(elem);
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/// The bits of one digit of base 2 or 16, with x and z; false for a
/// character that is not one.
bool digitBits(char c, bool hex, W4& d) {
    const std::uint64_t all = hex ? 0xF : 0x1;
    if (c == 'x' || c == 'X') {
        d = {all, all};
        return true;
    }
    if (c == 'z' || c == 'Z') {
        d = {0, all};
        return true;
    }
    std::uint64_t v = 0;
    if (c >= '0' && c <= '9')
        v = static_cast<std::uint64_t>(c - '0');
    else if (hex && c >= 'a' && c <= 'f')
        v = static_cast<std::uint64_t>(c - 'a') + 10;
    else if (hex && c >= 'A' && c <= 'F')
        v = static_cast<std::uint64_t>(c - 'A') + 10;
    else
        return false;
    if (v > all) return false;
    d = {v, 0};
    return true;
}

std::string hexText(std::int64_t a) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%llx", static_cast<unsigned long long>(a));
    return buf;
}

}  // namespace

MemLoad loadMemory(std::string_view text, bool hex, const std::vector<MemDim>& dims,
                   std::uint32_t width, std::optional<std::int64_t> start,
                   std::optional<std::int64_t> finish, bool twoState,
                   std::span<const std::vector<W4>> items) {
    MemLoad out;
    const Range range = rangeOf(dims, start, finish);
    const std::int64_t lo = std::min(dims[0].left, dims[0].right);
    const std::int64_t hi = std::max(dims[0].left, dims[0].right);
    const std::uint64_t inner = innerSize(dims);
    const std::uint32_t chunks = (width + 63) / 64;
    std::int64_t at = range.from;
    std::uint64_t below = 0;  // the place under address `at`
    std::uint64_t numbers = 0;
    bool addressed = false;
    bool loading = true;  // false past the end of the range: words are counted, not loaded
    std::size_t line = 1;
    const auto stop = [&](std::string& why, std::string text) {
        why = std::move(text);
        return out;
    };
    for (std::size_t i = 0; i < text.size();) {
        const char c = text[i];
        if (c == '\n') ++line;
        if (isSpace(c)) {
            ++i;
            continue;
        }
        if (text.substr(i, 2) == "//") {
            while (i < text.size() && text[i] != '\n')
                ++i;
            continue;
        }
        if (text.substr(i, 2) == "/*") {
            const std::size_t end = text.find("*/", i + 2);
            for (std::size_t k = i; k < std::min(end, text.size()); ++k)
                if (text[k] == '\n') ++line;
            i = end == std::string_view::npos ? text.size() : end + 2;
            continue;
        }
        if (c == '@') {
            // An address: hexadecimal, right after the @ (21.4).
            std::size_t k = i + 1;
            std::int64_t a = 0;
            W4 d{0, 0};
            while (k < text.size() && digitBits(text[k], true, d) && d.b == 0) {
                a = a * 16 + static_cast<std::int64_t>(d.a);
                ++k;
            }
            if (k == i + 1 || (k < text.size() && !isSpace(text[k]) && text[k] != '/'))
                return stop(out.error, "line " + std::to_string(line) +
                                           ": an address is a hexadecimal number right after @");
            if (!range.holds(a))
                return stop(out.range,
                            "address @" + hexText(a) + " in the file is outside the range " +
                                std::to_string(range.from) + " to " + std::to_string(range.to));
            addressed = true;
            at = a;
            below = 0;
            loading = true;
            i = k;
            continue;
        }
        // A number: digits of the radix with x, z and _; no width or radix.
        std::size_t k = i;
        W4 d{0, 0};
        while (k < text.size() && (text[k] == '_' || digitBits(text[k], hex, d)))
            ++k;
        if (k == i || (k < text.size() && !isSpace(text[k]) && text[k] != '/'))
            return stop(out.error, "line " + std::to_string(line) + ": '" +
                                       std::string(1, text[k]) +
                                       "' is not part of a number, an address or a comment");
        ++numbers;
        if (loading && !range.holds(at)) loading = false;  // past the end of the range
        if (loading && (at < lo || at > hi))
            return stop(out.range,
                        "address " + std::to_string(at) + " of the range is outside the memory");
        if (loading) {
            // The digits from the last one, each into its bits; more than
            // the width keep the low ones, fewer leave 0 above (SYSTEM_TASKS.md 7.1).
            MemWord w{elementAt(dims, at, below), std::vector<W4>(chunks, W4{0, 0})};
            const std::uint32_t bits = hex ? 4 : 1;
            std::uint32_t pos = 0;
            for (std::size_t j = k; j-- > i && pos < width;) {
                if (text[j] == '_') continue;
                (void)digitBits(text[j], hex, d);
                for (std::uint32_t b = 0; b < bits && pos < width; ++b, ++pos) {
                    W4& ch = w.value[pos / 64];
                    ch.a |= ((d.a >> b) & 1U) << (pos % 64);
                    ch.b |= ((d.b >> b) & 1U) << (pos % 64);
                }
            }
            // A 2-state memory takes x and z as 0.
            if (twoState)
                for (W4& ch : w.value)
                    ch = {ch.a & ~ch.b, 0};
            // A memory of an enumerated type takes the values of its items
            // only (21.4.2, SYSTEM_TASKS.md 7.1).
            const auto same = [&](const std::vector<W4>& v) {
                for (std::uint32_t n = 0; n < chunks; ++n)
                    if (v[n].a != w.value[n].a || v[n].b != w.value[n].b) return false;
                return true;
            };
            if (!items.empty() && std::none_of(items.begin(), items.end(), same))
                return stop(out.error, "line " + std::to_string(line) + ": " +
                                           std::string(text.substr(i, k - i)) +
                                           " is no value of the enumerated type");
            out.words.push_back(std::move(w));
            if (++below == inner) {
                below = 0;
                at += range.step;
            }
        }
        i = k;
    }
    if (!addressed && numbers != range.size() * inner)
        out.count = "the file has " + std::to_string(numbers) + " words for the " +
                    std::to_string(range.size() * inner) + " of the range " +
                    std::to_string(range.from) + " to " + std::to_string(range.to);
    return out;
}

std::vector<std::uint32_t> dumpOrder(const std::vector<MemDim>& dims,
                                     std::optional<std::int64_t> start,
                                     std::optional<std::int64_t> finish) {
    const Range range = rangeOf(dims, start, finish);
    const std::int64_t lo = std::min(dims[0].left, dims[0].right);
    const std::int64_t hi = std::max(dims[0].left, dims[0].right);
    const std::uint64_t inner = innerSize(dims);
    std::vector<std::uint32_t> out;
    for (std::uint64_t n = 0; n < range.size(); ++n) {
        const std::int64_t a = range.from + static_cast<std::int64_t>(n) * range.step;
        if (a < lo || a > hi) break;  // outside the memory
        for (std::uint64_t b = 0; b < inner; ++b)
            out.push_back(elementAt(dims, a, b));
    }
    return out;
}

std::optional<std::string> readFile(const std::string& name) {
    std::FILE* f = std::fopen(name.c_str(), "rb");
    if (f == nullptr) return std::nullopt;
    std::string text;
    char buf[1 << 14];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        text.append(buf, n);
    std::fclose(f);
    return text;
}

}  // namespace kirimasim::kernel
