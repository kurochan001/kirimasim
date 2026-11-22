// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace kirimasim::kernel {

/// Fork membership and pending joins (SCHEDULER_SPEC 10). The tier owns
/// its frames and orders cancellation by the birth indices assigned here.
enum class JoinMode : std::uint8_t { None, All, Any, WaitFork };
struct ChildBirth {
    std::uint64_t order, group;
};
class ForkState {
public:
    [[nodiscard]] std::uint64_t births() const noexcept { return births_; }
    [[nodiscard]] bool waiting() const noexcept { return pending_ != JoinMode::None; }
    void cancelJoin() noexcept { pending_ = JoinMode::None; }
    void waitFork() noexcept { pending_ = JoinMode::WaitFork; }
    [[nodiscard]] bool canFork(JoinMode mode, std::size_t count) const noexcept {
        return count <= UINT32_MAX - live_ && count <= UINT64_MAX - births_ &&
               (mode == JoinMode::None || group_ < UINT64_MAX);
    }
    void begin(JoinMode mode, std::uint32_t count) noexcept {
        if (mode == JoinMode::None) return;
        ++group_;
        done_ = 0;
        size_ = count;
        pending_ = mode;
    }
    ChildBirth born(JoinMode mode) noexcept {
        ++live_;
        return {births_++, mode == JoinMode::None ? 0 : group_};
    }
    void ended(ChildBirth child) noexcept {
        --live_;
        if (child.group && child.group == group_) ++done_;
    }
    /// A zero-child join, including join_any, completes immediately.
    bool continueAfterJoin() noexcept {
        switch (pending_) {
            case JoinMode::All:
                if (done_ < size_) return false;
                break;
            case JoinMode::Any:
                if (!done_ && size_) return false;
                break;
            case JoinMode::WaitFork:
                if (live_) return false;
                break;
            case JoinMode::None: break;
        }
        pending_ = JoinMode::None;
        return true;
    }

private:
    std::uint64_t births_ = 0, group_ = 0;
    std::uint32_t live_ = 0, size_ = 0, done_ = 0;
    JoinMode pending_ = JoinMode::None;
};

/// SCHEDULER_SPEC 6.1 steps 4 through 7. Preserve declaration order within each set.
template <class Set, class Start>
void startTimeZero(const Set& continuous, const Set& always, const Set& initial,
                   const Set& combinational, Start start) {
    for (const auto* group : {&continuous, &always, &initial, &combinational})
        for (const auto& process : *group)
            start(process);
}
}  // namespace kirimasim::kernel
