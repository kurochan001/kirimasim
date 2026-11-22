// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "native_check.h"
#include "kirimasim/kernel/w4ops.h"
#include <algorithm>

namespace kirimasim::vm::detail {
NativeCheck::NativeCheck(kernel::Store& store, std::size_t maxAccesses) : store_(store) {
    // Emitted bodies have acyclic control flow and each instruction accesses
    // at most one slot. Allocate outside the C callback boundary.
    overlay_.reserve(maxAccesses);
    expected_.reserve(maxAccesses);
}
NativeCheck::Word& NativeCheck::element(std::uint32_t slot, std::uint32_t elem) {
    const auto found = std::ranges::find_if(
        overlay_, [&](const Cell& c) { return c.slot == slot && c.ordinal == elem; });
    if (found != overlay_.end()) return found->words;
    if (overlay_.size() == overlay_.capacity()) {
        wrong_ = slot;
        return scratch_;
    }
    const kernel::SlotId id{slot};
    const auto* words = store_.element(id, elem);
    overlay_.push_back(
        {slot, elem, {words[0], store_.layout(id).domain == kernel::Domain::Four ? words[1] : 0}});
    return overlay_.back().words;
}
NativeCheck::Effect NativeCheck::effect(bool nba, kernel::SlotBits at, kernel::W4 v) const {
    const auto& layout = store_.layout(at.slot);
    const auto width = at.width ? at.width : layout.width;
    const auto mask = kernel::mask(width);
    if (layout.domain == kernel::Domain::Two) v = kernel::to2(v);
    return {nba, at.slot.index(), at.elem, at.lsb, width, v.a & mask, v.b & mask};
}
void NativeCheck::record(bool nba, kernel::SlotBits at, const std::uint64_t* value) {
    const bool four = store_.layout(at.slot).domain == kernel::Domain::Four;
    const auto e = effect(nba, at, {value[0], four ? value[1] : 0});
    if (expected_.size() == expected_.capacity()) {
        wrong_ = e.slot;
        return;
    }
    expected_.push_back(e);
    if (!nba) {
        auto& w = element(e.slot, e.element);
        const auto bits = kernel::mask(e.width) << e.lsb;
        w[0] = (w[0] & ~bits) | (e.a << e.lsb);
        w[1] = (w[1] & ~bits) | (e.b << e.lsb);
    }
}
kirima_status NativeCheck::evaluate(kirima_resume_fn fn, std::uint32_t process) {
    kirima_kernel_api api{};
    api.slot_words = [](kirima_context* ctx, std::uint32_t s, std::uint32_t e) {
        return reinterpret_cast<NativeCheck*>(ctx)->element(s, e).data();
    };
    api.write = [](kirima_context* ctx, std::uint32_t s, std::uint32_t e, std::uint32_t l,
                   std::uint32_t w, const std::uint64_t* v) {
        reinterpret_cast<NativeCheck*>(ctx)->record(false, {kernel::SlotId{s}, e, l, w}, v);
        return 1;
    };
    api.schedule_nba = [](kirima_context* ctx, std::uint32_t, std::uint32_t s, std::uint32_t e,
                          std::uint32_t l, std::uint32_t w, const std::uint64_t* v, std::uint64_t) {
        reinterpret_cast<NativeCheck*>(ctx)->record(true, {kernel::SlotId{s}, e, l, w}, v);
    };
    api.ending = [](kirima_context*) { return 0; };
    kirima_model_activation frame{&api, process, 0, nullptr, nullptr};
    return fn(reinterpret_cast<kirima_context*>(this), &frame);
}
void NativeCheck::observe(bool nba, kernel::SlotBits at, kernel::W4 value) {
    const auto actual = effect(nba, at, value);
    if (!wrong_ && (cursor_ >= expected_.size() || expected_[cursor_] != actual))
        wrong_ = actual.slot;
    ++cursor_;
}
std::optional<std::uint32_t> NativeCheck::mismatch() const {
    if (wrong_) return wrong_;
    if (cursor_ < expected_.size()) return expected_[cursor_].slot;
    return {};
}
}  // namespace kirimasim::vm::detail
