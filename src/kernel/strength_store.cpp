// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/strength_store.h"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <tuple>

#include "kirimasim/kernel/bits.h"

namespace kirimasim::kernel {
namespace {
bool valid(StrengthRange value) noexcept {
    return value.lo >= -7 && value.hi <= 7 && value.lo <= value.hi;
}
}  // namespace

bool StrengthStorage::normalize(SlotBits& at) const noexcept {
    if (!registered(at.slot)) return false;
    const auto& layout = store_.layout(at.slot);
    if (at.width == 0) {
        at.lsb = 0;
        at.width = layout.width;
    }
    return at.elem < layout.elements && at.lsb <= layout.width && at.width <= layout.width - at.lsb;
}

StrengthError StrengthStorage::enable(SlotId slot,
                                      std::span<const StrengthRange> initial) noexcept {
    if (slot.index() >= store_.size() || registered(slot)) return StrengthError::InvalidTarget;
    const auto layout = store_.layout(slot);
    if (layout.domain != Domain::Four || layout.width == 0 || layout.elements == 0)
        return StrengthError::InvalidTarget;
    const auto count = std::uint64_t{layout.width} * layout.elements;
    if (count != initial.size()) return StrengthError::InvalidShape;
    for (std::uint32_t e = 0; e < layout.elements; ++e) {
        const auto* words = store_.element(slot, e);
        for (std::uint32_t bit = 0; bit < layout.width; ++bit) {
            const auto range = initial[std::uint64_t{e} * layout.width + bit];
            if (!valid(range)) return StrengthError::InvalidValue;
            const auto value = strengthValue(range);
            if (value.a != bitAt(words, 2, bit) || value.b != bitAt(words + 1, 2, bit))
                return StrengthError::InvalidValue;
        }
    }
    try {
        if (slots_.size() >= UINT32_MAX) return StrengthError::ResourceLimit;
        SlotState state(memory_);
        state.slot = slot;
        state.values.assign(initial.begin(), initial.end());
        if (bySlot_.size() < store_.size()) bySlot_.resize(store_.size(), kNone);
        const auto id = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back(std::move(state));
        bySlot_[slot.index()] = id;
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        return StrengthError::ResourceLimit;
    }
}

StrengthError StrengthStorage::read(SlotBits at, std::span<StrengthRange> out) const noexcept {
    if (!normalize(at)) return StrengthError::InvalidTarget;
    if (out.size() < at.width) return StrengthError::InvalidShape;
    const auto& values = slots_[bySlot_[at.slot.index()]].values;
    const auto first = std::uint64_t{at.elem} * store_.layout(at.slot).width + at.lsb;
    std::copy_n(values.begin() + first, at.width, out.begin());
    return StrengthError::None;
}

StrengthError StrengthStorage::readPacked(SlotBits at, std::span<W4> out) const noexcept {
    if (!normalize(at)) return StrengthError::InvalidTarget;
    if (out.size() < strengthWordCount(at.width)) return StrengthError::InvalidShape;
    const auto& values = slots_[bySlot_[at.slot.index()]].values;
    const auto first = std::uint64_t{at.elem} * store_.layout(at.slot).width + at.lsb;
    std::fill_n(out.begin(), strengthWordCount(at.width), W4{});
    for (std::uint32_t bit = 0; bit < at.width; ++bit)
        writeStrengthRange(out, bit, values[first + bit]);
    return StrengthError::None;
}

StrengthWatchResult StrengthStorage::subscribe(SlotBits at, StrengthNoticeFn fn,
                                               void* frame) noexcept {
    if (!normalize(at) || fn == nullptr) return {StrengthError::InvalidTarget};
    if (nextSerial_ == UINT64_MAX) return {StrengthError::ResourceLimit};
    try {
        std::uint32_t id = freeWatch_;
        if (id == kNone) {
            if (watches_.size() >= UINT32_MAX) return {StrengthError::ResourceLimit};
            id = static_cast<std::uint32_t>(watches_.size());
            watches_.emplace_back();
        } else
            freeWatch_ = watches_[id].next;
        auto& watch = watches_[id];
        watch.at = at;
        watch.fn = fn;
        watch.frame = frame;
        watch.serial = nextSerial_++;
        watch.next = kNone;
        watch.active = true;
        ++liveWatches_;
        return {StrengthError::None, {id, watch.generation}};
    } catch (const std::bad_alloc&) {
        return {StrengthError::ResourceLimit};
    } catch (const std::length_error&) {
        return {StrengthError::ResourceLimit};
    }
}

bool StrengthStorage::cancel(StrengthWatchToken token) noexcept {
    if (token.row >= watches_.size()) return false;
    auto& watch = watches_[token.row];
    if (!watch.active || watch.generation != token.generation) return false;
    watch.active = false;
    watch.fn = nullptr;
    watch.frame = nullptr;
    --liveWatches_;
    if (watch.generation != UINT64_MAX) {
        ++watch.generation;
        watch.next = freeWatch_;
        freeWatch_ = token.row;
    }
    return true;
}
void StrengthStorage::clearWatches() noexcept {
    for (std::uint32_t i = 0; i < watches_.size(); ++i)
        cancel({i, watches_[i].generation});
}

StrengthError StrengthStorage::prepare(std::span<const StrengthUpdate> updates, Plan& plan) {
    if (updates.size() > UINT32_MAX) return StrengthError::ResourceLimit;
    // Validate every input before staging, then capture all caller buffers.
    for (const auto& update : updates) {
        auto at = update.at;
        if (!normalize(at)) return StrengthError::InvalidTarget;
        if (update.values.size() != at.width) return StrengthError::InvalidShape;
        for (const auto range : update.values)
            if (!valid(range)) return StrengthError::InvalidValue;
    }
    plan.watchCutoff = nextSerial_;
    plan.parts.reserve(updates.size());
    plan.order.reserve(updates.size());
    plan.elements.reserve(updates.size());
    for (const auto& update : updates) {
        plan.order.push_back(static_cast<std::uint32_t>(plan.parts.size()));
        auto& part = plan.parts.emplace_back(memory_);
        part.at = update.at;
        normalize(part.at);
        part.state = bySlot_[part.at.slot.index()];
        part.after.assign(update.values.begin(), update.values.end());
        const auto first =
            std::uint64_t{part.at.elem} * store_.layout(part.at.slot).width + part.at.lsb;
        const auto& ranges = slots_[part.state].values;
        part.before.assign(ranges.begin() + first, ranges.begin() + first + part.at.width);
        part.rangeChanged = part.before != part.after;
    }
    std::sort(plan.order.begin(), plan.order.end(), [&](auto a, auto b) {
        const auto& x = plan.parts[a].at;
        const auto& y = plan.parts[b].at;
        return std::tuple{x.slot.index(), x.elem, x.lsb} <
               std::tuple{y.slot.index(), y.elem, y.lsb};
    });
    for (std::size_t i = 0; i < plan.order.size(); ++i) {
        auto& part = plan.parts[plan.order[i]];
        bool sameElement = false;
        if (i != 0) {
            const auto& prev = plan.parts[plan.order[i - 1]].at;
            sameElement = part.at.slot == prev.slot && part.at.elem == prev.elem;
            if (sameElement && part.at.lsb < prev.lsb + prev.width)
                return StrengthError::InvalidShape;
        }
        if (!sameElement) {
            auto& element = plan.elements.emplace_back(memory_);
            element.slot = part.at.slot;
            element.elem = part.at.elem;
            const auto* words = store_.element(element.slot, element.elem);
            element.before.assign(words, words + store_.layout(element.slot).elemWords);
            element.after = element.before;
        }
        part.element = static_cast<std::uint32_t>(plan.elements.size() - 1);
        auto& element = plan.elements.back();
        for (std::uint32_t bit = 0; bit < part.at.width; ++bit) {
            const auto value = strengthValue(part.after[bit]);
            writeField(element.after.data(), 2, std::uint64_t{part.at.lsb} + bit, 1, value.a);
            writeField(element.after.data() + 1, 2, std::uint64_t{part.at.lsb} + bit, 1, value.b);
        }
        part.valueChanged = !equalBits(element.before.data(), 2, part.at.lsb, element.after.data(),
                                       2, part.at.lsb, part.at.width) ||
                            !equalBits(element.before.data() + 1, 2, part.at.lsb,
                                       element.after.data() + 1, 2, part.at.lsb, part.at.width);
    }
    return StrengthError::None;
}

void StrengthStorage::commit(const Plan& plan) noexcept {
    for (const auto& part : plan.parts) {
        auto& ranges = slots_[part.state].values;
        const auto first =
            std::uint64_t{part.at.elem} * store_.layout(part.at.slot).width + part.at.lsb;
        std::copy(part.after.begin(), part.after.end(), ranges.begin() + first);
    }
    for (const auto& element : plan.elements)
        std::copy(element.after.begin(), element.after.end(),
                  store_.element(element.slot, element.elem));
}

void StrengthStorage::announce(const Plan& plan, const bool& stop) noexcept {
    for (const auto& part : plan.parts) {
        if (!part.rangeChanged) continue;
        const auto count = watches_.size();
        for (std::size_t i = 0; i < count; ++i) {
            if (stop) return;
            const auto watch = watches_[i];  // Callback may grow or recycle the table.
            if (!watch.active || watch.serial >= plan.watchCutoff ||
                watch.at.slot != part.at.slot || watch.at.elem != part.at.elem)
                continue;
            const auto first = std::max(watch.at.lsb, part.at.lsb);
            const auto end = std::min(watch.at.lsb + watch.at.width, part.at.lsb + part.at.width);
            if (first >= end) continue;
            const auto before = std::span<const StrengthRange>(part.before)
                                    .subspan(first - part.at.lsb, end - first);
            const auto after = std::span<const StrengthRange>(part.after)
                                   .subspan(first - part.at.lsb, end - first);
            if (!std::equal(before.begin(), before.end(), after.begin()))
                watch.fn(watch.frame, {part.at.slot, part.at.elem, first, end - first}, before,
                         after);
        }
    }
}
}  // namespace kirimasim::kernel
