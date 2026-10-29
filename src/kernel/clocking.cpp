// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/sim.h"

#include <algorithm>
#include <cstddef>
#include <new>
#include <stdexcept>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {

ClockingError Scheduler::addClockingBlock(WaitItem clock, EventId event,
                                          std::span<const ClockingInput> inputs) noexcept {
    // Registration belongs to setup, after static initialization, before run.
    const auto valid = [&](SlotBits at) {
        if (at.slot.index() >= store_.size()) return false;
        const auto& l = store_.layout(at.slot);
        const auto width = at.width == 0 ? l.width : at.width;
        return (l.domain == Domain::Two || l.domain == Domain::Four) && at.elem < l.elements &&
               at.lsb < l.width && width <= l.width - at.lsb && (at.width != 0 || at.lsb == 0);
    };
    const auto& part = clock.part;
    if (clockingSealed_ || clock.guard != nullptr || clock.guardFrame != nullptr ||
        event.index() >= events_.size() || part.numElems > 1 ||
        !valid({part.slot, part.firstElem, part.lsb, part.width}) ||
        (part.numElems == 0 && store_.layout(part.slot).elements != 1) ||
        static_cast<unsigned>(clock.edge) > static_cast<unsigned>(Edge::Both))
        return ClockingError::Invalid;
    for (const auto& block : clocking_)
        if (block.event == event)
            return ClockingError::Invalid;
        else
            for (const auto& in : block.inputs)
                if (in.at.target.slot == part.slot) return ClockingError::Invalid;
    const auto widthOf = [&](SlotBits at) {
        return at.width == 0 ? store_.layout(at.slot).width : at.width;
    };
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const auto& in = inputs[i];
        if (!valid(in.source) || !valid(in.target)) return ClockingError::Invalid;
        const auto& src = store_.layout(in.source.slot);
        const auto& dst = store_.layout(in.target.slot);
        if (src.domain != dst.domain || widthOf(in.source) != widthOf(in.target) ||
            in.target.elem != 0 || in.target.lsb != 0 || dst.elements != 1 ||
            widthOf(in.target) != dst.width || in.target.slot == part.slot)
            return ClockingError::Invalid;
        for (std::size_t j = 0; j < inputs.size(); ++j)
            if ((i != j && inputs[j].target.slot == in.target.slot) ||
                inputs[j].source.slot == in.target.slot)
                return ClockingError::Invalid;
        for (const auto& block : clocking_) {
            if (block.clock.part.slot == in.target.slot) return ClockingError::Invalid;
            for (const auto& other : block.inputs)
                if (other.at.target.slot == in.target.slot ||
                    other.at.source.slot == in.target.slot ||
                    other.at.target.slot == in.source.slot || other.at.target.slot == part.slot)
                    return ClockingError::Invalid;
        }
    }
    try {
        auto* memory = ctx_.heap().memoryResource();
        ClockingState state{clock, event, std::pmr::vector<ClockingState::Input>(memory)};
        state.inputs.reserve(inputs.size());
        for (const auto& in : inputs) {
            std::pmr::vector<std::uint64_t> sample(memory);
            const auto stride = store_.layout(in.source.slot).domain == Domain::Four ? 2U : 1U;
            sample.resize(std::size_t{wordsFor(widthOf(in.source))} * stride);
            state.inputs.push_back({in, std::move(sample)});
        }
        // Only the final move publishes registration; failures leave no watcher.
        clocking_.push_back(std::move(state));
        return ClockingError::None;
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
    return ClockingError::ResourceLimit;
}

bool Scheduler::hasClocking(SlotId slot) const noexcept {
    for (const auto& block : clocking_)
        if (block.clock.part.slot == slot) return true;
    return false;
}

void Scheduler::sampleClocking() noexcept {
    if (clocking_.empty() || (clockingSampled_ && clockingSampleTime_ == time_)) return;
    clockingSampled_ = true;
    clockingSampleTime_ = time_;
    for (auto& block : clocking_)
        for (auto& in : block.inputs) {
            const auto& src = in.at.source;
            const auto& l = store_.layout(src.slot);
            const auto width = src.width == 0 ? l.width : src.width;
            const unsigned stride = l.domain == Domain::Four ? 2U : 1U;
            for (unsigned plane = 0; plane < stride; ++plane)
                copyBits(in.sample.data() + plane, stride, 0,
                         store_.element(src.slot, src.elem) + plane, stride, src.lsb, width);
        }
}

void Scheduler::publishClocking(std::uint32_t id) noexcept {
    const auto savedWalking = walking_;
    const auto savedDraining = draining_;
    const auto limited = [&]() noexcept {
        walking_ = savedWalking;
        draining_ = savedDraining;
        if (savedWalking == 0 && !savedDraining) lateChanges_.clear();
        resourceLimit();
    };
    try {
        ensureSlotTables();
        std::pmr::vector<LateChange> notices(ctx_.heap().memoryResource());
        notices.reserve(clocking_[id].inputs.size());
        // Prepare every notification before any destination changes.
        for (const auto& in : clocking_[id].inputs) {
            const auto& dst = in.at.target;
            const auto& l = store_.layout(dst.slot);
            const auto* old = store_.element(dst.slot, 0);
            if (std::equal(in.sample.begin(), in.sample.end(), old)) continue;
            auto& notice = notices.emplace_back();
            notice.slot = dst.slot;
            notice.width = l.width;
            notice.old.assign(old, old + l.elemWords);
            notice.now.assign(in.sample.begin(), in.sample.end());
        }
        lateChanges_.reserve(lateChanges_.size() + notices.size());
        for (const auto& notice : notices)
            std::copy(notice.now.begin(), notice.now.end(), store_.element(notice.slot, 0));
        counters_.increment(cWrites_, clocking_[id].inputs.size());
        counters_.increment(cChanges_, notices.size());
        if (ctx_.dump().glitches())
            for (const auto& notice : notices)
                ctx_.dump().changed(notice.slot, time_);
        beginWalk();
        for (auto& notice : notices)
            lateChanges_.push_back(std::move(notice));
        endWalk();
        if (!ending_) trigger(clocking_[id].event);
        return;
    } catch (const std::bad_alloc&) {
        limited();
    } catch (const std::length_error&) {
        limited();
    }
}

}  // namespace kirimasim::kernel
