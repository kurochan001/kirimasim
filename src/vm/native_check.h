// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/signal_abi.h"

namespace kirimasim::vm::detail {
/// Compare one generated body against the VM's actual input snapshots and
/// ordered effects. Kernel observers run exactly once, in the reference run.
class NativeCheck {
public:
    struct Budget {
        bool checkBranches = false;
        std::size_t accesses = 0, words = 0, widest = 2;
    };
    struct Nba {
        kernel::ProcessId process;
        std::uint64_t delay;
        bool operator==(const Nba&) const = default;
    };
    explicit NativeCheck(kernel::Store& store) : store_(store) {}
    void reset(Budget budget, std::span<const kernel::W4> frame,
               kernel::SlotId countedSlot = kernel::kNone);
    void branch(bool interrupted) {
        ++referenceBranches_;
        if (interrupted) interruptedBranch_ = referenceBranches_;
    }
    [[nodiscard]] bool matchesFrame(std::span<const kernel::W4> frame, bool checkBranches) const;
    struct BoundSite {
        std::size_t index;
        kernel::SlotId slot;
        std::uint32_t element;
    };
    void setVariableSites(const kirima_site_binding_v1*, std::span<const BoundSite>,
                          std::size_t count);
    kirima_status evaluate(kirima_resume_fn fn, std::uint32_t process,
                           kirima_site_resume_v1 site = nullptr);
    void endAfterEffect() {
        if (!end_) end_ = expected_.size();
    }
    void read(kernel::SlotId slot, std::uint32_t ordinal);
    void observe(kernel::SlotBits at, const std::uint64_t* value, std::optional<Nba> nba);
    [[nodiscard]] std::optional<std::uint32_t> mismatch() const;

private:
    struct Effect {
        std::optional<Nba> nba;
        std::uint32_t slot, element, lsb, width;
        std::size_t valueOffset;
        std::array<std::uint64_t, 2> old{};
        bool siteValid = false;
        bool equal = false;
    };
    const kirima_site_binding_v1* liveSites_ = nullptr;
    kirima_site_binding_v1 shadowSites_{};
    std::span<const BoundSite> processSites_;
    std::vector<const std::uint64_t*> shadowElements_;
    std::uint64_t shadowWrites_ = 0;
    void prepareSiteEffect();
    kernel::Store& store_;
    std::vector<kernel::W4> frame_;
    std::size_t referenceBranches_ = 0, replayBranches_ = 0, interruptedBranch_ = 0;
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
    struct CountedSlot {
        kernel::SlotId slot = kernel::kNone;
        kernel::W4 initial{}, last{};
        std::uint64_t writes = 0, changes = 0, witnessed = 0;
        bool prepared = false, committed = false;
    } counted_;
    std::uint64_t countedSerial_ = 0;
    std::uint32_t process_ = UINT32_MAX;
    std::uint32_t prepareCounted(std::uint32_t process, std::uint32_t slot,
                                 kirima_counted_slot* token, kirima_w4* initial);
    std::uint32_t recordCounted(kirima_counted_slot token, kirima_w4 value);
    std::uint32_t commitCounted(kirima_counted_slot token, kirima_w4 value, std::uint64_t writes,
                                std::uint64_t changes);
    [[nodiscard]] bool validCounted(kirima_counted_slot token) const;
    std::uint64_t* element(std::uint32_t slot, std::uint32_t elem);
    void record(kernel::SlotBits at, const std::uint64_t* value, std::optional<Nba> nba);
};
}  // namespace kirimasim::vm::detail
