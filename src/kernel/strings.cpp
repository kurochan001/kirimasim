// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/strings.h"

#include <cstdlib>
#include <new>
#include <stdexcept>

namespace kirimasim::kernel {

std::optional<double> stringToReal(std::string_view text) noexcept try {
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    if (text.empty() || !digit(text.front())) return 0;
    std::string number;
    std::size_t pos = 0;
    const auto digits = [&] {
        while (pos < text.size() && (digit(text[pos]) || text[pos] == '_')) {
            if (text[pos] != '_') number += text[pos];
            ++pos;
        }
    };
    digits();
    if (pos + 1 < text.size() && text[pos] == '.' && digit(text[pos + 1])) {
        number += text[pos++];
        digits();
    }
    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
        auto exponent = pos + 1;
        if (exponent < text.size() && (text[exponent] == '+' || text[exponent] == '-')) ++exponent;
        if (exponent < text.size() && digit(text[exponent])) {
            number.append(text.substr(pos, exponent - pos));
            pos = exponent;
            digits();
        }
    }
    return std::strtod(number.c_str(), nullptr);
} catch (const std::bad_alloc&) {
    return std::nullopt;
} catch (const std::length_error&) {
    return std::nullopt;
}

std::uint64_t Strings::intern(std::string_view s) {
    if (s.empty()) return 0;
    std::string key(s);
    if (auto it = handles_.find(key); it != handles_.end()) return it->second;
    texts_.push_back(key);
    const auto h = static_cast<std::uint64_t>(texts_.size());
    handles_.emplace(std::move(key), h);
    return h;
}

}  // namespace kirimasim::kernel
