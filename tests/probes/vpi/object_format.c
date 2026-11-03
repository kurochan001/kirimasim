/* Copyright (c) 2026 The kirimasim Authors */
/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include <vpi_user.h>

/* Independent process-local fixture state; callbacks verify their actual payload. */
static int errors, callbacks, expected_format, width;
static vpiHandle callback_token;

static void inspect(p_vpi_value value, int expected) {
    if (value->format != expected_format) {
        ++errors;
        return;
    }
    switch (value->format) {
        case vpiScalarVal: errors += value->value.scalar != (expected ? vpi1 : vpi0); break;
        case vpiIntVal: errors += value->value.integer != expected; break;
        case vpiRealVal: errors += value->value.real != expected; break;
        case vpiVectorVal:
            if (!value->value.vector) {
                ++errors;
                break;
            }
            for (int i = 0; i < (width + 31) / 32; ++i) {
                errors += value->value.vector[i].aval != (i == 0 ? expected : 0);
                errors += value->value.vector[i].bval != 0;
            }
            break;
        default: ++errors; break;
    }
}

static void read_value(vpiHandle object, int expected) {
    s_vpi_value value = {0};
    value.format = vpiObjTypeVal;
    vpi_get_value(object, &value);
    if (vpi_chk_error(0)) ++errors;
    inspect(&value, expected);
}

static PLI_INT32 changed(p_cb_data data) {
    ++callbacks;
    if (!data->value)
        ++errors;
    else
        inspect(data->value, 0);
    return 0;
}

static PLI_INT32 start(PLI_BYTE8* unused) {
    (void)unused;
    errors = callbacks = 0;
    vpiHandle args = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle object = vpi_scan(args), format = vpi_scan(args);
    s_vpi_value requested = {0};
    requested.format = vpiIntVal;
    vpi_get_value(format, &requested);
    expected_format = requested.value.integer;
    width = vpi_get(vpiSize, object);
    while (vpi_scan(args)) {
    }
    read_value(object, 0);
    vpiHandle named = vpi_handle_by_name(vpi_get_str(vpiFullName, object), 0);
    if (!named)
        ++errors;
    else
        read_value(named, 0);

    s_vpi_value force = {0};
    force.format = vpiIntVal;
    force.value.integer = 1;
    vpi_put_value(object, &force, 0, vpiForceFlag);
    if (vpi_chk_error(0)) ++errors;
    read_value(object, 1);
    s_vpi_value release = {0};
    release.format = vpiObjTypeVal;
    vpi_put_value(object, &release, 0, vpiReleaseFlag);
    if (vpi_chk_error(0)) ++errors;
    inspect(&release, 1);
    read_value(object, 1);

    s_vpi_value callback_value = {0};
    callback_value.format = vpiObjTypeVal;
    s_vpi_time time = {0};
    time.type = vpiSuppressTime;
    s_cb_data cb = {0};
    cb.reason = cbValueChange;
    cb.cb_rtn = changed;
    cb.obj = object;
    cb.time = &time;
    cb.value = &callback_value;
    callback_token = vpi_register_cb(&cb);
    if (!callback_token || vpi_chk_error(0)) ++errors;

    s_vpi_value next = {0};
    next.format = vpiIntVal;
    next.value.integer = 0;
    vpi_put_value(object, &next, 0, vpiNoDelay);
    if (vpi_chk_error(0)) ++errors;
    return 0;
}

static PLI_INT32 finish(PLI_BYTE8* unused) {
    (void)unused;
    if (callback_token) vpi_remove_cb(callback_token);
    vpi_printf("object-format errors=%d callbacks=%d\n", errors, callbacks);
    return 0;
}

static PLI_INT32 read_net(PLI_BYTE8* unused) {
    (void)unused;
    errors = 0;
    vpiHandle args = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle object = vpi_scan(args), format = vpi_scan(args);
    s_vpi_value requested = {0};
    requested.format = vpiIntVal;
    vpi_get_value(format, &requested);
    expected_format = requested.value.integer;
    width = vpi_get(vpiSize, object);
    while (vpi_scan(args)) {
    }
    read_value(object, 1);
    vpiHandle named = vpi_handle_by_name(vpi_get_str(vpiFullName, object), 0);
    if (!named)
        ++errors;
    else
        read_value(named, 1);
    vpi_printf("object-format read-errors=%d\n", errors);
    return 0;
}

static void startup(void) {
    s_vpi_systf_data routine = {0};
    routine.type = vpiSysTask;
    routine.tfname = "$object_format_start";
    routine.calltf = start;
    vpi_register_systf(&routine);
    routine.tfname = "$object_format_finish";
    routine.calltf = finish;
    vpi_register_systf(&routine);
    routine.tfname = "$object_format_read";
    routine.calltf = read_net;
    vpi_register_systf(&routine);
}
void (*vlog_startup_routines[])(void) = {startup, 0};
