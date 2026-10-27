// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Owned delayed-readiness payloads and common NBA issue ordering.
// Approved event-NBA contract, scheduler items 4/5 and payload lifetime.
#include "kirimasim/kernel/sim.h"

#include <algorithm>
#include <new>
#include <stdexcept>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {

bool Scheduler::liveNba(NbaTicket ticket, NbaState state) const noexcept {
    return ticket.index < capturedNbas_.size() && ticket.serial != 0 &&
           capturedNbas_[ticket.index].serial == ticket.serial &&
           capturedNbas_[ticket.index].state == state;
}

NbaCapture Scheduler::captureNba(ProcessId origin, std::span<const NbaWrite> writes) noexcept {
    if (ending_ || runningFinal_) return {{}, NbaError::InvalidPhase};
    if (origin.index() >= processes_.size() || processes_[origin.index()].fn == nullptr)
        return {{}, NbaError::InvalidOrigin};
    if (nextNbaIssue_ == UINT64_MAX) {
        resourceLimit();
        return {{}, NbaError::ResourceLimit};
    }
    for (const auto& write : writes) {
        const auto at = write.target;
        if (at.slot.index() >= store_.size() || ctx_.strengths().registered(at.slot))
            return {{}, NbaError::InvalidTarget};
        const auto& layout = store_.layout(at.slot);
        const auto width = at.width == 0 ? layout.width : at.width;
        const auto lsb = at.width == 0 ? 0 : at.lsb;
        if (at.elem >= layout.elements || width > layout.width || lsb > layout.width - width)
            return {{}, NbaError::InvalidTarget};
        const bool scalar = layout.domain == Domain::Real || layout.domain == Domain::Handle;
        if (scalar && (lsb != 0 || width != layout.width)) return {{}, NbaError::InvalidTarget};
        const auto words = scalar ? 1 : wordsFor(width) * (layout.domain == Domain::Four ? 2 : 1);
        if (write.value.size() != words) return {{}, NbaError::InvalidValue};
    }
    try {
        begin();
        // Stage every allocation before the record/issue/counters are published.
        CapturedNba staged(ctx_.heap().memoryResource());
        staged.fields.reserve(writes.size());
        for (const auto& write : writes) {
            staged.fields.push_back({write.target, staged.value.size(),
                                     static_cast<std::uint32_t>(write.value.size())});
            staged.value.insert(staged.value.end(), write.value.begin(), write.value.end());
        }
        std::uint32_t index = freeCapturedNba_;
        if (index == kNone) {
            if (capturedNbas_.size() >= kNone) throw std::bad_alloc{};
            index = static_cast<std::uint32_t>(capturedNbas_.size());
            capturedNbas_.emplace_back(ctx_.heap().memoryResource());
        } else {
            freeCapturedNba_ = capturedNbas_[index].next;
        }
        auto& record = capturedNbas_[index];
        const auto serial = record.serial + 1;
        record.serial = serial;
        record.group = processes_[origin.index()].group;
        record.issue = detail::takeNbaIssue(nextNbaIssue_);
        record.control = kNone;
        record.fields.swap(staged.fields);
        record.value.swap(staged.value);
        record.state = NbaState::Waiting;
        for (std::size_t i = 0; i < writes.size(); ++i)
            counters_.increment(cNbaScheduled_);
        return {{index, serial}, NbaError::None};
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
    return {{}, NbaError::ResourceLimit};
}

// Fresh ordinary issues append in O(1), preserving their existing ordering.
// Only an older capture needs to find its unexecuted suffix. Some NBA events
// may already have moved into Active/Reactive when a callback makes it ready.
void Scheduler::enqueueNba(Region region, Event event, std::uint64_t issue) {
    event.origin = region;
    if (issue != nextNbaIssue_ - 1) {
        const auto after = [&](const Event& pending) {
            if (pending.origin != region) return false;
            if (pending.kind == Kind::Nba) return pending.gen > issue;
            if (pending.kind != Kind::NbaCaptured ||
                !liveNba({pending.id, pending.gen}, NbaState::Ready))
                return false;
            return capturedNbas_[pending.id].issue > issue;
        };
        auto& running =
            regions_[static_cast<int>(region == Region::Nba ? Region::Active : Region::Reactive)];
        const auto first = std::find_if(running.begin(), running.end(), after);
        if (first != running.end()) {
            running.insert(first, event);
            return;
        }
        auto& queued = regions_[static_cast<int>(region)];
        queued.insert(std::find_if(queued.begin(), queued.end(), after), event);
        return;
    }
    regions_[static_cast<int>(region)].push_back(event);
}

NbaError Scheduler::readyNba(NbaTicket ticket) noexcept {
    if (!liveNba(ticket, NbaState::Waiting)) return NbaError::InvalidTicket;
    if (capturedNbas_[ticket.index].control != kNone) return NbaError::InvalidTicket;
    if (ending_ || runningFinal_) return NbaError::InvalidPhase;
    if (capturedNbas_[ticket.index].fields.empty()) {
        releaseCapturedNba(ticket.index);
        return NbaError::None;
    }
    const auto group = capturedNbas_[ticket.index].group;
    const auto issue = capturedNbas_[ticket.index].issue;
    const auto region = group == Group::Active ? Region::Nba : Region::ReNba;
    try {
        enqueueNba(region, {Kind::NbaCaptured, region, ticket.index, ticket.serial}, issue);
        capturedNbas_[ticket.index].state = NbaState::Ready;
        return NbaError::None;
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
    return NbaError::ResourceLimit;
}

void Scheduler::releaseCapturedNba(std::uint32_t index) noexcept {
    auto& record = capturedNbas_[index];
    // Free payload storage without allocating, even when its high-water size
    // was large. Only the reusable small descriptor/generation row remains.
    CapturedNba empty(ctx_.heap().memoryResource());
    record.fields.swap(empty.fields);
    record.value.swap(empty.value);
    record.state = NbaState::Free;
    if (record.serial != UINT64_MAX) {
        record.next = freeCapturedNba_;
        freeCapturedNba_ = index;
    }
}

NbaError Scheduler::discardNba(NbaTicket ticket) noexcept {
    if (!liveNba(ticket, NbaState::Waiting)) return NbaError::InvalidTicket;
    if (capturedNbas_[ticket.index].control != kNone) return NbaError::InvalidTicket;
    releaseCapturedNba(ticket.index);
    return NbaError::None;
}

void Scheduler::cancelCapturedNbas() noexcept {
    for (std::uint32_t i = 0; i < capturedNbas_.size(); ++i)
        if (capturedNbas_[i].state != NbaState::Free) releaseCapturedNba(i);
}

void Scheduler::deliverCapturedNba(NbaTicket ticket) {
    if (!liveNba(ticket, NbaState::Ready)) return;
    capturedNbas_[ticket.index].state = NbaState::Executing;
    const auto count = capturedNbas_[ticket.index].fields.size();
    for (std::size_t i = 0; i < count && !ending_; ++i) {
        // write consumes the input before invoking callbacks. They can grow
        // the record table; reacquire by stable ticket for the next field.
        const auto field = capturedNbas_[ticket.index].fields[i];
        const auto* value = capturedNbas_[ticket.index].value.data() + field.offset;
        write(field.target, value);
    }
    releaseCapturedNba(ticket.index);
}

NbaStorageStats Scheduler::nbaStorageStats() const noexcept {
    NbaStorageStats stats;
    stats.records = capturedNbas_.size();
    for (const auto& record : capturedNbas_) {
        stats.live += record.state != NbaState::Free ? 1 : 0;
        stats.ready += record.state == NbaState::Ready ? 1 : 0;
    }
    return stats;
}
}  // namespace kirimasim::kernel
