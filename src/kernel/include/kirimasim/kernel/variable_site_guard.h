// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstdint>

namespace kirimasim::kernel {

// The watch belongs to the containing Context, even when a caller copies its
// public Store or Counters. Replacing or moving storage revokes existing borrows.
class VariableSiteGuard {
public:
    VariableSiteGuard() = default;
    VariableSiteGuard(const VariableSiteGuard&) noexcept {}
    VariableSiteGuard(VariableSiteGuard&& other) noexcept { other.invalidate(); }
    VariableSiteGuard& operator=(const VariableSiteGuard& other) noexcept {
        if (this != &other) invalidate();
        return *this;
    }
    VariableSiteGuard& operator=(VariableSiteGuard&& other) noexcept {
        invalidate();
        other.invalidate();
        return *this;
    }

    void watch(std::uint64_t* generation) noexcept { generation_ = generation; }
    void invalidate() const noexcept {
        if (generation_) *generation_ = 0;
    }

private:
    std::uint64_t* generation_ = nullptr;
};

}  // namespace kirimasim::kernel
