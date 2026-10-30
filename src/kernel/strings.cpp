// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/strings.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

namespace kirimasim::kernel {

std::string putChar(std::string_view text, std::optional<std::int64_t> index, std::uint8_t byte) {
    std::string result(text);
    if (index && *index >= 0 && static_cast<std::uint64_t>(*index) < result.size() && byte != 0)
        result[static_cast<std::size_t>(*index)] = static_cast<char>(byte);
    return result;
}

std::optional<double> stringToReal(std::string_view text) noexcept try {
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    std::string number;
    std::size_t pos = 0;
    // One leading sign, which a digit must follow (VALUE_SEMANTICS.md 7.6).
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) number += text[pos++];
    if (pos == text.size() || !digit(text[pos])) return 0;
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
            while (pos < exponent)
                number += text[pos++];
            digits();
        }
    }
    return std::strtod(number.c_str(), nullptr);
} catch (const std::bad_alloc&) {
    return std::nullopt;
} catch (const std::length_error&) {
    return std::nullopt;
}

void Strings::Pin::reset() noexcept {
    if (owner_ != nullptr && handle_ != 0) {
        auto& entry = owner_->texts_[static_cast<std::uint32_t>(handle_) - 1];
        --entry.pins;
    }
    owner_ = nullptr;
    handle_ = 0;
}
StringError Strings::pin(std::uint64_t handle, Pin& output) noexcept {
    if (handle != 0) {
        const auto low = static_cast<std::uint32_t>(handle);
        if (low == 0 || low > texts_.size()) return StringError::InvalidHandle;
        auto& entry = texts_[low - 1];
        if (!entry.live || entry.generation != handle >> 32) return StringError::InvalidHandle;
        if (entry.pins == UINT64_MAX) return StringError::ResourceLimit;
        ++entry.pins;
        if (collecting_) entry.marked = true;
    }
    output.reset();
    output.owner_ = this;
    output.handle_ = handle;
    return StringError::None;
}
std::uint32_t stringToInteger(std::string_view text, unsigned base) noexcept {
    const auto digit = [base](char c) {  // base when c is not a digit of base
        unsigned d = base;
        if (c >= '0' && c <= '9')
            d = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            d = static_cast<unsigned>(c - 'A' + 10);
        return d < base ? d : base;
    };
    std::size_t pos = 0;
    const bool negative = !text.empty() && text.front() == '-';
    if (negative) ++pos;
    std::uint32_t n = 0;
    for (; pos < text.size(); ++pos) {
        if (text[pos] == '_') continue;
        const unsigned d = digit(text[pos]);
        if (d == base) break;
        n = n * base + d;  // Wraps: the low 32 bits (6.16.9).
    }
    return negative ? 0U - n : n;
}

std::uint64_t Strings::intern(std::string_view s) {
    if (s.empty()) return 0;
    if (auto it = handles_.find(s); it != handles_.end()) return it->second;
    if (collecting_ || s.size() > UINT64_MAX - bytes_) throw std::bad_alloc{};
    std::pmr::string value(s, memory_);
    const bool grow = freeCount_ == 0;
    std::uint32_t index = 0;
    if (grow) {
        if (texts_.size() >= UINT32_MAX) throw std::bad_alloc{};
        if (free_.size() < texts_.size() + 1)
            free_.resize(std::max(texts_.size() + 1, free_.size() * 2));
        index = static_cast<std::uint32_t>(texts_.size());
        texts_.emplace_back(memory_);
    } else {
        index = free_[freeCount_ - 1];
    }
    auto& entry = texts_[index];
    const auto handle = (std::uint64_t{entry.generation} << 32) | (std::uint64_t{index} + 1);
    try {
        handles_.emplace(value, handle);
    } catch (...) {
        if (grow) texts_.pop_back();
        throw;
    }
    entry.text = std::move(value);
    entry.live = true;
    entry.marked = false;
    if (!grow) --freeCount_;
    ++live_;
    bytes_ += entry.text.size();
    return handle;
}

StringError Strings::beginCollection(std::span<const std::uint64_t> roots) noexcept {
    if (collecting_) return StringError::InvalidState;
    for (auto& entry : texts_)
        entry.marked = entry.pins != 0;
    collecting_ = true;
    failed_ = false;
    for (const auto root : roots) {
        if (const auto error = mark(root); error != StringError::None) {
            cancelCollection();
            return error;
        }
    }
    return StringError::None;
}
StringError Strings::mark(std::uint64_t handle) noexcept {
    if (!collecting_) return StringError::InvalidState;
    if (handle == 0) return StringError::None;
    const auto low = static_cast<std::uint32_t>(handle);
    if (low == 0 || low > texts_.size()) {
        failed_ = true;
        return StringError::InvalidHandle;
    }
    auto& entry = texts_[low - 1];
    if (!entry.live || entry.generation != handle >> 32) {
        failed_ = true;
        return StringError::InvalidHandle;
    }
    entry.marked = true;
    return StringError::None;
}
void Strings::finishCollection() noexcept {
    if (!collecting_) return;
    if (failed_) {
        cancelCollection();
        return;
    }
    for (std::uint32_t i = 0; i < texts_.size(); ++i) {
        auto& entry = texts_[i];
        if (!entry.live || entry.marked) continue;
        handles_.erase(entry.text);
        --live_;
        bytes_ -= entry.text.size();
        std::pmr::string empty(memory_);
        entry.text.swap(empty);
        entry.live = false;
        if (entry.generation == UINT32_MAX) continue;
        ++entry.generation;
        free_[freeCount_++] = i;
    }
    const auto twice = [](std::uint64_t n) { return n > UINT64_MAX / 2 ? UINT64_MAX : n * 2; };
    nextObjects_ = std::max(std::uint64_t{256}, twice(live_));
    nextBytes_ = std::max(std::uint64_t{65536}, twice(bytes_));
    collecting_ = false;
}

}  // namespace kirimasim::kernel
