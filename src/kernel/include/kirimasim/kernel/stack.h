// The stack of the thread that runs a design, so that a run stops before
// it runs out (RUNTIME_ABI.md 5): the count of nested calls is the limit
// of the design, and this is the limit of the thread, whatever thread
// that is and whatever the build. front has a twin of this (src/front/
// stack.h): neither library depends on the other (CLAUDE.md, rules of
// library dependencies).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstddef>
#include <cstdint>

namespace kirimasim::kernel {

class StackRange {
public:
    /// What is left at the place that checks must be more than this.
    static constexpr std::size_t kMargin = std::size_t{1} << 20;

    /// No range: nearlyUsedUp() is always false, and only the count of
    /// calls applies.
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

}  // namespace kirimasim::kernel
