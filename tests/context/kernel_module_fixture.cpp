// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/value_abi.h"
#include <cstddef>
#include <cstdint>

namespace {
// Hand-written implementation of model/module.sv. Each frame belongs to
// its host. A suspended model never borrows a pointer to VM scratch.
kirima_status resume(kirima_context* context, void* raw) {
    auto& frame = *static_cast<kirima_model_activation*>(raw);
    auto* pending = static_cast<kirima_w4*>(frame.payload);
    if (!frame.continuation) {
        frame.private_state = pending;
        pending[0] = {39, 0};
        pending[1] = {1, 0};
        ++frame.continuation;
        frame.kernel->wait_delay(context, frame.process, 3);
        return KIRIMA_WAITING;
    }
    const std::uint64_t value[]{pending[0].a, pending[0].b, pending[1].a, pending[1].b};
    frame.kernel->schedule_nba(context, frame.process, 0, 0, 0, 0, value, 0);
    return KIRIMA_DONE;
}
void dispose(kirima_context*, kirima_model_activation* frame) { frame->private_state = nullptr; }
static_assert(sizeof(kirima_w4) == 16 && alignof(kirima_w4) == 8);
constexpr kirima_model_field fields[]{{0, 0, sizeof(kirima_w4) * 2, 1, 0, 2}};
constexpr kirima_model_body bodies[]{
    {{KIRIMA_MODEL_PROCESS, 0, 1, 8, 32, 32, fields}, resume, dispose}};
constexpr kirima_service_requirement services[]{{KIRIMA_SERVICE_VALUE, KIRIMA_VALUE_VERSION}};
constexpr kirima_module_table table{
    KIRIMA_MODULE_DESCRIPTOR,
    sizeof(kirima_module_table),
    KIRIMA_ABI_MAJOR,
    5,
    {1, KIRIMA_MODEL_LAYOUT_64_V1, {0x58, 0xd2, 0x58, 0x1e, 0x65, 0xe6, 0x36, 0xe7,
                                    0x3d, 0xc7, 0x37, 0xeb, 0x80, 0x47, 0xe,  0x1e,
                                    0xd1, 0xa4, 0x3,  0x44, 0x40, 0xbc, 0x6e, 0x80,
                                    0x3,  0xf3, 0x44, 0x26, 0x40, 0x33, 0x6b, 0xf1}},
    1,
    1,
    bodies,
    services};
}  // namespace
extern "C" KIRIMA_MODULE_EXPORT const kirima_module_table* kirima_get_module_table(uint32_t major,
                                                                                   uint32_t minor) {
    return major == table.abi_major && minor >= table.minimum_minor ? &table : nullptr;
}
