// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/analysis/tables.h"
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/value_abi.h"
#include "kirimasim/sir/frame_layout.h"
#include "kirimasim/sir/text_v1.h"
#include "kirimasim/vm/vm.h"
#include <algorithm>
#include <array>
#include <dlfcn.h>
#include <memory>
#include <vector>

namespace {
void field(void* raw, const kirimasim::sir::FrameVar&,
           const kirimasim::sir::FrameFieldLayout& row) {
    static_cast<std::vector<kirima_model_field>*>(raw)->push_back(
        {row.offset.value_or(UINT64_MAX), row.logicalOffset, row.size, row.elements, row.id.index(),
         row.chunksPerElement});
}
}  // namespace
bool checkLoadableModule(const kirima_kernel_api* api, const char* file) {
    namespace s = kirimasim::sir;
    namespace k = kirimasim::kernel;
    const std::array<std::string, 1> sources{"module.sv"};
    const auto input = kirimasim::front::compile(sources);
    if (!input.verified) return false;
    const auto& design = input.verified->design();
    if (design.processes.size() != 1 || design.slots.size() != 1) return false;
    const auto canonical = s::print1(*input.verified);
    if (!canonical) return false;
    kirima_model_identity identity{s::kTextV1Version, KIRIMA_MODEL_LAYOUT_64_V1, {}};
    const auto digest = kirimasim::analysis::contentDigest(*canonical);
    std::copy(digest.begin(), digest.end(), identity.design_sha256);
    std::vector<kirima_model_field> fields;
    const auto size = s::framePayloadLayout(design.processes[0].frame, field, &fields);
    if (!size || fields.size() != 1) return false;
    const kirima_model_layout layout{KIRIMA_MODEL_PROCESS,   0,           1,
                                     s::kFrameCellAlignment, size->bytes, size->logicalBytes,
                                     fields.data()};
    // Context destruction and all pending work precede library unload.
    std::unique_ptr<void, int (*)(void*)> library(dlopen(file, RTLD_NOW | RTLD_LOCAL), dlclose);
    if (!library) return false;
    const auto get =
        reinterpret_cast<kirima_module_query_fn>(dlsym(library.get(), "kirima_get_module_table"));
    if (!get || get(api->abi_major + 1, api->abi_minor) || get(api->abi_major, 4)) return false;
    const auto* table = get(api->abi_major, api->abi_minor);
    if (kirima_validate_module(api, table, &identity, &layout, 1) != KIRIMA_MODULE_OK) return false;
    auto wrong = identity;
    wrong.design_sha256[31] ^= 1;
    if (kirima_validate_module(api, table, &wrong, &layout, 1) != KIRIMA_MODULE_DESIGN)
        return false;
    fields[0].offset += 8;
    if (kirima_validate_module(api, table, &identity, &layout, 1) != KIRIMA_MODULE_LAYOUT)
        return false;
    fields[0].offset -= 8;
    auto incompatible = *table;
    kirima_service_requirement unknown{UINT32_MAX, 1};
    incompatible.services = &unknown;
    if (kirima_validate_module(api, &incompatible, &identity, &layout, 1) != KIRIMA_MODULE_SERVICE)
        return false;
    std::vector<kirima_w4> payload(layout.bytes / s::kFrameCellBytes);
    kirima_model_activation frame{api, 0, 0, payload.data(), nullptr};
    k::Context context;
    const auto slot =
        context.store().add(k::Domain::Four, design.slots[0].type.width, 1, k::Init::Zero);
    auto* c = k::toC(context);
    frame.process = api->add_process(c, table->bodies[0].resume, &frame, KIRIMA_GROUP_ACTIVE);
    api->start(c, frame.process);
    if (context.scheduler().run() != k::EndReason::Quiescent || context.scheduler().time() != 3)
        return false;
    const std::array<std::uint64_t, 4> expected{39, 0, 1, 0};
    const auto* words = context.store().element(slot, 0);
    k::Context interpreted;
    const auto vm = kirimasim::vm::run(*input.verified, interpreted);
    if (vm.reason != k::EndReason::Quiescent || interpreted.scheduler().time() != 3 ||
        !std::equal(expected.begin(), expected.end(), words) ||
        !std::equal(words, words + expected.size(), interpreted.store().element(slot, 0)) ||
        frame.private_state != payload.data() || !table->bodies[0].dispose)
        return false;
    const k::ProcessId process{frame.process};
    if (context.scheduler().hasCallbackOrigins(process)) return false;
    context.scheduler().releaseProcess(process);
    table->bodies[0].dispose(c, &frame);
    if (frame.private_state) return false;

    // A second host kills the suspended body before its delayed write.
    // Draining the old wakeup must leave the slot and its payload untouched.
    kirima_model_activation cancelled{api, 0, 0, payload.data(), nullptr};
    k::Context stopped;
    auto* target = k::toC(stopped);
    auto stoppedSlot = stopped.store().add(k::Domain::Four, 65, 1, k::Init::Zero);
    cancelled.process =
        api->add_process(target, table->bodies[0].resume, &cancelled, KIRIMA_GROUP_ACTIVE);
    const auto cancel = [](kirima_context* ctx, void* raw) -> kirima_status {
        const auto& active = *static_cast<kirima_model_activation*>(raw);
        k::fromC(ctx).scheduler().kill(k::ProcessId{active.process});
        return KIRIMA_DONE;
    };
    api->start(target, cancelled.process);
    api->start(target, api->add_process(target, cancel, &cancelled, KIRIMA_GROUP_ACTIVE));
    if (stopped.scheduler().run() != k::EndReason::Quiescent || cancelled.continuation != 1 ||
        cancelled.private_state != payload.data())
        return false;
    const auto* stoppedWords = stopped.store().element(stoppedSlot, 0);
    if (!std::all_of(stoppedWords, stoppedWords + expected.size(),
                     [](auto word) { return word == 0; }))
        return false;
    stopped.scheduler().releaseProcess(k::ProcessId{cancelled.process});
    table->bodies[0].dispose(target, &cancelled);
    return !cancelled.private_state;
}
