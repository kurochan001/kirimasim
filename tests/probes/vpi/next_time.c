/* Copyright (c) 2026 The kirimasim Authors */
/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include <math.h>
#include <stdint.h>
#include <vpi_user.h>

/* State belongs to this original single-design plugin fixture. */
static int errors, callbacks, mode;
static unsigned last_tick;

static PLI_INT32 next_time(p_cb_data data) {
    s_vpi_time now = {0};
    now.type = vpiSimTime;
    vpi_get_time(0, &now);
    ++callbacks;
    last_tick = now.low;
    errors += now.high != 0 || now.low != 3;
    errors += !data->time;
    if (data->time) {
        if (data->time->type == vpiScaledRealTime)
            errors += data->time->real != 3.0;
        else
            errors += data->time->type != vpiSimTime || data->time->high != 0 ||
                      data->time->low != 3;
    }
    return 0;
}

static PLI_INT32 register_next(p_cb_data unused) {
    s_vpi_time time = {0};
    time.type = mode < 2 ? vpiSimTime : vpiScaledRealTime;
    if (mode == 1) time.high = time.low = UINT32_MAX;
    time.real = mode == 2   ? -1.0
                : mode == 3 ? INFINITY
                : mode == 4 ? NAN
                : mode == 5 ? 1.0e300
                : mode == 6 ? 0.0
                            : 1.25;
    s_cb_data cb = {0};
    cb.reason = cbNextSimTime;
    cb.cb_rtn = next_time;
    cb.time = &time;
    errors += !vpi_register_cb(&cb);
    errors += vpi_chk_error(0) != 0;
    /* Descriptors are borrowed only during registration. */
    time.type = vpiSuppressTime;
    time.high = time.low = 123;
    time.real = -999.0;
    cb.cb_rtn = 0;
    (void)unused;
    return 0;
}

static int integer(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    errors += vpi_chk_error(0) != 0;
    return value.value.integer;
}

static PLI_INT32 start(PLI_BYTE8* unused) {
    errors = callbacks = 0;
    last_tick = 0;
    vpiHandle call = vpi_handle(vpiSysTfCall, 0);
    vpiHandle args = vpi_iterate(vpiArgument, call);
    mode = integer(vpi_scan(args));
    const int phase = integer(vpi_scan(args));
    while (vpi_scan(args)) {
    }
    if (phase == 0) {
        register_next(0);
    } else {
        s_vpi_time time = {0};
        time.type = vpiSimTime;
        s_cb_data cb = {0};
        cb.reason = phase == 1 ? cbReadOnlySynch : cbAfterDelay;
        cb.cb_rtn = register_next;
        cb.time = &time;
        errors += !vpi_register_cb(&cb);
        errors += vpi_chk_error(0) != 0;
    }
    (void)unused;
    return 0;
}

static PLI_INT32 finish(PLI_BYTE8* unused) {
    errors += callbacks != 1;
    vpi_printf("next-time errors=%d callbacks=%d tick=%u\n", errors, callbacks, last_tick);
    (void)unused;
    return 0;
}

static void startup(void) {
    s_vpi_systf_data routine = {0};
    routine.type = vpiSysTask;
    routine.tfname = "$next_time_start";
    routine.calltf = start;
    vpi_register_systf(&routine);
    routine.tfname = "$next_time_finish";
    routine.calltf = finish;
    vpi_register_systf(&routine);
}
void (*vlog_startup_routines[])(void) = {startup, 0};
