// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original external ABI probes. No simulator implementation or test is copied.
#include <vpi_user.h>

#include <stdlib.h>
#include <string.h>

static PLI_INT32 net_op(PLI_BYTE8 *unused) {
    (void)unused;
    vpiHandle call = vpi_handle(vpiSysTfCall, 0);
    vpiHandle args = vpi_iterate(vpiArgument, call);
    vpiHandle target = vpi_scan(args);
    vpiHandle input = vpi_scan(args);
    vpiHandle mode = vpi_scan(args);
    while (vpi_scan(args)) {}
    s_vpi_value flag = {0};
    flag.format = vpiIntVal;
    vpi_get_value(mode, &flag);
    s_vpi_value value = {0};
    value.format = vpiBinStrVal;
    char *owned = 0;
    if (flag.value.integer != vpiReleaseFlag) {
        vpi_get_value(input, &value);
        size_t size = strlen(value.value.str) + 1;
        owned = malloc(size);
        if (!owned) {
            vpi_printf("probe allocation failure\n");
            vpi_control(vpiFinish, 1);
            return 0;
        }
        memcpy(owned, value.value.str, size);
        value.value.str = owned;
    }
    vpiHandle event = vpi_put_value(target, &value, 0, flag.value.integer);
    PLI_INT32 error = vpi_chk_error(0);
    vpi_printf("op %d error=%d event=%d", flag.value.integer, error, event != 0);
    if (flag.value.integer == vpiReleaseFlag)
        vpi_printf(" returned=%s", value.value.str ? value.value.str : "<null>");
    s_vpi_value now = {0};
    now.format = vpiBinStrVal;
    vpi_get_value(target, &now);
    vpi_printf(" now=%s\n", now.value.str ? now.value.str : "<null>");
    free(owned);
    return 0;
}

static PLI_INT32 changed(p_cb_data callback) {
    s_vpi_value value = {0};
    value.format = vpiBinStrVal;
    vpi_get_value(callback->obj, &value);
    vpi_printf("change %s\n", value.value.str);
    return 0;
}

static PLI_INT32 watch(PLI_BYTE8 *unused) {
    (void)unused;
    vpiHandle args = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle target = vpi_scan(args);
    while (vpi_scan(args)) {}
    s_vpi_value value = {0};
    value.format = vpiSuppressVal;
    s_vpi_time time = {0};
    time.type = vpiSuppressTime;
    s_cb_data callback = {0};
    callback.reason = cbValueChange;
    callback.cb_rtn = changed;
    callback.obj = target;
    callback.value = &value;
    callback.time = &time;
    vpi_register_cb(&callback);
    return 0;
}

static void startup(void) {
    s_vpi_systf_data operation = {0};
    operation.type = vpiSysTask;
    operation.tfname = "$net_op";
    operation.calltf = net_op;
    vpi_register_systf(&operation);
    s_vpi_systf_data observation = {0};
    observation.type = vpiSysTask;
    observation.tfname = "$net_watch";
    observation.calltf = watch;
    vpi_register_systf(&observation);
}

void (*vlog_startup_routines[])(void) = {startup, 0};
