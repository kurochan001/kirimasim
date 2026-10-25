// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/managed_event.h"

#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace kirimasim::kernel {

ManagedEventPin::ManagedEventPin(ManagedEventPin&& other) noexcept
    : store_(std::exchange(other.store_, nullptr)), token_(std::exchange(other.token_, {})) {}
ManagedEventPin& ManagedEventPin::operator=(ManagedEventPin&& other) noexcept {
    if (this != &other) {
        reset();
        store_ = std::exchange(other.store_, nullptr);
        token_ = std::exchange(other.token_, {});
    }
    return *this;
}
ManagedEventPin::~ManagedEventPin() { reset(); }
void ManagedEventPin::reset() noexcept {
    if (store_ != nullptr) store_->release(token_);
    store_ = nullptr;
    token_ = {};
}
ManagedEventPin::operator bool() const noexcept {
    return store_ != nullptr && store_->valid(token_);
}

const ManagedEvents::Row* ManagedEvents::row(ManagedEventToken token) const noexcept {
    const auto encoded = static_cast<std::uint32_t>(token.bits);
    const auto generation = static_cast<std::uint32_t>(token.bits >> 32);
    if (encoded == 0 || generation == 0 || encoded > rows_.size()) return nullptr;
    const auto& r = rows_[encoded - 1];
    return r.live && r.generation == generation ? &r : nullptr;
}
bool ManagedEvents::valid(ManagedEventToken token) const noexcept { return row(token) != nullptr; }
ManagedEventError ManagedEvents::setBackend(ManagedEventBackend backend) noexcept {
    if (collecting_) return ManagedEventError::Busy;
    if (!rows_.empty() || backend.create == nullptr || backend.clear == nullptr)
        return ManagedEventError::InvalidBackend;
    backend_ = backend;
    return ManagedEventError::None;
}
ManagedEventResult<ManagedEventPin> ManagedEvents::create() noexcept {
    if (collecting_) return {{}, ManagedEventError::Busy};
    if (backend_.create == nullptr) return {{}, ManagedEventError::InvalidBackend};
    std::uint32_t index = free_;
    if (index == kNone) {
        if (rows_.size() >= std::numeric_limits<std::uint32_t>::max())
            return {{}, ManagedEventError::ResourceLimit};
        try {
            rows_.emplace_back();
        } catch (const std::bad_alloc&) {
            return {{}, ManagedEventError::ResourceLimit};
        } catch (const std::length_error&) {
            return {{}, ManagedEventError::ResourceLimit};
        }
        index = static_cast<std::uint32_t>(rows_.size() - 1);
        // Allocate bookkeeping first. A backend failure publishes no token
        // and leaves neither a registry row nor a scheduler object behind.
        const auto event = backend_.create(backend_.frame);
        if (!event || event.value == kNone) {
            rows_.pop_back();
            return {{}, event ? ManagedEventError::InvalidBackend : event.error};
        }
        rows_[index].event = event.value;
    } else {
        free_ = rows_[index].next;
    }
    auto& r = rows_[index];
    ++r.generation;  // A maximum generation is retired, never put on the free list.
    r.next = kNone;
    r.pins = 1;
    r.live = true;
    r.marked = false;
    ++live_;
    eligible_ = true;
    const ManagedEventToken token{(std::uint64_t{r.generation} << 32) | (std::uint64_t{index} + 1)};
    return {ManagedEventPin{this, token}};
}
ManagedEventResult<ManagedEventPin> ManagedEvents::retain(ManagedEventToken token) noexcept {
    if (collecting_) return {{}, ManagedEventError::Busy};
    const auto* found = row(token);
    if (found == nullptr)
        return {{}, token.bits == 0 ? ManagedEventError::Null : ManagedEventError::InvalidToken};
    if (found->pins == std::numeric_limits<std::uint64_t>::max())
        return {{}, ManagedEventError::ResourceLimit};
    ++rows_[static_cast<std::uint32_t>(token.bits) - 1].pins;
    return {ManagedEventPin{this, token}};
}
ManagedEventResult<ManagedEventPin> ManagedEvents::retain(const ManagedEventPin& pin) noexcept {
    if (pin.store_ != this) return {{}, ManagedEventError::InvalidToken};
    return retain(pin.token_);
}
ManagedEventResult<EventId> ManagedEvents::resolve(ManagedEventToken token) const noexcept {
    const auto* r = row(token);
    if (r == nullptr)
        return {kNone, token.bits == 0 ? ManagedEventError::Null : ManagedEventError::InvalidToken};
    return {r->event};
}
ManagedEventResult<EventId> ManagedEvents::resolve(const ManagedEventPin& pin) const noexcept {
    if (pin.store_ != this) return {kNone, ManagedEventError::InvalidToken};
    return resolve(pin.token_);
}
void ManagedEvents::release(ManagedEventToken token) noexcept {
    if (row(token) == nullptr) return;
    auto& r = rows_[static_cast<std::uint32_t>(token.bits) - 1];
    if (r.pins != 0) --r.pins;
    eligible_ = true;
}
void ManagedEvents::reclaim(std::uint32_t index) noexcept {
    auto& r = rows_[index];
    backend_.clear(backend_.frame, r.event);
    r.live = false;
    r.marked = false;
    r.pins = 0;
    --live_;
    if (r.generation == std::numeric_limits<std::uint32_t>::max()) {
        r.next = kNone;
        ++retired_;
    } else {
        r.next = free_;
        free_ = index;
    }
}
ManagedEventError ManagedEventRoots::add(ManagedEventToken token) noexcept {
    if (token.bits == 0) return ManagedEventError::None;
    if (store_.row(token) == nullptr) {
        error_ = ManagedEventError::InvalidToken;
        return error_;
    }
    store_.rows_[static_cast<std::uint32_t>(token.bits) - 1].marked = true;
    return ManagedEventError::None;
}
ManagedEventError ManagedEvents::collect(std::span<const ManagedEventRootProvider> roots) noexcept {
    if (collecting_) return ManagedEventError::Busy;
    collecting_ = true;
    for (auto& r : rows_)
        r.marked = false;
    ManagedEventRoots sink{*this};
    for (const auto& provider : roots) {
        const auto error = provider.enumerate == nullptr ? ManagedEventError::InvalidBackend
                                                         : provider.enumerate(provider.frame, sink);
        if (error != ManagedEventError::None || sink.error_ != ManagedEventError::None) {
            collecting_ = false;
            eligible_ = true;
            return error != ManagedEventError::None ? error : sink.error_;
        }
    }
    for (std::uint32_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].live && !rows_[i].marked && rows_[i].pins == 0) reclaim(i);
    collecting_ = false;
    eligible_ = false;
    return ManagedEventError::None;
}
ManagedEventError ManagedEvents::reset() noexcept {
    if (collecting_) return ManagedEventError::Busy;
    for (std::uint32_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].live) reclaim(i);
    eligible_ = false;
    return ManagedEventError::None;
}

}  // namespace kirimasim::kernel
