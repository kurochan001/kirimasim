// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kernel_abi_model.h"

#include <array>
#include <cstddef>
#include <cstdint>

static_assert(sizeof(kirima_slot_layout) == 24 && alignof(kirima_slot_layout) == 8);
static_assert(offsetof(kirima_slot_layout, offset) == 16);
// The first 152 bytes are the 0.3 prefix on the supported 64-bit targets.
static_assert(offsetof(kirima_kernel_api, slot_layout) == 152);
static_assert(offsetof(kirima_kernel_api, domain_words) == 160);

#include "kirimasim/kernel/disable_plan.h"
#include "kirimasim/kernel/process_control.h"

namespace {
bool checkDisable() {
    using namespace kirimasim::kernel;
    struct Scope {
        unsigned scope, calls, sync, exit;
        std::uint64_t children;
    };
    struct Call {
        unsigned body;
        std::uint64_t children;
    };
    const std::array<Scope, 3> scopes{{{5, 0, 0, 6, 2}, {8, 1, 0, 9, 4}, {5, 1, 2, 11, 7}}};
    const std::array<Call, 2> tasks{{{13, 3}, {13, 5}}};
    const std::array<Call, 2> sync{{{7, 11}, {9, 13}}};
    const auto scope = planDisable(false, 5, 0u, UINT32_MAX, true, scopes, tasks, sync);
    const auto task = planDisable(true, 0, 13u, UINT32_MAX, false, scopes, tasks, sync);
    const auto inner = planDisable(true, 0, 9u, UINT32_MAX, true, scopes, tasks, sync);
    return scope && scope->scope && scope->keep == 0 && scope->children == 2 &&
           scope->to.exit == 6 && task && !task->scope && task->keep == 1 && task->children == 3 &&
           task->to.at(0, 0) && task->to.exit == UINT32_MAX && inner && inner->to.at(2, 1) &&
           !planDisable(true, 0, 9u, UINT32_MAX, false, scopes, tasks, sync) &&
           !mayUnwind(inner->to, true, false, 2, 1) && !mayUnwind(inner->to, true, true, 2, 2) &&
           mayUnwind(inner->to, true, true, 2, 1) &&
           disableActivity(true, true, true, false) == DisableActivity::Error &&
           disableActivity(true, true, true, true) == DisableActivity::EndTree;
}
bool checkForks() {
    using namespace kirimasim::kernel;
    std::uint64_t used = UINT64_MAX - 1;
    if (!stepsFit(0, 12, 0) || stepsFit(used, 2, UINT64_MAX) || !takeStep(used, UINT64_MAX) ||
        takeStep(used, UINT64_MAX) || used != UINT64_MAX || !callDepthFull(4, 3, 7) ||
        callDepthFull(4, 2, 7))
        return false;
    if (!checkDisable()) return false;
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
