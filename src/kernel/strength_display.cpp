// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace kirimasim::kernel {
bool Scheduler::trackDisplayStrength(SlotBits part) noexcept {
    if (!displayStrengthReads_) return true;
    try {
        if (std::none_of(displayStrengthReads_->begin(), displayStrengthReads_->end(),
                         [&](const auto& old) {
                             return old.slot == part.slot && old.elem == part.elem &&
                                    old.lsb == part.lsb && old.width == part.width;
                         }))
            displayStrengthReads_->push_back(part);
        return true;
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
    return false;
}
void Scheduler::displayStrengthChanged(void* frame, SlotBits at,
                                       std::span<const StrengthRange> before,
                                       std::span<const StrengthRange> after) noexcept {
    auto& scheduler = *static_cast<Scheduler*>(frame);
    const auto mark = [&](DisplayMonitor& monitor) {
        for (const auto& part : monitor.strengthReads) {
            if (part.slot != at.slot || part.elem != at.elem) continue;
            const auto first = std::max<std::uint64_t>(part.lsb, at.lsb);
            const auto end = std::min(std::uint64_t{part.lsb} + part.width,
                                      std::uint64_t{at.lsb} + before.size());
            for (auto bit = first; bit < end; ++bit) {
                const auto i = static_cast<std::size_t>(bit - at.lsb);
                if (before[i].lo != after[i].lo || before[i].hi != after[i].hi) {
                    monitor.due = true;
                    return;
                }
            }
        }
    };
    mark(scheduler.monitor_);
    for (auto& monitor : scheduler.fileMonitors_)
        mark(monitor);
}
void Scheduler::runDisplayMonitor(std::uint64_t serial, ProcessId process, const FileTargets* files,
                                  DisplayReadSpec spec) {
    if (!spec.strength) {
        runHeapDisplayMonitor(serial, process, files, spec);
        return;
    }
    std::vector<SlotBits> reads;
    {
        struct Restore {
            std::vector<SlotBits>*& target;
            std::vector<SlotBits>* previous;
            ~Restore() { target = previous; }
        } restore{displayStrengthReads_, displayStrengthReads_};
        displayStrengthReads_ = &reads;
        runHeapDisplayMonitor(serial, process, files, spec);
    }
    if (ending_) return;
    DisplayMonitor* target = monitor_.serial == serial ? &monitor_ : nullptr;
    if (!target)
        for (auto& monitor : fileMonitors_)
            if (monitor.serial == serial) {
                target = &monitor;
                break;
            }
    if (!target) return;
    std::vector<StrengthWatchToken> watches;
    const auto cancel = [&]() {
        for (const auto token : watches)
            ctx_.strengths().cancel(token);
    };
    try {
        watches.reserve(reads.size());
        for (const auto part : reads) {
            const auto result = ctx_.strengths().subscribe(part, &displayStrengthChanged, this);
            if (result.error != StrengthError::None) {
                cancel();
                if (result.error == StrengthError::ResourceLimit)
                    resourceLimit();
                else
                    fail("invalid display strength dependency");
                return;
            }
            watches.push_back(result.token);
        }
    } catch (const std::bad_alloc&) {
        cancel();
        resourceLimit();
        return;
    } catch (const std::length_error&) {
        cancel();
        resourceLimit();
        return;
    }
    for (const auto token : target->strengthWatches)
        ctx_.strengths().cancel(token);
    target->strengthReads.swap(reads);
    target->strengthWatches.swap(watches);
}
}  // namespace kirimasim::kernel
