// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/model_abi.h"

namespace kirimasim::vm::detail {
/// Compare one generated body against the VM's actual input snapshots and
/// ordered effects. Kernel observers run exactly once, in the reference run.
class NativeCheck {
public:
    struct Budget {
        std::size_t accesses = 0, words = 0, widest = 2;
    };
    explicit NativeCheck(kernel::Store& store) : store_(store) {}
    void reset(Budget budget);
    kirima_status evaluate(kirima_resume_fn fn, std::uint32_t process);
    void endAfterEffect() {
        if (!end_) end_ = expected_.size();
    }
    void read(kernel::SlotId slot, std::uint32_t ordinal);
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
        std::size_t epoch;
        std::size_t offset;
    };
    std::vector<Cell> reads_;
    std::vector<std::uint64_t> words_, scratch_, values_;
    std::size_t readStart_ = 0;
    std::vector<Effect> expected_;
    std::size_t cursor_ = 0;
    std::optional<std::size_t> end_;
    std::optional<std::uint32_t> wrong_;
    std::uint64_t* element(std::uint32_t slot, std::uint32_t elem);
    void record(bool nba, kernel::SlotBits at, const std::uint64_t* value);
};
}  // namespace kirimasim::vm::detail
