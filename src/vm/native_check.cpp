// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "native_check.h"
#include "kirimasim/kernel/bits.h"
#include <algorithm>

namespace kirimasim::vm::detail {
void NativeCheck::reset(Budget budget) {
    reads_.clear();
    expected_.clear();
    words_.clear();
    values_.clear();
    readStart_ = 0;
    cursor_ = 0;
    end_.reset();
    wrong_.reset();
    scratch_.resize(budget.widest);
    reads_.reserve(budget.accesses);
    expected_.reserve(budget.accesses);
    words_.reserve(budget.words);
    values_.reserve(budget.words);
}
void NativeCheck::read(kernel::SlotId slot, std::uint32_t ordinal) {
    // A cached projection can replace several loads in one effect interval.
    // A synchronous observer of a write can change even an unrelated input.
    for (auto i = reads_.size(); i && reads_[i - 1].epoch == expected_.size(); --i)
        if (reads_[i - 1].slot == slot.index() && reads_[i - 1].ordinal == ordinal) return;
    const auto* data = store_.element(slot, ordinal);
    reads_.push_back({slot.index(), ordinal, expected_.size(), words_.size()});
    words_.insert(words_.end(), data, data + store_.layout(slot).elemWords);
}
std::uint64_t* NativeCheck::element(std::uint32_t slot, std::uint32_t ordinal) {
    while (readStart_ < reads_.size() && reads_[readStart_].epoch < cursor_)
        ++readStart_;
    for (auto i = readStart_; i < reads_.size() && reads_[i].epoch == cursor_; ++i)
        if (reads_[i].slot == slot && reads_[i].ordinal == ordinal)
            return words_.data() + reads_[i].offset;
    wrong_ = slot;
    return scratch_.data();
}
void NativeCheck::observe(bool nba, kernel::SlotBits at, const std::uint64_t* value) {
    const auto& layout = store_.layout(at.slot);
    const unsigned width = at.width ? at.width : layout.width;
    const unsigned stride = layout.domain == kernel::Domain::Four ? 2 : 1;
    const auto n = std::size_t{kernel::wordsFor(width)} * stride;
    expected_.push_back({nba, at.slot.index(), at.elem, at.lsb, width, values_.size()});
    values_.insert(values_.end(), value, value + n);
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
    api.ending = [](kirima_context* ctx) -> int {
        const auto& trace = *reinterpret_cast<NativeCheck*>(ctx);
        return trace.end_ && trace.cursor_ >= *trace.end_;
    };
    kirima_model_activation frame{&api, process, 0, nullptr, nullptr};
    return fn(reinterpret_cast<kirima_context*>(this), &frame);
}
void NativeCheck::record(bool nba, kernel::SlotBits at, const std::uint64_t* value) {
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
