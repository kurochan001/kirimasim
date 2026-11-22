// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "native_check.h"
#include "kirimasim/kernel/bits.h"
#include <algorithm>

namespace kirimasim::vm::detail {
NativeCheck::NativeCheck(kernel::Store& store, Budget budget)
    : store_(store), words_(budget.words), scratch_(budget.widest), values_(budget.words) {
    // Emitted bodies have acyclic control flow and each instruction accesses
    // at most one slot. Allocate outside the C callback boundary.
    overlay_.reserve(budget.accesses);
    expected_.reserve(budget.accesses);
}
std::uint64_t* NativeCheck::element(std::uint32_t slot, std::uint32_t elem) {
    const auto found = std::ranges::find_if(
        overlay_, [&](const Cell& c) { return c.slot == slot && c.ordinal == elem; });
    if (found != overlay_.end()) return words_.data() + found->offset;
    const kernel::SlotId id{slot};
    const auto size = store_.layout(id).elemWords;
    if (overlay_.size() == overlay_.capacity() || size > words_.size() - used_) {
        wrong_ = slot;
        return scratch_.data();
    }
    auto* out = words_.data() + used_;
    std::copy_n(store_.element(id, elem), size, out);
    overlay_.push_back({slot, elem, used_});
    used_ += size;
    return out;
}
void NativeCheck::record(bool nba, kernel::SlotBits at, const std::uint64_t* value) {
    const auto& layout = store_.layout(at.slot);
    const unsigned width = at.width ? at.width : layout.width;
    const unsigned stride = layout.domain == kernel::Domain::Four ? 2 : 1;
    const auto n = std::size_t{kernel::wordsFor(width)} * stride;
    if (expected_.size() == expected_.capacity() || n > values_.size() - saved_) {
        wrong_ = at.slot.index();
        return;
    }
    expected_.push_back({nba, at.slot.index(), at.elem, at.lsb, width, saved_});
    auto* copy = values_.data() + saved_;
    saved_ += n;
    std::copy_n(value, n, copy);
    const auto mask = kernel::mask((width - 1) % 64 + 1);
    for (unsigned plane = 0; plane < stride; ++plane)
        copy[n - stride + plane] &= mask;
    if (!nba) {
        auto* target = element(at.slot.index(), at.elem);
        for (unsigned plane = 0; plane < stride; ++plane)
            kernel::copyBits(target + plane, stride, at.lsb, copy + plane, stride, 0, width);
    }
}
kirima_status NativeCheck::evaluate(kirima_resume_fn fn, std::uint32_t process) {
    kirima_kernel_api api{};
    api.slot_words = [](kirima_context* ctx, std::uint32_t s, std::uint32_t e) {
        return reinterpret_cast<NativeCheck*>(ctx)->element(s, e);
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
void NativeCheck::observe(bool nba, kernel::SlotBits at, const std::uint64_t* value) {
    const auto& layout = store_.layout(at.slot);
    const unsigned width = at.width ? at.width : layout.width;
    const unsigned stride = layout.domain == kernel::Domain::Four ? 2 : 1;
    const auto index = cursor_++;
    if (wrong_) return;
    if (index >= expected_.size()) {
        wrong_ = at.slot.index();
        return;
    }
    const auto& e = expected_[index];
    if (e.nba != nba || e.slot != at.slot.index() || e.element != at.elem || e.lsb != at.lsb ||
        e.width != width) {
        wrong_ = at.slot.index();
        return;
    }
    for (unsigned plane = 0; plane < stride; ++plane)
        if (!kernel::equalBits(values_.data() + e.valueOffset + plane, stride, 0, value + plane,
                               stride, 0, width))
            wrong_ = at.slot.index();
}
std::optional<std::uint32_t> NativeCheck::mismatch() const {
    if (wrong_) return wrong_;
    if (cursor_ < expected_.size()) return expected_[cursor_].slot;
    return {};
}
}  // namespace kirimasim::vm::detail
