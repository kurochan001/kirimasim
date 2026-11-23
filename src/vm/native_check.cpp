// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "native_check.h"
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/process_abi.h"
#include <algorithm>

namespace kirimasim::vm::detail {
void NativeCheck::reset(Budget budget, std::span<const kernel::W4> frame) {
    frame_.assign(frame.begin(), frame.end());
    referenceBranches_ = replayBranches_ = interruptedBranch_ = 0;
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
    if (!wrong_) wrong_ = slot;
    return scratch_.data();
}
void NativeCheck::observe(kernel::SlotBits at, const std::uint64_t* value, std::optional<Nba> nba) {
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
        reinterpret_cast<NativeCheck*>(ctx)->record({kernel::SlotId{s}, e, l, w}, v, {});
        return 1;
    };
    api.schedule_nba = [](kirima_context* ctx, std::uint32_t processId, std::uint32_t s,
                          std::uint32_t e, std::uint32_t l, std::uint32_t w, const std::uint64_t* v,
                          std::uint64_t delay) {
        reinterpret_cast<NativeCheck*>(ctx)->record({kernel::SlotId{s}, e, l, w}, v,
                                                    Nba{kernel::ProcessId{processId}, delay});
    };
    api.ending = [](kirima_context* ctx) -> int {
        const auto& trace = *reinterpret_cast<NativeCheck*>(ctx);
        return trace.end_ && trace.cursor_ >= *trace.end_;
    };
    // Replay the precise branch at which the reference VM took SIGINT or
    // reached its deadline. Reading the live clock again would move that stop.
    api.query_service = [](std::uint32_t id, std::uint32_t version) -> const void* {
        static constexpr auto service = [] {
            kirima_process_api table{};
            table.checkpoint = [](kirima_context* ctx, std::uint32_t kind,
                                  std::uint32_t* taken) -> std::uint32_t {
                if (kind != KIRIMA_TAKE_INTERRUPT) return KIRIMA_PROCESS_INVALID;
                auto& trace = *reinterpret_cast<NativeCheck*>(ctx);
                *taken = ++trace.replayBranches_ == trace.interruptedBranch_;
                return KIRIMA_PROCESS_OK;
            };
            return table;
        }();
        return id == KIRIMA_SERVICE_PROCESS && version == KIRIMA_PROCESS_VERSION ? &service
                                                                                 : nullptr;
    };
    kirima_model_activation frame{&api, process, 0, frame_.data(), nullptr};
    return fn(reinterpret_cast<kirima_context*>(this), &frame);
}
bool NativeCheck::matchesFrame(std::span<const kernel::W4> frame) const {
    if (frame.empty()) return frame_.empty();
    return referenceBranches_ == replayBranches_ &&
           std::ranges::equal(frame_, frame,
                              [](auto x, auto y) { return x.a == y.a && x.b == y.b; });
}
void NativeCheck::record(kernel::SlotBits at, const std::uint64_t* value, std::optional<Nba> nba) {
    const auto index = cursor_++;
    if (wrong_) return;
    if (index >= expected_.size() || expected_[index].slot != at.slot.index()) {
        wrong_ = at.slot.index();
        return;
    }
    const auto& e = expected_[index];
    const auto& layout = store_.layout(at.slot);
    const unsigned width = at.width ? at.width : layout.width;
    const unsigned stride = layout.domain == kernel::Domain::Four ? 2 : 1;
    if (e.nba != nba || e.element != at.elem || e.lsb != at.lsb || e.width != width) {
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
