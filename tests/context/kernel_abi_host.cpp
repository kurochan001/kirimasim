// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <algorithm>
#include <array>
#include <cstdint>

#include "kernel_abi_model.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/value_abi.h"

extern "C" int check_c_nets(const kirima_kernel_api*, kirima_context*, kirima_context*,
                            void (*)(kirima_context*));
extern "C" int check_c_observation(const kirima_kernel_api*, kirima_context*, kirima_context*,
                                   void (*)(kirima_context*));
bool checkNativeSignalsAndTasks(const kirima_kernel_api& api);
bool checkNativeObjectsAndEvents(const kirima_kernel_api& api);
bool checkNativeContainers(const kirima_kernel_api& api);
bool checkNativeScalarTasks(const kirima_kernel_api& api);
bool checkNativeProcesses(const kirima_kernel_api& api);
bool checkNativeReferences(const kirima_kernel_api& api);
bool checkNativeIo(const kirima_kernel_api& api);
bool checkNativeValues(const kirima_value_api& api);

int main() {
    namespace k = kirimasim::kernel;
    const auto* api = kirima_query_kernel_api(KIRIMA_ABI_MAJOR, 4);
    if (!api || api != kirima_get_kernel_api() ||
        kirima_query_kernel_api(KIRIMA_ABI_MAJOR + 1, 0) ||
        kirima_query_kernel_api(KIRIMA_ABI_MAJOR, KIRIMA_ABI_MINOR + 1) ||
        kirima_query_kernel_api(KIRIMA_ABI_MAJOR, 3) != api)
        return 1;
    if (!check_c_value_model(api)) return 7;
    const auto* values = static_cast<const kirima_value_api*>(
        api->query_service(KIRIMA_SERVICE_VALUE, KIRIMA_VALUE_VERSION));
    if (!values || !checkNativeValues(*values)) return 8;
    if (!checkNativeIo(*api)) return 9;
    if (!checkNativeReferences(*api)) return 10;
    if (!checkNativeProcesses(*api)) return 11;
    if (!checkNativeScalarTasks(*api)) return 12;
    if (!check_c_presentation(api)) return 13;
    if (!checkNativeContainers(*api)) return 14;
    if (!checkNativeObjectsAndEvents(*api)) return 15;
    if (!checkNativeSignalsAndTasks(*api)) return 16;
    {
        k::Context obs, other;
        obs.store().add(k::Domain::Four, 8, 1, k::Init::Zero);
        obs.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        if (!check_c_observation(api, k::toC(obs), k::toC(other),
                                 [](kirima_context* c) { (void)k::fromC(c).scheduler().run(); }))
            return 17;
    }
    {
        k::Context nets, other;
        nets.store().add(k::Domain::Four, 1, 1, k::Init::Z);
        if (!check_c_nets(api, k::toC(nets), k::toC(other),
                          [](kirima_context* c) { (void)k::fromC(c).scheduler().run(); }))
            return 18;
    }
    k::Context ctx;
    auto* c = k::toC(ctx);
    ctx.store().add(k::Domain::Four, 4, 1, k::Init::Zero);
    auto wide = ctx.store().add(k::Domain::Four, 65, 2, k::Init::Zero);
    std::uint64_t count = 1;
    kirima_slot_layout row{};
    if (api->slot_layout(c, 2, &row) || api->slot_layout(c, 0, nullptr) ||
        api->domain_words(c, KIRIMA_DOMAIN_REAL, &count) != nullptr || count != 0)
        return 2;
    count = 1;
    if (api->domain_words(c, KIRIMA_DOMAIN_HANDLE + 1, &count) != nullptr || count != 0) return 3;
    model_frame frame{};
    auto old = *api;
    old.abi_minor = 3;
    if (start_abi_model(&old, c, &frame) || !start_abi_model(api, c, &frame)) return 4;
    if (ctx.scheduler().run() != k::EndReason::Quiescent || frame.resumes != 2 ||
        frame.times[0] != 0 || frame.times[1] != 3)
        return 5;
    constexpr std::array<std::uint64_t, 4> expected{5, 12, 1, 1}, zeros{};
    return std::equal(expected.begin(), expected.end(), frame.sample) &&
                   std::equal(expected.begin(), expected.end(), ctx.store().element(wide, 1)) &&
                   std::equal(zeros.begin(), zeros.end(), ctx.store().element(wide, 0))
               ? 0
               : 6;
}
