// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The stack of the thread that runs front, so that lowering stops with a
// diagnostic before it runs out (RUNTIME_ABI.md 5). The twin of the
// kernel's (kirimasim/kernel/stack.h): neither library depends on the
// other (CLAUDE.md, rules of library dependencies).

#pragma once

#include <cstddef>
#include <cstdint>

namespace kirimasim::front {

class StackRange {
public:
    /// What is left at the place that checks must be more than this.
    static constexpr std::size_t kMargin = std::size_t{1} << 20;

    /// No range: nearlyUsedUp() is always false.
    StackRange() = default;

    /// The stack of the calling thread, read from the system (macOS and
    /// Linux); no range where it cannot be read.
    static StackRange ofThisThread();

    /// Whether kMargin or less is left below the caller's frame. The
    /// stack grows down on every system kirimasim runs on. False with no
    /// range, and on a thread whose stack this is not.
    [[nodiscard]] bool nearlyUsedUp() const {
        // The frame itself, not a local's address, which AddressSanitizer
        // may move to a stack of its own.
        const auto here = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
        if (here <= low_ || here > high_) return false;
        return here - low_ <= kMargin;
    }

private:
    StackRange(std::uintptr_t low, std::uintptr_t high) : low_(low), high_(high) {}
    std::uintptr_t low_ = 0;   ///< The lowest address of the stack.
    std::uintptr_t high_ = 0;  ///< One past its highest.
};

}  // namespace kirimasim::front
