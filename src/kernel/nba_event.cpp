// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Persistent concrete controls, independently of process waits. The approved
// event-NBA contract supplies count, OR, callback and origin ownership rules.
#include "kirimasim/kernel/sim.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {
namespace {
std::uint64_t topMask(std::uint32_t width) {
    const auto bits = width % 64;
    return bits == 0 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1;
}
bool validPart(const SlotPart& part, const Store& store) {
    if (part.slot.index() >= store.size()) return false;
    const auto& layout = store.layout(part.slot);
    if (part.numElems == 0) {
        if (part.firstElem != 0) return false;
    } else if (part.firstElem >= layout.elements ||
               part.numElems > layout.elements - part.firstElem)
        return false;
    const auto width = part.width == 0 ? layout.width : part.width;
    const auto lsb = part.width == 0 ? 0 : part.lsb;
    return width <= layout.width && lsb <= layout.width - width;
}
}  // namespace

bool Scheduler::liveNbaEvent(NbaEventTicket ticket) const noexcept {
    return ticket.index < nbaEvents_.size() && ticket.serial != 0 &&
           nbaEvents_[ticket.index].serial == ticket.serial &&
           nbaEvents_[ticket.index].state == NbaEventState::Active;
}

NbaCountAction classifyNbaRepeat(NbaRepeat repeat) noexcept {
    const auto words = (std::uint64_t{repeat.width} + 63) / 64;
    if (repeat.width == 0 || repeat.words.size() != words) return NbaCountAction::Invalid;
    bool unknown = false, nonzero = false;
    for (std::size_t i = 0; i < repeat.words.size(); ++i) {
        const auto mask = i + 1 == repeat.words.size() ? topMask(repeat.width) : ~std::uint64_t{0};
        const auto value = repeat.words[i];
        if (!repeat.fourState && (value.b & mask) != 0) return NbaCountAction::Invalid;
        unknown = unknown || (value.b & mask) != 0;
        nonzero = nonzero || (value.a & mask) != 0;
    }
    const auto sign = (repeat.width - 1) % 64;
    const bool negative = repeat.isSigned && ((repeat.words.back().a >> sign) & 1U) != 0;
    if (unknown || negative || !nonzero) return NbaCountAction::Ready;
    return NbaCountAction::Bind;
}

NbaEventRegistration Scheduler::registerNbaEvent(ProcessId origin, NbaTicket payload,
                                                 NbaRepeat repeat,
                                                 std::span<const NbaEventItem> items,
                                                 SafePointHook onRelease) noexcept {
    if (ending_ || runningFinal_ || cancelingNbaEvents_) return {{}, NbaError::InvalidPhase};
    if (!liveNba(payload, NbaState::Waiting) || capturedNbas_[payload.index].control != kNone)
        return {{}, NbaError::InvalidTicket};
    if (origin.index() >= processes_.size() || processes_[origin.index()].fn == nullptr ||
        processes_[origin.index()].group != capturedNbas_[payload.index].group)
        return {{}, NbaError::InvalidOrigin};
    const auto action = classifyNbaRepeat(repeat);
    if (action == NbaCountAction::Invalid) return {{}, NbaError::InvalidValue};
    if (action == NbaCountAction::Ready) {
        const auto error = readyNba(payload);
        if (error != NbaError::None) return {{}, error};
        if (onRelease.fn != nullptr) onRelease.fn(onRelease.frame);
        return {};
    }
    if (items.empty()) return {{}, NbaError::InvalidValue};
    NbaEventTicket accepted;
    const auto beforeWalk = walking_;
    const auto beforeDrain = draining_;
    try {
        ensureSlotTables();
        NbaEventRecord staged(ctx_.heap().memoryResource());
        staged.monitors.reserve(static_cast<std::size_t>(
            std::count_if(items.begin(), items.end(), [](const auto& item) {
                return std::holds_alternative<MonitorSpec>(item);
            })));
        struct Rollback {
            Scheduler& scheduler;
            NbaEventRecord& staged;
            ~Rollback() {
                for (const auto ref : staged.monitors)
                    scheduler.releaseMonitor(ref.id, true);
            }
        } rollback{*this, staged};
        staged.remaining.reserve(repeat.words.size());
        for (const auto word : repeat.words)
            staged.remaining.push_back(word.a);
        staged.remaining.back() &= topMask(repeat.width);
        staged.items.reserve(items.size());
        for (const auto& item : items) {
            NbaBoundItem bound;
            if (const auto* slot = std::get_if<WaitItem>(&item)) {
                if (!validPart(slot->part, store_)) return {{}, NbaError::InvalidTarget};
                const auto domain = store_.layout(slot->part.slot).domain;
                if ((domain == Domain::Handle || domain == Domain::Real) && slot->edge != Edge::Any)
                    return {{}, NbaError::InvalidTarget};
                bound.slot = *slot;
                bound.guard = slot->guard;
                bound.frame = slot->guardFrame;
                staged.slots.push_back(slot->part.slot.index());
            } else if (const auto* named = std::get_if<NamedValueWaitItem>(&item)) {
                bound.named = true;
                bound.guard = named->guard;
                bound.frame = named->guardFrame;
                if (named->value.bits != 0) {
                    auto binding = bindEventValue(named->value);
                    if (!binding) {
                        if (binding.error == ManagedEventError::ResourceLimit)
                            throw std::bad_alloc{};
                        return {{}, NbaError::InvalidValue};
                    }
                    staged.managed = staged.managed || binding.value.pin.token().bits != 0;
                    staged.events.push_back(binding.value.event.index());
                    bound.binding = std::move(binding.value);
                }
            } else if (const auto* spec = std::get_if<MonitorSpec>(&item)) {
                if (spec->eval == nullptr || spec->untilTrue ||
                    (spec->domain != Domain::Two && spec->domain != Domain::Four &&
                     spec->domain != Domain::Real) ||
                    (spec->domain != Domain::Real && spec->width == 0))
                    return {{}, NbaError::InvalidValue};
                for (const auto& part : spec->slots)
                    if (!validPart(part, store_)) return {{}, NbaError::InvalidTarget};
                for (const auto event : spec->events)
                    if (event.index() >= events_.size() || events_[event.index()].managed)
                        return {{}, NbaError::InvalidValue};
                const auto id = allocateMonitor();
                bound.monitored = true;
                bound.monitor = {id, monitors_[id].serial};
                staged.monitors.push_back(bound.monitor);
                auto& monitor = monitors_[id];
                monitor.process = origin;
                monitor.spec = *spec;
                if (!prepareActivationReads(id))
                    return {{},
                            reason_ == EndReason::ResourceLimit ? NbaError::ResourceLimit
                                                                : NbaError::InvalidValue};
                for (const auto value : monitor.spec.eventValues) {
                    if (value.bits == 0) continue;
                    auto binding = bindEventValue(value);
                    if (!binding) {
                        if (binding.error == ManagedEventError::ResourceLimit)
                            throw std::bad_alloc{};
                        return {{}, NbaError::InvalidValue};
                    }
                    staged.managed = staged.managed || binding.value.pin.token().bits != 0;
                    monitor.eventBindings.push_back(std::move(binding.value));
                }
                staged.managed = staged.managed || monitor.spec.captureEvents;
            } else {
                return {{}, NbaError::InvalidValue};
            }
            staged.items.push_back(std::move(bound));
        }
        const auto unique = [](auto& values) {
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
        };
        unique(staged.slots);
        unique(staged.events);
        // All index/storage allocations precede publication. Sweeping dead
        // references changes neither subscriptions nor user-visible state.
        nbaEventSlots_.resize(store_.size());
        nbaEventNames_.resize(events_.size());
        const auto reserve = [&](auto& index, const auto& keys) {
            for (const auto key : keys) {
                auto& list = index[key];
                std::erase_if(list, [&](const auto ticket) { return !liveNbaEvent(ticket); });
                list.reserve(list.size() + 1);
            }
        };
        reserve(nbaEventSlots_, staged.slots);
        reserve(nbaEventNames_, staged.events);
        // Reserve every static dependency before publication. Dynamic reads
        // are captured by initialization, with the accepted owner already live.
        heapMonitors_.reserve(heapMonitors_.size() + staged.monitors.size());
        for (const auto ref : staged.monitors) {
            const auto& monitor = monitors_[ref.id];
            for (const auto& part : monitor.spec.slots) {
                auto& list = slotMonitors_[part.slot.index()];
                list.reserve(list.size() + staged.monitors.size());
            }
            const auto reserveEvent = [&](EventId event) {
                auto& list = events_[event.index()].monitors;
                list.reserve(list.size() + staged.monitors.size());
            };
            for (const auto event : monitor.spec.events)
                reserveEvent(event);
            for (const auto& binding : monitor.eventBindings)
                reserveEvent(binding.event);
        }
        if (processes_[origin.index()].nbaOrigins == UINT32_MAX) throw std::bad_alloc{};
        auto index = freeNbaEvent_;
        if (index == kNone) {
            if (nbaEvents_.size() >= kNone) throw std::bad_alloc{};
            index = static_cast<std::uint32_t>(nbaEvents_.size());
            nbaEvents_.emplace_back(ctx_.heap().memoryResource());
        } else {
            freeNbaEvent_ = nbaEvents_[index].next;
        }
        auto& record = nbaEvents_[index];
        ++record.serial;  // exhausted generations are never reused
        record.origin = origin;
        record.payload = payload;
        record.issue = capturedNbas_[payload.index].issue;
        record.release = onRelease;
        record.managed = staged.managed;
        record.callbacks = 0;
        record.remaining.swap(staged.remaining);
        record.items.swap(staged.items);
        record.slots.swap(staged.slots);
        record.events.swap(staged.events);
        record.monitors.swap(staged.monitors);
        const NbaEventTicket ticket{index, record.serial};
        for (const auto ref : record.monitors) {
            monitors_[ref.id].control = ticket.index;
            monitors_[ref.id].gen = ticket.serial;
        }
        record.state = NbaEventState::Active;
        capturedNbas_[payload.index].control = index;
        ++processes_[origin.index()].nbaOrigins;
        ++liveNbaEvents_;
        for (const auto slot : record.slots)
            nbaEventSlots_[slot].push_back(ticket);
        for (const auto event : record.events)
            nbaEventNames_[event].push_back(ticket);
        accepted = ticket;
        if (!record.monitors.empty()) initializeNbaMonitors(ticket);
        return {ticket};
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
    walking_ = beforeWalk;
    draining_ = beforeDrain;
    if (accepted.serial != 0) {
        finishNbaEvent(accepted, false);
        return {accepted};  // accepted callbacks report runtime failures through reason()
    }
    return {{}, NbaError::ResourceLimit};
}

void Scheduler::initializeNbaMonitors(NbaEventTicket ticket) {
    beginWalk();
    {
        ++nbaEvents_[ticket.index].callbacks;
        struct End {
            Scheduler& scheduler;
            NbaEventTicket ticket;
            ~End() { scheduler.endNbaEventCallback(ticket); }
        } callback{*this, ticket};
        const auto count = nbaEvents_[ticket.index].monitors.size();
        for (std::size_t i = 0; i < count; ++i)
            activateMonitor(nbaEvents_[ticket.index].monitors[i].id);
        for (std::size_t i = 0; i < count && liveNbaEvent(ticket) && !ending_; ++i) {
            const auto ref = nbaEvents_[ticket.index].monitors[i];
            if (!initializeMonitor(ref.id)) {
                finishNbaEvent(ticket, false);
                break;
            }
        }
        if (ending_) finishNbaEvent(ticket, false);
    }
    endWalk();
}

void Scheduler::releaseNbaEvent(NbaEventTicket ticket) noexcept {
    if (ticket.index >= nbaEvents_.size()) return;
    auto& record = nbaEvents_[ticket.index];
    if (record.serial != ticket.serial || record.state != NbaEventState::Retiring ||
        record.callbacks != 0)
        return;
    const auto payload = record.payload;
    const bool ready = record.ready;
    const auto origin = record.origin;
    const auto cleanup = record.release;
    record.release = {};
    for (const auto ref : record.monitors)
        releaseMonitor(ref.id, true);
    record.state = NbaEventState::Releasing;
    if (liveNba(payload, NbaState::Waiting) &&
        capturedNbas_[payload.index].control == ticket.index) {
        capturedNbas_[payload.index].control = kNone;
        if (ready) {
            if (readyNba(payload) != NbaError::None) static_cast<void>(discardNba(payload));
        } else {
            static_cast<void>(discardNba(payload));
        }
    }
    // The origin and all named pins remain owned through a reentrant cleanup.
    if (cleanup.fn != nullptr) cleanup.fn(cleanup.frame);
    auto& again = nbaEvents_[ticket.index];
    NbaEventRecord empty(ctx_.heap().memoryResource());
    again.remaining.swap(empty.remaining);
    again.items.swap(empty.items);
    again.slots.swap(empty.slots);
    again.events.swap(empty.events);
    again.monitors.swap(empty.monitors);
    again.state = NbaEventState::Free;
    again.origin = kNone;
    again.payload = {};
    if (again.serial != UINT64_MAX) {
        again.next = freeNbaEvent_;
        freeNbaEvent_ = ticket.index;
    }
    if (--processes_[origin.index()].nbaOrigins == 0) ++callbackReleaseEpoch_;
}

void Scheduler::finishNbaEvent(NbaEventTicket ticket, bool ready) noexcept {
    if (!liveNbaEvent(ticket)) return;
    auto& record = nbaEvents_[ticket.index];
    record.state = NbaEventState::Retiring;
    record.ready = ready;
    --liveNbaEvents_;
    const auto detach = [&](auto& index, const auto& keys) {
        for (const auto key : keys)
            std::erase(index[key], ticket);
    };
    detach(nbaEventSlots_, record.slots);
    detach(nbaEventNames_, record.events);
    releaseNbaEvent(ticket);
}

void Scheduler::endNbaEventCallback(NbaEventTicket ticket) noexcept {
    if (ticket.index >= nbaEvents_.size() || nbaEvents_[ticket.index].serial != ticket.serial)
        return;
    --nbaEvents_[ticket.index].callbacks;
    if (nbaEvents_[ticket.index].state == NbaEventState::Retiring) releaseNbaEvent(ticket);
}

NbaError Scheduler::discardNbaEvent(NbaEventTicket ticket) noexcept {
    if (!liveNbaEvent(ticket)) return NbaError::InvalidTicket;
    finishNbaEvent(ticket, false);
    return NbaError::None;
}
void Scheduler::cancelNbaEvents() noexcept {
    cancelNbaMonitorNotices();
    const auto previous = cancelingNbaEvents_;
    cancelingNbaEvents_ = true;
    for (std::uint32_t i = 0; i < nbaEvents_.size(); ++i)
        if (nbaEvents_[i].state == NbaEventState::Active)
            finishNbaEvent({i, nbaEvents_[i].serial}, false);
    cancelingNbaEvents_ = previous;
}
void Scheduler::cancelManagedNbaEvents() noexcept {
    const auto previous = cancelingNbaEvents_;
    cancelingNbaEvents_ = true;
    for (std::uint32_t i = 0; i < nbaEvents_.size(); ++i)
        if (nbaEvents_[i].state == NbaEventState::Active && nbaEvents_[i].managed)
            finishNbaEvent({i, nbaEvents_[i].serial}, false);
    cancelingNbaEvents_ = previous;
}

void Scheduler::matchNbaEvents(std::pmr::vector<NbaEventTicket>& candidates, NbaSource kind,
                               std::uint32_t source, std::uint32_t elem, std::uint32_t lsb,
                               std::uint32_t width, const std::uint64_t* old,
                               const std::uint64_t* now, std::span<const MonitorRef> monitors) {
    std::erase_if(candidates, [&](const auto ticket) { return !liveNbaEvent(ticket); });
    std::sort(candidates.begin(), candidates.end(), [&](const auto a, const auto b) {
        return nbaEvents_[a.index].issue < nbaEvents_[b.index].issue;
    });
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    for (const auto ticket : candidates) {
        if (ending_) break;
        if (!liveNbaEvent(ticket)) continue;
        const auto count = nbaEvents_[ticket.index].items.size();
        bool qualified = false;
        for (std::size_t i = 0; i < count && liveNbaEvent(ticket) && !ending_; ++i) {
            const auto& item = nbaEvents_[ticket.index].items[i];
            const auto ref = item.monitor;
            const bool monitored = item.monitored;
            const bool matches =
                monitored ? nbaMonitorDepends(ref, kind, source, elem, lsb, width, monitors)
                : kind == NbaSource::Named
                    ? item.named && item.binding.event == EventId{source}
                    : kind == NbaSource::Slot && !item.named &&
                          slotEventMatches(item.slot, SlotId{source}, store_.layout(SlotId{source}),
                                           elem, lsb, width, old, now);
            if (!matches) continue;
            // Copy callback operands before user code can grow/cancel tables.
            const auto guard = item.guard;
            void* const frame = item.frame;
            bool accepted = true;
            if (monitored || guard != nullptr) {
                if (nbaEvents_[ticket.index].callbacks == UINT32_MAX) throw std::bad_alloc{};
                ++nbaEvents_[ticket.index].callbacks;
                struct End {
                    Scheduler& scheduler;
                    NbaEventTicket ticket;
                    ~End() { scheduler.endNbaEventCallback(ticket); }
                } end{*this, ticket};
                accepted = monitored ? updateMonitor(ref.id) : guard(toC(ctx_), frame) != 0;
            }
            qualified = qualified || accepted;
        }
        if (!qualified || !liveNbaEvent(ticket) || ending_) continue;
        // A whole committed notification decrements once, after every relevant
        // item/guard, even when duplicate or overlapping items qualified.
        if (detail::decrementNbaCount(nbaEvents_[ticket.index].remaining))
            finishNbaEvent(ticket, true);
    }
}

void Scheduler::notifyNbaSlot(SlotId slot, std::uint32_t elem, std::uint32_t lsb,
                              std::uint32_t width, const std::uint64_t* old,
                              const std::uint64_t* now) noexcept {
    if (liveNbaEvents_ == 0) return;
    try {
        std::pmr::vector<NbaEventTicket> candidates(ctx_.heap().memoryResource());
        if (slot.index() < nbaEventSlots_.size()) candidates = nbaEventSlots_[slot.index()];
        for (const auto ref : slotMonitors_[slot.index()])
            if (live(ref) && monitors_[ref.id].control != kNone)
                candidates.push_back({monitors_[ref.id].control, monitors_[ref.id].gen});
        matchNbaEvents(candidates, NbaSource::Slot, slot.index(), elem, lsb, width, old, now);
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
}
void Scheduler::notifyNbaNamed(EventId event) noexcept {
    if (liveNbaEvents_ == 0) return;
    try {
        std::pmr::vector<NbaEventTicket> candidates(ctx_.heap().memoryResource());
        if (event.index() < nbaEventNames_.size()) candidates = nbaEventNames_[event.index()];
        for (const auto ref : events_[event.index()].monitors)
            if (observesEvent(ref, event) && monitors_[ref.id].control != kNone)
                candidates.push_back({monitors_[ref.id].control, monitors_[ref.id].gen});
        matchNbaEvents(candidates, NbaSource::Named, event.index(), 0, 0, 0, nullptr, nullptr);
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
}
bool Scheduler::nbaMonitorDepends(MonitorRef ref, NbaSource kind, std::uint32_t source,
                                  std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                                  std::span<const MonitorRef> monitors) const {
    if (!live(ref)) return false;
    if (kind == NbaSource::Slot) return monitorDependsOnSlot(ref, SlotId{source}, elem, lsb, width);
    if (kind == NbaSource::Named) return observesEvent(ref, EventId{source});
    return std::any_of(monitors.begin(), monitors.end(), [&](const auto& item) {
        return item.id == ref.id && item.serial == ref.serial;
    });
}

void Scheduler::notifyNbaMonitors(std::span<const MonitorRef> monitors) {
    std::pmr::vector<NbaEventTicket> candidates(ctx_.heap().memoryResource());
    for (const auto ref : monitors)
        if (live(ref) && monitors_[ref.id].control != kNone)
            candidates.push_back({monitors_[ref.id].control, monitors_[ref.id].gen});
    matchNbaEvents(candidates, NbaSource::Monitors, 0, 0, 0, 0, nullptr, nullptr, monitors);
}

void Scheduler::queueNbaMonitorNotice(std::pmr::vector<MonitorRef>& monitors) {
    if (monitors.empty()) return;
    auto index = freeNbaNotice_;
    if (index == kNone) {
        if (nbaNotices_.size() >= kNone) throw std::bad_alloc{};
        index = static_cast<std::uint32_t>(nbaNotices_.size());
        nbaNotices_.emplace_back(ctx_.heap().memoryResource());
    } else {
        freeNbaNotice_ = nbaNotices_[index].next;
    }
    auto& notice = nbaNotices_[index];
    ++notice.serial;
    notice.pending = true;
    notice.monitors.swap(monitors);
    LateChange change;
    change.nba = true;
    change.monitor = {index, notice.serial};
    try {
        lateChanges_.push_back(std::move(change));
    } catch (...) {
        releaseNbaMonitorNotice(index);
        throw;
    }
}

void Scheduler::releaseNbaMonitorNotice(std::uint32_t index) noexcept {
    auto& notice = nbaNotices_[index];
    if (!notice.pending) return;
    std::pmr::vector<MonitorRef> empty(ctx_.heap().memoryResource());
    notice.monitors.swap(empty);
    notice.pending = false;
    if (notice.serial != UINT64_MAX) {
        notice.next = freeNbaNotice_;
        freeNbaNotice_ = index;
    }
}

void Scheduler::deliverNbaMonitorNotice(MonitorRef ref) {
    if (ref.id >= nbaNotices_.size() || nbaNotices_[ref.id].serial != ref.serial ||
        !nbaNotices_[ref.id].pending)
        return;
    auto monitors = std::move(nbaNotices_[ref.id].monitors);
    releaseNbaMonitorNotice(ref.id);
    notifyNbaMonitors(monitors);
}

void Scheduler::cancelNbaMonitorNotices() noexcept {
    nbaHeapPending_.clear();
    for (std::uint32_t i = 0; i < nbaNotices_.size(); ++i)
        releaseNbaMonitorNotice(i);
}

NbaEventStats Scheduler::nbaEventStats() const noexcept {
    NbaEventStats stats;
    stats.records = nbaEvents_.size();
    stats.live = liveNbaEvents_;
    stats.noticeRecords = nbaNotices_.size();
    for (const auto& notice : nbaNotices_)
        stats.pendingNotices += notice.pending ? 1 : 0;
    for (const auto& record : nbaEvents_) {
        stats.callbacks += record.callbacks;
        if (record.state == NbaEventState::Active) stats.subscriptions += record.items.size();
    }
    return stats;
}
}  // namespace kirimasim::kernel
