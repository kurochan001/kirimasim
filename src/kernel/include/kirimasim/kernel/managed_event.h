// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Managed named-event identity (automatic-capture contract 5/6/8).
// C++20. Only declared event values are roots; integers are not traced.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

#include "kirimasim/kernel/id.h"

namespace kirimasim::kernel {

struct EventTag;
using EventId = Id<EventTag>;
struct ManagedEventToken {
    std::uint64_t bits = 0;
    friend bool operator==(ManagedEventToken, ManagedEventToken) = default;
};
enum class ManagedEventError : std::uint8_t {
    None,
    Null,
    InvalidToken,
    InvalidBackend,
    Busy,
    ResourceLimit
};
template <class T> struct ManagedEventResult {
    T value{};
    ManagedEventError error = ManagedEventError::None;
    explicit operator bool() const noexcept { return error == ManagedEventError::None; }
};
struct ManagedEventBackend {
    void* frame = nullptr;
    ManagedEventResult<EventId> (*create)(void*) noexcept = nullptr;
    void (*clear)(void*, EventId) noexcept = nullptr;
};
class ManagedEvents;

/// A runtime pin or unpublished temporary. Typed source holders are traced
/// at safe points; pinning does not make a new source-language variable.
/// Pins and tokens belong to one Context and must not outlive that Context.
class ManagedEventPin {
public:
    ManagedEventPin() noexcept = default;
    ManagedEventPin(const ManagedEventPin&) = delete;
    ManagedEventPin& operator=(const ManagedEventPin&) = delete;
    ManagedEventPin(ManagedEventPin&& other) noexcept;
    ManagedEventPin& operator=(ManagedEventPin&& other) noexcept;
    ~ManagedEventPin();
    void reset() noexcept;
    [[nodiscard]] ManagedEventToken token() const noexcept { return token_; }
    explicit operator bool() const noexcept;

private:
    friend class ManagedEvents;
    ManagedEventPin(ManagedEvents* store, ManagedEventToken token) noexcept
        : store_(store), token_(token) {}
    ManagedEvents* store_ = nullptr;
    ManagedEventToken token_;
};

/// A borrowed sink for trusted, nonallocating typed-root enumerators. They
/// must not execute source code, mutate storage or release runtime pins.
class ManagedEventRoots {
public:
    ManagedEventError add(ManagedEventToken token) noexcept;

private:
    friend class ManagedEvents;
    explicit ManagedEventRoots(ManagedEvents& store) noexcept : store_(store) {}
    ManagedEvents& store_;
    ManagedEventError error_ = ManagedEventError::None;
};
struct ManagedEventRootProvider {
    const void* frame = nullptr;
    ManagedEventError (*enumerate)(const void*, ManagedEventRoots&) noexcept = nullptr;
};

/// Reuses scheduler objects only after typed holders and runtime pins end.
/// A collector call is a safe point: no unpublished source temporary may
/// exist without a pin. Providers enumerate typed holders, including unread
/// fields of retained automatic scopes. Legacy static handles are not tokens
/// of this registry; adapters keep their existing representation separately.
class ManagedEvents {
public:
    explicit ManagedEvents(std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
        : rows_(memory) {}
    ManagedEvents(const ManagedEvents&) = delete;
    ManagedEvents& operator=(const ManagedEvents&) = delete;
    // Destruction frees bookkeeping; the Context owns the scheduler objects.
    // It does not call a backend that may already have been destroyed.
    ManagedEventError setBackend(ManagedEventBackend backend) noexcept;
    ManagedEventResult<ManagedEventPin> create() noexcept;
    ManagedEventResult<ManagedEventPin> retain(ManagedEventToken token) noexcept;
    ManagedEventResult<ManagedEventPin> retain(const ManagedEventPin& pin) noexcept;
    [[nodiscard]] ManagedEventResult<EventId> resolve(ManagedEventToken token) const noexcept;
    [[nodiscard]] ManagedEventResult<EventId> resolve(const ManagedEventPin& pin) const noexcept;
    [[nodiscard]] bool valid(ManagedEventToken token) const noexcept;
    ManagedEventError collect(std::span<const ManagedEventRootProvider> roots = {}) noexcept;
    /// Invalidate all current values/pins; reused rows keep advancing generations.
    ManagedEventError reset() noexcept;
    void rootsChanged() noexcept { eligible_ = true; }
    [[nodiscard]] bool collectionEligible() const noexcept { return eligible_; }
    [[nodiscard]] std::size_t liveObjects() const noexcept { return live_; }
    [[nodiscard]] std::size_t allocatedObjects() const noexcept { return rows_.size(); }
    [[nodiscard]] std::size_t retiredObjects() const noexcept { return retired_; }

private:
    friend class ManagedEventPin;
    friend class ManagedEventRoots;
    struct Row {
        EventId event = kNone;
        std::uint64_t pins = 0;
        std::uint32_t generation = 0, next = kNone;
        bool live = false, marked = false;
    };
    std::pmr::vector<Row> rows_;
    ManagedEventBackend backend_;
    std::uint32_t free_ = kNone;
    std::size_t live_ = 0, retired_ = 0;
    bool collecting_ = false, eligible_ = false;
    [[nodiscard]] const Row* row(ManagedEventToken token) const noexcept;
    void release(ManagedEventToken token) noexcept;
    void reclaim(std::uint32_t index) noexcept;
};

}  // namespace kirimasim::kernel
