// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Complete published net intervals and internal notices. MOS/switch
// contract 1/6/7. Context owned; C++20, independent of SIR.
#pragma once

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/strength_value.h"

namespace kirimasim::kernel {
class Scheduler;

enum class StrengthError : std::uint8_t {
    None,
    InvalidTarget,
    InvalidShape,
    InvalidValue,
    ResourceLimit,
    Reentrant,
    UnsupportedHold
};
struct StrengthUpdate {
    SlotBits at;
    std::span<const StrengthRange> values;
};
struct StrengthPublication {
    StrengthError error = StrengthError::None;
    bool committed = false;
    bool changed = false;  ///< A complete range changed, even if its value did not.
};
struct StrengthWatchToken {
    std::uint32_t row = kNone;
    std::uint64_t generation = 0;
    friend bool operator==(const StrengthWatchToken&, const StrengthWatchToken&) = default;
};
struct StrengthWatchResult {
    StrengthError error = StrengthError::None;
    StrengthWatchToken token;
};
// Snapshot spans name the changed intersection and live only for the call.
// Internal callbacks queue dependent work; they do not synchronously publish
// another component. They must not throw. Cancellation/growth is permitted.
using StrengthNoticeFn = void (*)(void*, SlotBits, std::span<const StrengthRange>,
                                  std::span<const StrengthRange>) noexcept;

class StrengthStorage {
public:
    explicit StrengthStorage(Store& store, std::pmr::memory_resource* memory) noexcept
        : store_(store), memory_(memory), slots_(memory), bySlot_(memory), watches_(memory) {}
    StrengthStorage(const StrengthStorage&) = delete;
    StrengthStorage& operator=(const StrengthStorage&) = delete;

    [[nodiscard]] bool registered(SlotId slot) const noexcept {
        return slot.index() < bySlot_.size() && bySlot_[slot.index()] != kNone;
    }
    [[nodiscard]] std::size_t slotCount() const noexcept { return slots_.size(); }
    [[nodiscard]] std::size_t watchRows() const noexcept { return watches_.size(); }
    [[nodiscard]] std::uint32_t liveWatches() const noexcept { return liveWatches_; }
    StrengthError read(SlotBits at, std::span<StrengthRange> out) const noexcept;
    StrengthError readPacked(SlotBits at, std::span<W4> out) const noexcept;
    /// Captured runtime selection within a static parent bit window. An
    /// invalid element/position reads strong X; partially outside selections
    /// retain in-range metadata and use strong X only for missing bits.
    StrengthError readSelectedPacked(SlotBits window, std::optional<std::uint32_t> element,
                                     std::optional<std::int64_t> position, std::uint32_t width,
                                     std::span<W4> out) const noexcept;
    StrengthWatchResult subscribe(SlotBits at, StrengthNoticeFn fn, void* frame) noexcept;
    bool cancel(StrengthWatchToken token) noexcept;
    void clearWatches() noexcept;

private:
    friend class Scheduler;
    struct SlotState {
        explicit SlotState(std::pmr::memory_resource* memory) : values(memory) {}
        SlotState(SlotState&&) noexcept = default;
        SlotId slot{0};
        std::pmr::vector<StrengthRange> values;
    };
    struct Watch {
        SlotBits at;
        StrengthNoticeFn fn = nullptr;
        void* frame = nullptr;
        Scheduler* scheduler = nullptr;
        std::uint32_t process = kNone;
        std::uint64_t waitGeneration = 0;
        std::uint64_t generation = 1, serial = 0;
        std::uint32_t next = kNone;
        bool active = false;
    };
    struct Part {
        explicit Part(std::pmr::memory_resource* memory) : before(memory), after(memory) {}
        Part(Part&&) noexcept = default;
        SlotBits at;
        std::uint32_t state = 0, element = 0;
        std::pmr::vector<StrengthRange> before, after;
        bool rangeChanged = false, valueChanged = false;
    };
    struct Element {
        explicit Element(std::pmr::memory_resource* memory) : before(memory), after(memory) {}
        Element(Element&&) noexcept = default;
        SlotId slot{0};
        std::uint32_t elem = 0;
        std::pmr::vector<std::uint64_t> before, after;
    };
    struct Plan {
        explicit Plan(std::pmr::memory_resource* memory)
            : parts(memory), elements(memory), order(memory) {}
        std::pmr::vector<Part> parts;
        std::pmr::vector<Element> elements;
        std::pmr::vector<std::uint32_t> order;
        std::uint64_t watchCutoff = 0;
    };
    Store& store_;
    std::pmr::memory_resource* memory_;
    std::pmr::vector<SlotState> slots_;
    std::pmr::vector<std::uint32_t> bySlot_;
    std::pmr::vector<Watch> watches_;
    std::uint32_t freeWatch_ = kNone, liveWatches_ = 0;
    std::uint64_t nextSerial_ = 0;
    bool publishing_ = false;

    StrengthWatchResult subscribeImpl(SlotBits at, StrengthNoticeFn fn, void* frame,
                                      Scheduler* scheduler, std::uint32_t process,
                                      std::uint64_t generation) noexcept;
    bool normalize(SlotBits& at) const noexcept;
    StrengthError enable(SlotId slot, std::span<const StrengthRange> initial) noexcept;
    StrengthError prepare(std::span<const StrengthUpdate> updates, Plan& plan);
    void commit(const Plan& plan) noexcept;
    void announce(const Plan& plan, const bool& stop) noexcept;
};
}  // namespace kirimasim::kernel
