// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Commit complete net snapshots before either internal or ordinary notices
// (approved MOS/switch contract 6/7).
#include "kirimasim/kernel/context.h"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <vector>

namespace kirimasim::kernel {
StrengthError Scheduler::enableStrengths(SlotId slot,
                                         std::span<const StrengthRange> initial) noexcept {
    if (slot.index() < heldOf_.size() && heldOf_[slot.index()] != kNotHeld)
        return StrengthError::UnsupportedHold;
    return ctx_.strengths().enable(slot, initial);
}

StrengthPublication Scheduler::publishStrengths(std::span<const StrengthUpdate> updates) noexcept {
    auto& storage = ctx_.strengths();
    if (storage.publishing_) return {StrengthError::Reentrant};
    struct Reset {
        bool& flag;
        ~Reset() { flag = false; }
    } reset{storage.publishing_};
    storage.publishing_ = true;
    const auto savedWalking = walking_;
    const auto savedDraining = draining_;
    bool committed = false, changed = false;
    const auto limited = [&]() noexcept {
        walking_ = savedWalking;
        draining_ = savedDraining;
        if (savedWalking == 0 && !savedDraining) lateChanges_.clear();
        resourceLimit();
        return StrengthPublication{StrengthError::ResourceLimit, committed, changed};
    };
    try {
        begin();
        ensureSlotTables();
        StrengthStorage::Plan plan(storage.memory_);
        const auto status = storage.prepare(updates, plan);
        if (status != StrengthError::None) return {status};
        std::vector<LateChange> notices;
        notices.reserve(plan.parts.size());
        // Stage every ordinary notification before any published bit changes.
        for (const auto& part : plan.parts) {
            changed = changed || part.rangeChanged;
            if (!part.valueChanged) continue;
            const auto slot = part.at.slot.index();
            const bool watched = !subscriptions_[slot].empty() || !slotMonitors_[slot].empty() ||
                                 monitorSlot_[slot] != 0;
            if (!watched) continue;
            const auto& element = plan.elements[part.element];
            auto& notice = notices.emplace_back();
            notice.slot = part.at.slot;
            notice.elem = part.at.elem;
            notice.lsb = part.at.lsb;
            notice.width = part.at.width;
            notice.old.assign(element.before.begin(), element.before.end());
            notice.now.assign(element.after.begin(), element.after.end());
        }
        if (notices.size() > lateChanges_.max_size() - lateChanges_.size())
            throw std::length_error("strength notice capacity");
        lateChanges_.reserve(lateChanges_.size() + notices.size());
        storage.commit(plan);
        committed = true;
        for (const auto& part : plan.parts) {
            counters_.increment(cWrites_);
            if (part.valueChanged) counters_.increment(cChanges_);
        }
        if (ctx_.dump().glitches())
            for (const auto& element : plan.elements)
                if (element.before != element.after) ctx_.dump().changed(element.slot, time_);
        beginWalk();
        for (auto& notice : notices)
            lateChanges_.push_back(std::move(notice));
        storage.announce(plan, ending_);
        endWalk();
        return {StrengthError::None, true, changed};
    } catch (const std::bad_alloc&) {
        return limited();
    } catch (const std::length_error&) {
        return limited();
    }
}
}  // namespace kirimasim::kernel
