// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Concrete named-event values and operation ownership (contract 5/6/8).
#include "kirimasim/kernel/sim.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {

ManagedEventResult<EventId> Scheduler::resolveEventValue(EventValue value) const noexcept {
    if (value.bits == 0) return {kNone, ManagedEventError::Null};
    if ((value.bits >> 32) != 0) {
        const auto result = ctx_.managedEvents().resolve(ManagedEventToken{value.bits});
        if (!result) return result;
        if (result.value.index() >= events_.size() || !events_[result.value.index()].managed)
            return {kNone, ManagedEventError::InvalidToken};
        return result;
    }
    const auto index = static_cast<std::uint32_t>(value.bits - 1);
    if (index >= events_.size() || events_[index].managed)
        return {kNone, ManagedEventError::InvalidToken};
    return {EventId{index}};
}
ManagedEventResult<Scheduler::EventBinding> Scheduler::bindEventValue(EventValue value) noexcept {
    const auto event = resolveEventValue(value);
    if (!event) return {{}, event.error};
    EventBinding binding;
    binding.value = value;
    binding.event = event.value;
    if ((value.bits >> 32) != 0) {
        auto held = ctx_.managedEvents().retain(ManagedEventToken{value.bits});
        if (!held) return {{}, held.error};
        binding.pin = std::move(held.value);
    }
    return {std::move(binding)};
}

ManagedEventError Scheduler::waitAnyValues(ProcessId p, std::span<const WaitItem> items,
                                           std::span<const MonitorSpec> monitors,
                                           std::span<const NamedValueWaitItem> named) noexcept {
    if (p.index() >= processes_.size() || processes_[p.index()].done)
        return ManagedEventError::InvalidToken;
    std::pmr::vector<NamedWaitItem> resolved(ctx_.heap().memoryResource());
    std::pmr::vector<ManagedEventPin> pins(ctx_.heap().memoryResource());
    std::pmr::vector<ManagedEventPin> monitorPins(ctx_.heap().memoryResource());
    try {
        resolved.reserve(named.size());
        pins.reserve(named.size());
        for (const auto& monitor : monitors)
            for (const auto value : monitor.eventValues) {
                if (value.bits == 0) continue;
                auto bound = bindEventValue(value);
                if (!bound) return bound.error;
                if (bound.value.pin.token().bits != 0)
                    monitorPins.push_back(std::move(bound.value.pin));
            }
        for (const auto& item : named) {
            auto bound = bindEventValue(item.value);
            if (!bound) return bound.error;
            resolved.push_back({bound.value.event, item.guard, item.guardFrame, item.flushPoint});
            const auto token = bound.value.pin.token();
            if (token.bits != 0 && std::none_of(pins.begin(), pins.end(), [&](const auto& pin) {
                    return pin.token() == token;
                }))
                pins.push_back(std::move(bound.value.pin));
        }
    } catch (const std::bad_alloc&) {
        return ManagedEventError::ResourceLimit;
    } catch (const std::length_error&) {
        return ManagedEventError::ResourceLimit;
    }
    const auto beforeWalk = walking_;
    const bool beforeDrain = draining_;
    usedEventValues_ = usedEventValues_ || !pins.empty() || !monitorPins.empty();
    const auto failed = [&] {
        cancelWait(p);
        walking_ = beforeWalk;
        draining_ = beforeDrain;
        if (walking_ == 0 && !draining_) lateChanges_.clear();
        resourceLimit();
        return ManagedEventError::ResourceLimit;
    };
    try {
        std::uint64_t registered = 0;
        registerWait(p, items, monitors, resolved, &registered);
        if (ending_) {
            cancelWait(p);
            return reason_ == EndReason::ResourceLimit ? ManagedEventError::ResourceLimit
                                                       : ManagedEventError::None;
        }
        // Initial evaluation/late notifications may already have ended this
        // wait, canceled it or installed another wait. Never replace its pins.
        auto& process = processes_[p.index()];
        if (!process.done && process.gen == registered) process.eventPins.swap(pins);
        return ManagedEventError::None;
    } catch (const std::bad_alloc&) {
        return failed();
    } catch (const std::length_error&) {
        return failed();
    }
}
ManagedEventError Scheduler::waitNamedValue(ProcessId p, EventValue value,
                                            bool flushPoint) noexcept {
    const NamedValueWaitItem item{value, nullptr, nullptr, flushPoint};
    return waitAnyValues(p, {}, {}, {&item, 1});
}

ManagedEventError Scheduler::triggerValue(EventValue value) noexcept {
    if (value.bits == 0) return ManagedEventError::None;
    auto bound = bindEventValue(value);
    if (!bound) return bound.error;
    const auto beforeWalk = walking_;
    const bool beforeDrain = draining_;
    const auto failed = [&] {
        walking_ = beforeWalk;
        draining_ = beforeDrain;
        if (walking_ == 0 && !draining_) lateChanges_.clear();
        resourceLimit();
        return ManagedEventError::ResourceLimit;
    };
    try {
        if (walking_ != 0) {
            usedEventValues_ = usedEventValues_ || bound.value.pin.token().bits != 0;
            LateChange change;
            change.trigger = true;
            change.event = bound.value.event;
            change.eventPin = std::move(bound.value.pin);
            lateChanges_.push_back(std::move(change));
        } else {
            trigger(bound.value.event);
        }
        return ManagedEventError::None;
    } catch (const std::bad_alloc&) {
        return failed();
    } catch (const std::length_error&) {
        return failed();
    }
}
ManagedEventError Scheduler::triggerNbValue(ProcessId p, EventValue value,
                                            std::uint64_t delay) noexcept {
    return triggerNbValue(p, value, std::optional<std::uint64_t>{delay});
}
ManagedEventError Scheduler::triggerNbValue(ProcessId p, EventValue value,
                                            std::optional<std::uint64_t> normalized) noexcept {
    if (value.bits == 0) return ManagedEventError::None;
    if (p.index() >= processes_.size()) return ManagedEventError::InvalidToken;
    auto bound = bindEventValue(value);
    if (!bound) return bound.error;
    if (!normalized) {
        overflow_ = true;
        return ManagedEventError::None;
    }
    const auto delay = *normalized;
    if (bound.value.pin.token().bits == 0) {
        try {
            triggerNb(p, bound.value.event, delay);
            return ManagedEventError::None;
        } catch (const std::bad_alloc&) {
            return ManagedEventError::ResourceLimit;
        } catch (const std::length_error&) {
            return ManagedEventError::ResourceLimit;
        }
    }
    auto index = freeEventValue_;
    try {
        if (index == kNone) {
            if (pendingEventValues_.size() >= kNone) return ManagedEventError::ResourceLimit;
            pendingEventValues_.emplace_back();
            index = static_cast<std::uint32_t>(pendingEventValues_.size() - 1);
        } else {
            freeEventValue_ = pendingEventValues_[index].next;
        }
    } catch (const std::bad_alloc&) {
        return ManagedEventError::ResourceLimit;
    } catch (const std::length_error&) {
        return ManagedEventError::ResourceLimit;
    }
    auto& record = pendingEventValues_[index];
    usedEventValues_ = true;
    ++record.serial;  // Exhausted serials are never put back on the free list.
    record.next = kNone;
    record.pending = true;
    record.event = bound.value.event;
    record.pin = std::move(bound.value.pin);
    const auto region = processes_[p.index()].group == Group::Active ? Region::Nba : Region::ReNba;
    try {
        schedule(region, {Kind::TriggerValue, region, index, record.serial}, delay);
        if (delay > UINT64_MAX - time_) releaseEventValue(index);
        return ManagedEventError::None;
    } catch (const std::bad_alloc&) {
        releaseEventValue(index);
        return ManagedEventError::ResourceLimit;
    } catch (const std::length_error&) {
        releaseEventValue(index);
        return ManagedEventError::ResourceLimit;
    }
}
void Scheduler::releaseEventValue(std::uint32_t id) noexcept {
    auto& record = pendingEventValues_[id];
    if (!record.pending) return;
    record.pending = false;
    record.pin.reset();
    if (record.serial != std::numeric_limits<std::uint64_t>::max()) {
        record.next = freeEventValue_;
        freeEventValue_ = id;
    }
}
void Scheduler::deliverEventValue(const Event& event) {
    if (event.id >= pendingEventValues_.size()) return;
    auto& record = pendingEventValues_[event.id];
    if (!record.pending || record.serial != event.gen) return;
    auto pin = std::move(record.pin);
    const auto target = record.event;
    releaseEventValue(event.id);
    // A callback may cancel the record table, collect or reuse this record.
    // This local pin survives the complete trigger and its late notices.
    if (!pin) {
        fail("invalid managed event in a pending trigger");
        return;
    }
    const auto resolved = ctx_.managedEvents().resolve(pin);
    if (!resolved || resolved.value != target) {
        fail("invalid pending event target");
        return;
    }
    const auto error = triggerValue(EventValue::managed(pin.token()));
    if (error == ManagedEventError::ResourceLimit)
        resourceLimit();
    else if (error != ManagedEventError::None)
        fail("invalid pending event trigger");
}
void Scheduler::cancelManagedEventOperations() noexcept {
    if (!usedEventValues_) return;
    for (std::uint32_t i = 0; i < pendingEventValues_.size(); ++i)
        releaseEventValue(i);
    for (std::uint32_t i = 0; i < processes_.size(); ++i) {
        const auto& process = processes_[i];
        bool managed = !process.eventPins.empty();
        for (const auto monitor : process.monitors) {
            managed = managed || monitors_[monitor].spec.captureEvents ||
                      !monitors_[monitor].spec.eventValues.empty();
            for (const auto& binding : monitors_[monitor].eventBindings)
                managed = managed || binding.pin.token().bits != 0;
        }
        if (managed) cancelWait(ProcessId{i});
    }
    for (auto& change : lateChanges_)
        if (change.eventPin.token().bits != 0) {
            change.canceled = true;
            change.eventPin.reset();
        }
}

ManagedEventResult<bool> Scheduler::triggeredValue(EventValue value) noexcept {
    if (eventCapture_ != nullptr && eventCapture_->error != ManagedEventError::None)
        return {{}, eventCapture_->error};
    if (value.bits == 0) return {false};
    auto bound = bindEventValue(value);
    if (!bound) {
        if (eventCapture_ != nullptr) eventCapture_->error = bound.error;
        return {{}, bound.error};
    }
    const bool on = triggered(bound.value.event);
    if (eventCapture_ != nullptr) {
        auto& capture = *eventCapture_;
        try {
            if (std::none_of(capture.reads.begin(), capture.reads.end(),
                             [&](const auto& read) { return read.value == value; }))
                capture.reads.push_back(std::move(bound.value));
        } catch (const std::bad_alloc&) {
            capture.error = ManagedEventError::ResourceLimit;
        } catch (const std::length_error&) {
            capture.error = ManagedEventError::ResourceLimit;
        }
        if (capture.error != ManagedEventError::None) return {{}, capture.error};
    }
    return {on};
}

bool Scheduler::observesEvent(MonitorRef ref, EventId event) const {
    if (!live(ref)) return false;
    const auto& monitor = monitors_[ref.id];
    if (!monitor.spec.captureEvents && monitor.spec.eventValues.empty()) return true;
    return std::find(monitor.spec.events.begin(), monitor.spec.events.end(), event) !=
               monitor.spec.events.end() ||
           std::any_of(monitor.eventBindings.begin(), monitor.eventBindings.end(),
                       [&](const auto& binding) { return binding.event == event; });
}
void Scheduler::attachEventMonitor(EventId event, MonitorRef ref) {
    auto& object = events_[event.index()];
    if (std::any_of(object.monitors.begin(), object.monitors.end(),
                    [&](const auto& old) { return old.id == ref.id && old.serial == ref.serial; }))
        return;
    // Capacity is reserved before publication by the event-read path. The
    // legacy/static path can allocate here as its previous registration did.
    if (object.monitors.size() >= object.monitorLimit) {
        std::erase_if(object.monitors, [&](const auto& old) { return !observesEvent(old, event); });
        object.monitorLimit =
            static_cast<std::uint32_t>(std::max<std::size_t>(16, 2 * object.monitors.size()));
    }
    object.monitors.push_back(ref);
}
bool Scheduler::publishEventReads(std::uint32_t id, EventCapture& capture) {
    const MonitorRef ref{id, monitors_[id].serial};
    for (const auto& read : capture.reads) {
        auto& list = events_[read.event.index()].monitors;
        list.reserve(list.size() + 1);
    }
    for (const auto& read : capture.reads)
        attachEventMonitor(read.event, ref);
    monitors_[id].eventBindings.swap(capture.reads);
    return true;
}
bool Scheduler::evalMonitorValue(std::uint32_t id, std::span<std::uint64_t> out) {
    const bool enabled =
        monitors_[id].spec.captureEvents || !monitors_[id].spec.eventValues.empty();
    if (!enabled && eventCapture_ == nullptr) return evalMonitorBody(id, out);
    const auto serial = monitors_[id].serial;
    EventCapture capture(ctx_.heap().memoryResource());
    std::pmr::vector<ManagedEventPin> held(ctx_.heap().memoryResource());
    try {
        for (const auto& old : monitors_[id].eventBindings) {
            if (old.pin.token().bits == 0) continue;
            auto pin = ctx_.managedEvents().retain(old.pin);
            if (!pin) {
                fail("invalid managed monitor event");
                return false;
            }
            held.push_back(std::move(pin.value));
        }
        for (const auto value : monitors_[id].spec.eventValues) {
            if (value.bits == 0) continue;
            if (std::any_of(capture.reads.begin(), capture.reads.end(),
                            [&](const auto& read) { return read.value == value; }))
                continue;
            auto binding = bindEventValue(value);
            if (!binding) {
                if (binding.error == ManagedEventError::ResourceLimit)
                    heapLimit();
                else
                    fail("invalid managed monitor dependency");
                return false;
            }
            capture.reads.push_back(std::move(binding.value));
        }
        struct Restore {
            EventCapture*& current;
            EventCapture* previous;
            ~Restore() { current = previous; }
        } restore{eventCapture_, eventCapture_};
        eventCapture_ = monitors_[id].spec.captureEvents ? &capture : nullptr;
        if (!evalMonitorBody(id, out)) return false;
        if (capture.error != ManagedEventError::None) {
            if (capture.error == ManagedEventError::ResourceLimit)
                heapLimit();
            else
                fail("invalid managed event read");
            return false;
        }
        if (ending_ || monitors_[id].serial != serial) return false;
        return publishEventReads(id, capture);
    } catch (const std::bad_alloc&) {
        heapLimit();
        return false;
    } catch (const std::length_error&) {
        heapLimit();
        return false;
    }
}

}  // namespace kirimasim::kernel
