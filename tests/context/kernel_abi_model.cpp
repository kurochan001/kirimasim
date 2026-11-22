// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kernel_abi_model.h"

#include <cstddef>
#include <cstdint>

static_assert(sizeof(kirima_slot_layout) == 24 && alignof(kirima_slot_layout) == 8);
static_assert(offsetof(kirima_slot_layout, offset) == 16);
// The first 152 bytes are the 0.3 prefix on the supported 64-bit targets.
static_assert(offsetof(kirima_kernel_api, slot_layout) == 152);
static_assert(offsetof(kirima_kernel_api, domain_words) == 160);

#include "kirimasim/kernel/process_control.h"

namespace {
bool checkForks() {
    using namespace kirimasim::kernel;
    int probes = 0;
    auto retained = [&] {
        ++probes;
        return true;
    };
    if (frameRelease(false, true, 1, 0, false, retained) != FrameRelease::Keep || probes ||
        frameRelease(false, true, 0, 0, false, retained) != FrameRelease::Retry || probes != 1 ||
        frameRelease(false, true, 0, 0, false, [] { return false; }) != FrameRelease::Release)
        return false;
    ForkState join;
    join.begin(JoinMode::All, 1);
    const auto child = join.born(JoinMode::All);
    bool flushed = true;
    if (!suspendForJoin(
            join, [] { return 1; }, [&](int, bool flush) { flushed = flush; }) ||
        flushed)
        return false;
    join.ended(child);
    if (suspendForJoin(join, [] { return 1; }, [&](int, bool) { flushed = true; }) || flushed)
        return false;
    ForkState parent;
    parent.begin(JoinMode::Any, 0);
    if (!parent.continueAfterJoin() || !parent.canFork(JoinMode::All, 2)) return false;
    parent.begin(JoinMode::All, 2);
    const auto first = parent.born(JoinMode::All);
    const auto second = parent.born(JoinMode::All);
    const auto detached = parent.born(JoinMode::None);
    if (first.order != 0 || second.order != 1 || detached.group ||
        parent.canFork(JoinMode::All, UINT32_MAX))
        return false;
    parent.ended(first);
    if (parent.continueAfterJoin()) return false;
    parent.ended(second);
    if (!parent.continueAfterJoin()) return false;
    parent.waitFork();
    if (parent.continueAfterJoin()) return false;
    parent.ended(detached);
    return parent.continueAfterJoin() && parent.births() == 3;
}
kirima_status resume(kirima_context* context, void* raw) {
    auto& frame = *static_cast<model_frame*>(raw);
    if (frame.resumes >= 2) return KIRIMA_ERROR;
    const auto& api = *frame.api;
    frame.times[frame.resumes++] = api.time(context);
    if (frame.resumes == 1) {
        const std::uint64_t value[]{5, 12, 1, 1};
        api.schedule_nba(context, frame.process, 1, 1, 0, 0, value, 2);
        api.wait_delay(context, frame.process, 3);
        return KIRIMA_WAITING;
    }
    kirima_slot_layout row{};
    if (!api.slot_layout(context, 1, &row) || row.domain != KIRIMA_DOMAIN_FOUR || row.width != 65 ||
        row.elements != 2 || row.element_words != 4 || row.offset != 2)
        return KIRIMA_ERROR;
    std::uint64_t count = 0;
    const auto* words = api.domain_words(context, row.domain, &count);
    if (words == nullptr || count != row.offset + std::uint64_t{row.elements} * row.element_words)
        return KIRIMA_ERROR;
    for (unsigned i = 0; i < 4; ++i)
        frame.sample[i] = words[row.offset + row.element_words + i];
    return KIRIMA_DONE;
}
}  // namespace

extern "C" int start_abi_model(const kirima_kernel_api* api, kirima_context* context,
                               model_frame* frame) {
    if (api == nullptr || api->abi_major != KIRIMA_ABI_MAJOR || api->abi_minor < 4) return 0;
    if (!checkForks()) return 0;
    frame->api = api;
    frame->process = api->add_process(context, &resume, frame, KIRIMA_GROUP_ACTIVE);
    api->start(context, frame->process);
    return 1;
}
