// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/event_abi.h"
#include "kirimasim/kernel/object_abi.h"
extern "C" int check_c_objects(const kirima_kernel_api*, kirima_context*, kirima_context*);
extern "C" int check_c_events(const kirima_kernel_api*, kirima_context*, kirima_context*,
                              void (*)(kirima_context*));
bool checkNativeObjectsAndEvents(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    const auto* events =
        static_cast<const kirima_event_api*>(api.query_service(KIRIMA_SERVICE_EVENT, 1));
    const auto* objects =
        static_cast<const kirima_object_api*>(api.query_service(KIRIMA_SERVICE_OBJECT, 1));
    if (!events || !objects) return false;
    k::Context empty(k::OutputDestinations{}, nullptr, std::pmr::null_memory_resource());
    auto* c = k::toC(empty);
    kirima_pinned_event e{};
    kirima_pinned_object o{};
    std::uint64_t rng = 3;
    if (events->create(c, &e) != KIRIMA_EVENT_RESOURCE || e.pin.serial ||
        objects->create(c, 0, &rng, &o) != KIRIMA_OBJ_RESOURCE || o.pin.serial || rng != 3 ||
        empty.managedEvents().allocatedObjects() || empty.heap().objectStats().live)
        return false;
    k::Context ctx, other;
    const auto run = [](kirima_context* c) { (void)k::fromC(c).scheduler().run(); };
    return check_c_objects(&api, k::toC(ctx), k::toC(other)) &&
           check_c_events(&api, k::toC(ctx), k::toC(other), run) && ctx.scheduler().time() == 4;
}
