// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <cstdint>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/reference_abi.h"

extern "C" int check_c_reference_model(const kirima_kernel_api*, kirima_context*, kirima_context*,
                                       std::uint64_t, kirima_reference*);

bool checkNativeReferences(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    k::Context ctx, other;
    const auto text = ctx.strings().intern("captured");
    kirima_reference keep{};
    std::size_t writes = 0;
    ctx.activations().setChangeHook(
        {[](void* p, const k::ActivationAccess&) noexcept { ++*static_cast<std::size_t*>(p); },
         &writes});
    if (!check_c_reference_model(&api, k::toC(ctx), k::toC(other), text, &keep) || writes != 2)
        return false;
    const auto part = ctx.references().part(keep.ticket);
    if (!part || part.value.numElems != 2 || ctx.activations().stats().leases != 1) return false;
    std::array<k::W4, 3> cells{};
    if (ctx.activations().read(part.value.binding, 1, cells) != k::ActivationError::None ||
        cells[2].a != 1 || cells[2].b != 1)
        return false;
    std::uint64_t root = 0;
    if (!ctx.activations().visitStringRoots(
            [](void* out, std::uint64_t h) noexcept {
                *static_cast<std::uint64_t*>(out) = h;
                return true;
            },
            &root) ||
        root != text)
        return false;
    const auto* refs =
        static_cast<const kirima_reference_api*>(api.query_service(KIRIMA_SERVICE_REFERENCE, 1));
    if (!refs || refs->release(k::toC(ctx), keep) || ctx.activations().stats().live ||
        ctx.activations().read(part.value.binding, 1, cells) != k::ActivationError::InvalidBinding)
        return false;
    return ctx.activations().checkCounts().empty();
}
