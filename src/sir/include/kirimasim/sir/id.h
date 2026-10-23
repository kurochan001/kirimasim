// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Distinct types for the numbers of the SIR's tables. A slot number, a
// block number and a frame variable number cannot be given for one
// another, and none of them is an integer: using one as an offset or as
// another kind of number does not compile (issues #1 and #3 were such
// mix-ups). The kernel has a template of its own: neither library depends
// on the other (CLAUDE.md, rules of library dependencies).

#pragma once

#include <compare>
#include <cstdint>

namespace kirimasim::sir {

/// "No row", for the numbers of every table. It converts to an Id of any
/// kind, and to the plain number 0xffffffff that other rows still use.
struct None {
    // Implicit on purpose: `x == kNone` for a plain number.
    constexpr operator std::uint32_t() const noexcept { return 0xffffffffU; }
};
inline constexpr None kNone{};

/// A row of one table of the SIR; Tag names the table.
template <class Tag> class Id {
public:
    constexpr Id() = default;
    constexpr explicit Id(std::uint32_t v) noexcept : v_(v) {}
    constexpr Id(None) noexcept : v_(kNone) {}  // Implicit on purpose: `SlotId s = kNone;`

    /// The number, to index the table this Id is a row of.
    [[nodiscard]] constexpr std::uint32_t index() const { return v_; }

    friend constexpr bool operator==(Id, Id) = default;
    friend constexpr auto operator<=>(Id, Id) = default;
    friend constexpr bool operator==(Id a, None) { return a.v_ == kNone; }

private:
    std::uint32_t v_ = 0;
};

}  // namespace kirimasim::sir
