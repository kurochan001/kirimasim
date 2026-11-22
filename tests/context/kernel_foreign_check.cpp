// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/vpi.h"
extern "C" int check_c_foreign(const kirima_kernel_api*, kirima_context*, kirima_context*);
namespace {
PLI_INT32 invoke(PLI_BYTE8*) {
    const auto self = vpi_handle(vpiSysTfCall, nullptr);
    const auto args = vpi_iterate(vpiArgument, self);
    const auto a = vpi_scan(args);
    s_vpi_value value{};
    value.format = vpiIntVal;
    vpi_get_value(a, &value);
    const auto original = value.value.integer;
    value.value.integer = original + 1;
    vpi_put_value(a, &value, nullptr, vpiNoDelay);
    value.value.integer = original * 2;
    vpi_put_value(self, &value, nullptr, vpiNoDelay);
    vpi_free_object(args);
    return 0;
}
}  // namespace
bool checkNativeForeign(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    k::Context ctx, other;
    {
        k::Vpi::Entry entry(ctx);
        char name[] = "$model_vpi";
        s_vpi_systf_data d{};
        d.type = vpiSysFunc;
        d.sysfunctype = vpiIntFunc;
        d.tfname = name;
        d.calltf = invoke;
        if (!vpi_register_systf(&d)) return false;
    }
    return check_c_foreign(&api, k::toC(ctx), k::toC(other));
}
