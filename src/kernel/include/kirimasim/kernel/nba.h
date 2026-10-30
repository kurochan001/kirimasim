// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Captured NBA payloads (approved event-NBA contract, scheduler items 4/5).
// C++20; no execution-tier or source-language dependency.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

/// A character of a string (SCHEDULER_SPEC.md 13): the byte is written at
/// the index (none when unknown) into the string held when the update
/// applies, as putc would.
struct NbaStringChar {
    std::optional<std::int64_t> index;
    std::uint8_t byte = 0;
};
struct NbaWrite {
    SlotBits target;
    /// Already captured value words, in the target domain's storage form.
    /// The call copies these words; typed handle snapshots are the caller's.
    std::span<const std::uint64_t> value;
    /// With a character, `value` is empty and `target` a whole string element.
    std::optional<NbaStringChar> character{};
};

/// Context-local generation-checked identifier; it must not outlive its Context.
struct NbaTicket {
    std::uint32_t index = kNone;
    std::uint64_t serial = 0;
    friend bool operator==(NbaTicket, NbaTicket) = default;
};
enum class NbaError : std::uint8_t {
    None,
    InvalidOrigin,
    InvalidTarget,
    InvalidValue,
    InvalidTicket,
    InvalidPhase,
    ResourceLimit
};
/// The captured repeat operand, before normalization. Words are little-endian
/// a/b pairs; unused top bits are ignored. A two-state operand must have no b bits.
struct NbaRepeat {
    std::uint32_t width = 1;
    bool isSigned = false;
    bool fourState = true;
    std::span<const W4> words;
};
enum class NbaCountAction : std::uint8_t { Invalid, Ready, Bind };
/// Shared count classification; backends can avoid materializing a skipped
/// control without duplicating signedness, X/Z or padding rules.
NbaCountAction classifyNbaRepeat(NbaRepeat repeat) noexcept;
struct NbaEventTicket {
    std::uint32_t index = kNone;
    std::uint64_t serial = 0;
    friend bool operator==(NbaEventTicket, NbaEventTicket) = default;
};
struct NbaEventRegistration {
    NbaEventTicket ticket;
    NbaError error = NbaError::None;
    explicit operator bool() const { return error == NbaError::None; }
};
struct NbaEventStats {
    std::uint64_t records = 0;
    std::uint64_t live = 0;
    std::uint64_t callbacks = 0;
    std::uint64_t subscriptions = 0;
    std::uint64_t noticeRecords = 0;
    std::uint64_t pendingNotices = 0;
};
struct NbaCapture {
    NbaTicket ticket;
    NbaError error = NbaError::None;
    explicit operator bool() const { return error == NbaError::None; }
};
struct NbaStorageStats {
    std::uint64_t records = 0;
    std::uint64_t live = 0;
    std::uint64_t ready = 0;
};

namespace detail {
/// Decrement one normalized, nonempty positive little-endian countdown.
/// The return value says whether this notification exhausted the count.
constexpr bool decrementNbaCount(std::span<std::uint64_t> remaining) noexcept {
    for (auto& word : remaining) {
        const auto before = word;
        --word;
        if (before != 0) break;
    }
    for (const auto word : remaining)
        if (word != 0) return false;
    return true;
}
/// A pure counter step shared by ordinary and captured NBAs. Zero is the
/// failure sentinel; exhaustion leaves the counter unchanged, never wrapping.
constexpr std::uint64_t takeNbaIssue(std::uint64_t& next) noexcept {
    if (next == 0 || next == UINT64_MAX) return 0;
    return next++;
}
}  // namespace detail
}  // namespace kirimasim::kernel
