// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/context.h"
extern "C" int check_c_container(const kirima_kernel_api*, kirima_context*, kirima_context*,
                                 kirima_heap_token*, std::uint32_t*);
bool checkNativeContainers(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    k::Context ctx, other;
    kirima_heap_token token{};
    std::uint32_t type = 0;
    if (!check_c_container(&api, k::toC(ctx), k::toC(other), &token, &type)) return false;
    k::W4 out{};
    const auto read = ctx.heap().read(k::HeapTypeId{type}, {token.bits}, 0, {&out, 1});
    return read && out.a == 248 && !out.b;
}
