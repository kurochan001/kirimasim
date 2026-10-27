// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Captured NBA payloads (approved event-NBA contract, scheduler items 4/5).
// C++20; no execution-tier or source-language dependency.
#pragma once

#include <cstdint>
#include <span>

#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/store.h"

namespace kirimasim::kernel {

struct NbaWrite {
    SlotBits target;
    /// Already captured value words, in the target domain's storage form.
    /// The call copies these words; typed handle snapshots are the caller's.
    std::span<const std::uint64_t> value;
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
/// A pure counter step shared by ordinary and captured NBAs. Zero is the
/// failure sentinel; exhaustion leaves the counter unchanged, never wrapping.
constexpr std::uint64_t takeNbaIssue(std::uint64_t& next) noexcept {
    if (next == 0 || next == UINT64_MAX) return 0;
    return next++;
}
}  // namespace detail
}  // namespace kirimasim::kernel
