// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vpi_user.h>

static void args(vpiHandle* out, int count) {
    vpiHandle it = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    for (int i = 0; i < count; ++i)
        out[i] = vpi_scan(it);
    while (vpi_scan(it)) {
    }
}
static int integer(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    return value.value.integer;
}
static PLI_INT32 named(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[3];
    args(a, 3);
    s_vpi_value path = {0};
    path.format = vpiStringVal;
    vpi_get_value(a[0], &path);
    if (!path.value.str) {
        vpi_printf("named missing path\n");
        return 0;
    }
    vpiHandle target = vpi_handle_by_name(path.value.str, 0);
    if (!target) {
        vpi_printf("named missing target\n");
        return 0;
    }
    int flag = integer(a[2]);
    s_vpi_value value = {0};
    value.format = vpiBinStrVal;
    vpi_get_value(a[1], &value);
    char* copy = malloc(strlen(value.value.str) + 1);
    if (!copy) {
        vpi_printf("probe allocation failure\n");
        return 0;
    }
    strcpy(copy, value.value.str);
    value.value.str = copy;
    vpi_put_value(target, &value, 0, flag);
    int error = vpi_chk_error(0);
    if (flag == vpiReleaseFlag) vpi_printf("named returned=%s\n", value.value.str);
    value.format = vpiBinStrVal;
    vpi_get_value(target, &value);
    vpi_printf("named error=%d now=%s\n", error, value.value.str);
    free(copy);
    return 0;
}
static PLI_INT32 indexed(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[3];
    args(a, 3);
    vpiHandle bit = vpi_handle_by_index(a[0], integer(a[1]));
    if (!bit) {
        vpi_printf("indexed missing target\n");
        return 0;
    }
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = 1;
    vpi_put_value(bit, &value, 0, integer(a[2]));
    int error = vpi_chk_error(0);
    vpi_printf("indexed error=%d now=%d\n", error, integer(a[0]));
    return 0;
}
static PLI_INT32 bad_release(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[1];
    args(a, 1);
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = 1;
    vpi_put_value(a[0], &value, 0, vpiForceFlag);
    value.format = 999;
    value.value.integer = 123;
    vpi_put_value(a[0], &value, 0, vpiReleaseFlag);
    int error = vpi_chk_error(0);
    vpi_printf("bad-release error=%d format=%d payload=%d now=%d\n", error, value.format,
               value.value.integer, integer(a[0]));
    return 0;
}
static PLI_INT32 ignored_time(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[1];
    args(a, 1);
    s_vpi_time time = {0};
    time.type = 999;
    time.high = UINT32_MAX;
    time.low = UINT32_MAX;
    const int flags[3] = {vpiNoDelay, vpiForceFlag, vpiReleaseFlag};
    for (int i = 0; i < 3; ++i) {
        s_vpi_value value = {0};
        value.format = vpiIntVal;
        value.value.integer = i == 0;
        vpi_put_value(a[0], &value, &time, flags[i]);
        int error = vpi_chk_error(0);
        vpi_printf("time mode=%d error=%d now=%d\n", flags[i], error, integer(a[0]));
    }
    return 0;
}
static PLI_INT32 readonly_call(p_cb_data cb) {
    if (!cb->obj || vpi_get(vpiType, cb->obj) != vpiNet) {
        vpi_printf("readonly invalid target\n");
        return 0;
    }
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = 1;
    vpi_put_value(cb->obj, &value, 0, vpiForceFlag);
    int error = vpi_chk_error(0);
    vpi_printf("readonly error=%d now=%d\n", error, integer(cb->obj));
    return 0;
}
static PLI_INT32 readonly_start(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[1];
    args(a, 1);
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    s_cb_data cb = {0};
    cb.reason = cbReadOnlySynch;
    cb.cb_rtn = readonly_call;
    cb.obj = a[0];
    cb.time = &time;
    vpi_register_cb(&cb);
    return 0;
}
struct reentry {
    unsigned calls, errors;
    int nested;
    int width;
    vpiHandle target;
};
static PLI_INT32 nested_call(p_cb_data cb) {
    struct reentry* state = (struct reentry*)cb->user_data;
    ++state->calls;
    if (!cb->value || cb->value->format != vpiVectorVal || !cb->value->value.vector) {
        ++state->errors;
        return 0;
    }
    const int one = cb->value->value.vector[0].aval & 1;
    for (int word = 0; word < (state->width + 31) / 32; ++word) {
        uint32_t mask = word == (state->width - 1) / 32 && state->width % 32
                            ? (UINT32_C(1) << (state->width % 32)) - 1
                            : UINT32_MAX;
        if ((uint32_t)cb->value->value.vector[word].aval != (one ? mask : 0) ||
            cb->value->value.vector[word].bval != 0)
            ++state->errors;
    }
    if (one && !state->nested) {
        state->nested = 1;
        s_vpi_value value = {0};
        value.format = vpiIntVal;
        vpi_put_value(state->target, &value, 0, vpiForceFlag);
        if (vpi_chk_error(0) || integer(state->target) != 0) ++state->errors;
        if (!(cb->value->value.vector[0].aval & 1)) ++state->errors;
    }
    return 0;
}
static PLI_INT32 reenter(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[1];
    args(a, 1);
    struct reentry state = {0};
    state.target = a[0];
    state.width = vpi_get(vpiSize, a[0]);
    if (state.width <= 0) {
        vpi_printf("reentry invalid width\n");
        return 0;
    }
    s_vpi_time time = {0};
    time.type = vpiSuppressTime;
    s_vpi_value observed = {0};
    observed.format = vpiVectorVal;
    s_cb_data cb = {0};
    cb.reason = cbValueChange;
    cb.cb_rtn = nested_call;
    cb.obj = a[0];
    cb.time = &time;
    cb.value = &observed;
    cb.user_data = (PLI_BYTE8*)&state;
    vpiHandle subscription = vpi_register_cb(&cb);
    char* ones = malloc((size_t)state.width + 1);
    if (!ones) {
        vpi_remove_cb(subscription);
        return 0;
    }
    memset(ones, '1', (size_t)state.width);
    ones[state.width] = 0;
    s_vpi_value value = {0};
    value.format = vpiBinStrVal;
    value.value.str = ones;
    vpi_put_value(a[0], &value, 0, vpiForceFlag);
    if (vpi_chk_error(0)) ++state.errors;
    vpi_remove_cb(subscription);
    vpi_printf("reentry calls=%u errors=%u now=%d\n", state.calls, state.errors, integer(a[0]));
    free(ones);
    return 0;
}
static PLI_INT32 compile_probe(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle a[1];
    args(a, 1);
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = 1;
    vpi_put_value(a[0], &value, 0, vpiForceFlag);
    vpi_printf("compile error=%d\n", vpi_chk_error(0));
    (void)vpi_get(vpiSize, a[0]);
    return 0;
}
static PLI_INT32 runtime_probe(PLI_BYTE8* unused) {
    (void)unused;
    vpi_printf("runtime\n");
    return 0;
}
static void startup(void) {
    const char* names[] = {"$net_named",    "$net_index",   "$net_bad_release", "$net_ignored_time",
                           "$net_readonly", "$net_reenter", "$net_compile"};
    PLI_INT32 (*calls[])(PLI_BYTE8*) = {named,          indexed, bad_release,  ignored_time,
                                        readonly_start, reenter, runtime_probe};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        s_vpi_systf_data data = {0};
        data.type = vpiSysTask;
        data.tfname = (PLI_BYTE8*)names[i];
        data.calltf = calls[i];
        if (i == 6) data.compiletf = compile_probe;
        vpi_register_systf(&data);
    }
}
void (*vlog_startup_routines[])(void) = {startup, 0};
