// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Distinct types for the numbers the kernel hands out and takes: slots,
// processes, events and counters cannot be given for one another, and none
// of them is an integer. The SIR has a template of its own (sir/id.h): the
// kernel depends on nothing in kirimasim (CLAUDE.md, rules of library
// dependencies). The C interface of abi.h keeps plain numbers; abi.cpp
// turns them into these. C++20, like every public kernel header.

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace kirimasim::kernel {

/// "No row", for the numbers of every kernel table. It converts to an Id of
/// any kind, and to the plain number 0xffffffff that other rows still use.
struct None {
    // Implicit on purpose: `x == kNone` for a plain number.
    constexpr operator std::uint32_t() const { return 0xffffffffU; }
};
inline constexpr None kNone{};

/// A row of one table of the kernel; Tag names the table.
template <class Tag> class Id {
public:
    constexpr Id() = default;
    constexpr explicit Id(std::uint32_t v) : v_(v) {}
    constexpr Id(None) : v_(kNone) {}  // Implicit on purpose: `ProcessId p = kNone;`

    /// The number, to index the table this Id is a row of.
    [[nodiscard]] constexpr std::uint32_t index() const { return v_; }

    friend constexpr bool operator==(Id, Id) = default;
    friend constexpr auto operator<=>(Id, Id) = default;
    friend constexpr bool operator==(Id a, None) { return a.v_ == kNone; }

private:
    std::uint32_t v_ = 0;
};

}  // namespace kirimasim::kernel

template <class Tag> struct std::hash<kirimasim::kernel::Id<Tag>> {
    std::size_t operator()(kirimasim::kernel::Id<Tag> i) const noexcept {
        return std::hash<std::uint32_t>{}(i.index());
    }
};
