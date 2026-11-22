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
    for (const auto& row : sampled_)
        if (row.current == part.slot || (row.history && row.history->previous == part.slot))
            return ClockingError::Invalid;
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
        for (const auto& row : sampled_) {
            if (row.source.slot == in.target.slot || row.current == in.target.slot ||
                row.current == in.source.slot ||
                (row.history && (row.history->previous == in.target.slot ||
                                 row.history->previous == in.source.slot ||
                                 row.history->clock.part.slot == in.target.slot)))
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
        outOfMemory();
    } catch (const std::length_error&) {
        outOfMemory();
    }
    return ClockingError::ResourceLimit;
}

ClockingError Scheduler::addSampledInput(SlotBits source, SlotId current,
                                         std::span<const std::uint64_t> initial,
                                         std::optional<SampleHistory> history) noexcept {
    if (clockingSealed_ || source.slot.index() >= store_.size() || current.index() >= store_.size())
        return ClockingError::Invalid;
    const auto& src = store_.layout(source.slot);
    const auto width = source.width == 0 ? src.width : source.width;
    const auto target = [&](SlotId id) {
        if (id.index() >= store_.size() || id == source.slot) return false;
        const auto& dst = store_.layout(id);
        return dst.width == width && dst.domain == src.domain && dst.elements == 1;
    };
    if ((src.domain != Domain::Two && src.domain != Domain::Four) || !target(current) ||
        source.elem >= src.elements || source.lsb >= src.width || width > src.width - source.lsb ||
        initial.size() != store_.layout(current).elemWords)
        return ClockingError::Invalid;
    const auto writes = [&](SlotId id) {
        return id == current || (history && id == history->previous);
    };
    const auto collides = [&](SlotId dst) {
        return writes(dst) || dst == source.slot || (history && dst == history->clock.part.slot);
    };
    if (history) {
        const auto& clock = history->clock;
        const auto& part = clock.part;
        if (!target(history->previous) || history->previous == current || writes(part.slot) ||
            clock.guard || clock.guardFrame ||
            (clock.edge != Edge::Pos && clock.edge != Edge::Neg) ||
            part.slot.index() >= store_.size())
            return ClockingError::Invalid;
        const auto& l = store_.layout(part.slot);
        if ((l.domain != Domain::Two && l.domain != Domain::Four) || l.elements != 1 ||
            part.firstElem != 0 || part.numElems > 1 || part.lsb >= l.width ||
            (part.width == 0 ? l.width : part.width) != 1)
            return ClockingError::Invalid;
    }
    for (const auto& row : sampled_) {
        if (writes(row.source.slot) || collides(row.current) ||
            (row.history &&
             (writes(row.history->clock.part.slot) || collides(row.history->previous))))
            return ClockingError::Invalid;
    }
    for (const auto& block : clocking_) {
        if (writes(block.clock.part.slot)) return ClockingError::Invalid;
        for (const auto& in : block.inputs)
            if (writes(in.at.source.slot) || collides(in.at.target.slot))
                return ClockingError::Invalid;
    }
    try {
        std::pmr::vector<std::uint64_t> scratch(initial.size(), 0, ctx_.heap().memoryResource());
        if (history) {
            const auto slot = history->clock.part.slot.index();
            if (samplesByClock_.size() <= slot) samplesByClock_.resize(std::size_t{slot} + 1);
            auto& watchers = samplesByClock_[slot];
            watchers.reserve(watchers.size() + 1);
        }
        sampled_.push_back({source, current, history, std::move(scratch)});
        if (history)
            samplesByClock_[history->clock.part.slot.index()].push_back(sampled_.size() - 1);
        std::ranges::copy(initial, store_.element(current, 0));
        if (history) std::ranges::copy(initial, store_.element(history->previous, 0));
        return ClockingError::None;
    } catch (const std::bad_alloc&) {
        outOfMemory();
    } catch (const std::length_error&) {
        outOfMemory();
    }
    return ClockingError::ResourceLimit;
}

void Scheduler::prepareSampleChange(std::pmr::vector<LateChange>& changes, SlotId target,
                                    std::span<const std::uint64_t> value) {
    const auto* old = store_.element(target, 0);
    if (std::equal(value.begin(), value.end(), old)) return;
    auto& change = changes.emplace_back();
    change.slot = target;
    change.width = store_.layout(target).width;
    change.old.assign(old, old + value.size());
    change.now.assign(value.begin(), value.end());
}

void Scheduler::publishSampleChanges(std::pmr::vector<LateChange>& changes, std::size_t writes) {
    lateChanges_.reserve(lateChanges_.size() + changes.size());
    for (const auto& change : changes)
        std::ranges::copy(change.now, store_.element(change.slot, 0));
    counters_.increment(cWrites_, writes);
    counters_.increment(cChanges_, changes.size());
    if (tracingRace())
        for (const auto& change : changes)
            observeRaceSlot(change.slot, 0);
    if (ctx_.dump().glitches())
        for (const auto& change : changes)
            ctx_.dump().changed(change.slot, time_);
    const auto savedWalking = walking_;
    const auto savedDraining = draining_;
    beginWalk();
    for (auto& change : changes)
        lateChanges_.push_back(std::move(change));
    try {
        endWalk();
    } catch (...) {
        walking_ = savedWalking;
        draining_ = savedDraining;
        if (savedWalking == 0 && !savedDraining) lateChanges_.clear();
        throw;
    }
}

void Scheduler::sampleInputs() {
    if (sampledTime_ == time_) return;
    sampledTime_ = time_;
    // Prepare all new values and notices before publishing any one of them.
    // Snapshot destinations are private; a qualifying edge only marks the
    // current sample for promotion on the next distinct simulation time.
    try {
        std::pmr::vector<LateChange> changes(ctx_.heap().memoryResource());
        std::size_t writes = sampled_.size();
        for (auto& row : sampled_) {
            if (row.ticked && row.history) {
                ++writes;
                prepareSampleChange(changes, row.history->previous,
                                    {store_.element(row.current, 0), row.scratch.size()});
            }
            const auto stride = store_.layout(row.source.slot).domain == Domain::Four ? 2U : 1U;
            for (unsigned plane = 0; plane < stride; ++plane)
                copyBits(row.scratch.data() + plane, stride, 0,
                         store_.element(row.source.slot, row.source.elem) + plane, stride,
                         row.source.lsb, store_.layout(row.current).width);
            prepareSampleChange(changes, row.current, row.scratch);
            row.ticked = false;
        }
        publishSampleChanges(changes, writes);
    } catch (const std::bad_alloc&) {
        outOfMemory();
    } catch (const std::length_error&) {
        resourceLimit();
    }
}

bool Scheduler::hasClocking(SlotId slot) const noexcept {
    for (const auto& block : clocking_)
        if (block.clock.part.slot == slot) return true;
    return slot.index() < samplesByClock_.size() && !samplesByClock_[slot.index()].empty();
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
        for (const auto& in : clocking_[id].inputs)
            prepareSampleChange(notices, in.at.target.slot, in.sample);
        publishSampleChanges(notices, clocking_[id].inputs.size());
        if (!ending_) trigger(clocking_[id].event);
        return;
    } catch (const std::bad_alloc&) {
        limited();
    } catch (const std::length_error&) {
        limited();
    }
}

}  // namespace kirimasim::kernel
