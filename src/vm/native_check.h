// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/model_abi.h"
#include <array>

namespace kirimasim::vm::detail {
/// Evaluate the emitted scalar body against a private overlay. A blocking
/// write is visible to its later loads. An NBA only enters the ordered log.
class NativeCheck {
public:
    NativeCheck(kernel::Store& store, std::size_t maxAccesses);
    kirima_status evaluate(kirima_resume_fn fn, std::uint32_t process);
    void observe(bool nba, kernel::SlotBits at, kernel::W4 value);
    [[nodiscard]] std::optional<std::uint32_t> mismatch() const;

private:
    struct Effect {
        bool nba;
        std::uint32_t slot, element, lsb, width;
        std::uint64_t a, b;
        bool operator==(const Effect&) const = default;
    };
    using Word = std::array<std::uint64_t, 2>;
    kernel::Store& store_;
    struct Cell {
        std::uint32_t slot, ordinal;
        Word words;
    };
    std::vector<Cell> overlay_;
    Word scratch_{};
    std::vector<Effect> expected_;
    std::size_t cursor_ = 0;
    std::optional<std::uint32_t> wrong_;
    Word& element(std::uint32_t slot, std::uint32_t elem);
    Effect effect(bool nba, kernel::SlotBits at, kernel::W4 v) const;
    void record(bool nba, kernel::SlotBits at, const std::uint64_t* value);
};
}  // namespace kirimasim::vm::detail
