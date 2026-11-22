// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/model_abi.h"

namespace kirimasim::vm::detail {
/// Evaluate the emitted scalar body against a private overlay. A blocking
/// write is visible to its later loads. An NBA only enters the ordered log.
class NativeCheck {
public:
    struct Budget {
        std::size_t accesses = 0, words = 0, widest = 2;
    };
    NativeCheck(kernel::Store& store, Budget budget);
    kirima_status evaluate(kirima_resume_fn fn, std::uint32_t process);
    void observe(bool nba, kernel::SlotBits at, const std::uint64_t* value);
    [[nodiscard]] std::optional<std::uint32_t> mismatch() const;

private:
    struct Effect {
        bool nba;
        std::uint32_t slot, element, lsb, width;
        std::size_t valueOffset;
    };
    kernel::Store& store_;
    struct Cell {
        std::uint32_t slot, ordinal;
        std::size_t offset;
    };
    std::vector<Cell> overlay_;
    std::vector<std::uint64_t> words_, scratch_, values_;
    std::size_t used_ = 0, saved_ = 0;
    std::vector<Effect> expected_;
    std::size_t cursor_ = 0;
    std::optional<std::uint32_t> wrong_;
    std::uint64_t* element(std::uint32_t slot, std::uint32_t elem);
    void record(bool nba, kernel::SlotBits at, const std::uint64_t* value);
};
}  // namespace kirimasim::vm::detail
