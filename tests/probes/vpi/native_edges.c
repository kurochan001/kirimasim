// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
/* Original host acceptance cases. No simulator implementation is included. */
#include <stdlib.h>
#include <string.h>
#include <vpi_user.h>

static vpiHandle argument(void) {
    vpiHandle it = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, NULL));
    vpiHandle result = it ? vpi_scan(it) : NULL;
    if (it) vpi_free_object(it);
    return result;
}
static int integer(vpiHandle handle) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(handle, &value);
    return value.value.integer;
}
static void put(vpiHandle handle, int number) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = number;
    vpi_put_value(handle, &value, NULL, vpiNoDelay);
}
static PLI_INT32 compile_call(PLI_BYTE8* unused) {
    (void)unused;
    vpi_printf("compile\n");
    return 0;
}
static PLI_INT32 echo(PLI_BYTE8* unused) {
    (void)unused;
    vpi_printf("echo %d\n", integer(argument()));
    return 0;
}
static PLI_INT32 real_function(PLI_BYTE8* unused) {
    s_vpi_value value = {0};
    (void)unused;
    value.format = vpiRealVal;
    value.value.real = 2.25;
    vpi_put_value(vpi_handle(vpiSysTfCall, NULL), &value, NULL, vpiNoDelay);
    return 0;
}
static PLI_INT32 scope_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle call = vpi_handle(vpiSysTfCall, NULL);
    vpiHandle scope = vpi_handle(vpiScope, call);
    vpi_printf("scope %s\n",
               scope && vpi_get(vpiLineNo, call) > 0 && vpi_get_str(vpiFile, call) ? "ok" : "BAD");
    return 0;
}
static PLI_INT32 errors(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle arg = argument();
    put(arg, 9);
    int readonly_error = vpi_chk_error(NULL) == vpiError;
    s_vpi_value value = {0};
    value.format = 999;
    vpi_get_value(arg, &value);
    int format_error = value.format == vpiSuppressVal && vpi_chk_error(NULL) == vpiError;
    vpi_printf("errors %s\n", readonly_error && format_error && integer(arg) == 3 ? "ok" : "BAD");
    return 0;
}
static PLI_INT32 after(p_cb_data cb) {
    vpi_printf("later %d\n", integer(cb->obj));
    return 0;
}
static PLI_INT32 expired(p_cb_data cb) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(cb->obj, &value);
    vpi_printf("expired %s\n",
               value.format == vpiSuppressVal && vpi_chk_error(NULL) == vpiError ? "ok" : "BAD");
    return 0;
}
static PLI_INT32 timer(PLI_BYTE8* mode) {
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    time.low = 1;
    s_cb_data cb = {0};
    cb.reason = cbAfterDelay;
    cb.obj = argument();
    cb.time = &time;
    cb.cb_rtn = *mode == 'e' ? expired : after;
    vpi_register_cb(&cb);
    cb.cb_rtn = NULL;
    time.low = 999;
    return 0;
}
static PLI_INT32 end(p_cb_data cb) {
    (void)cb;
    vpi_printf("end\n");
    return 0;
}
static PLI_INT32 lifecycle(PLI_BYTE8* unused) {
    (void)unused;
    s_cb_data cb = {0};
    cb.reason = cbEndOfSimulation;
    cb.cb_rtn = end;
    vpi_register_cb(&cb);
    return 0;
}
static PLI_INT32 finish_call(PLI_BYTE8* unused) {
    (void)unused;
    vpi_control(vpiFinish, 0);
    return 0;
}
static PLI_INT32 sync_callback(p_cb_data cb) {
    if (cb->reason == cbReadWriteSynch) {
        vpi_printf("rw %d\n", integer(cb->obj));
        put(cb->obj, 7);
    } else
        vpi_printf("ro %d\n", integer(cb->obj));
    return 0;
}
static PLI_INT32 sync_call(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    s_cb_data cb = {0};
    cb.obj = argument();
    cb.time = &time;
    cb.cb_rtn = sync_callback;
    cb.reason = cbReadWriteSynch;
    vpi_register_cb(&cb);
    cb.reason = cbReadOnlySynch;
    vpi_register_cb(&cb);
    return 0;
}
static void startup(void) {
    struct Entry {
        const char* name;
        PLI_INT32 (*call)(PLI_BYTE8*);
        PLI_BYTE8* data;
    };
    const struct Entry entries[] = {
        {"$edge_echo", echo, NULL},          {"$edge_compile", echo, NULL},
        {"$edge_real", real_function, NULL}, {"$edge_scope", scope_call, NULL},
        {"$edge_errors", errors, NULL},      {"$edge_later", timer, "later"},
        {"$edge_expire", timer, "expire"},   {"$edge_lifecycle", lifecycle, NULL},
        {"$edge_finish", finish_call, NULL}, {"$edge_sync", sync_call, NULL}};
    for (unsigned i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        s_vpi_systf_data data = {0};
        data.type = i == 2 ? vpiSysFunc : vpiSysTask;
        data.sysfunctype = vpiRealFunc;
        data.tfname = (PLI_BYTE8*)entries[i].name;
        data.calltf = entries[i].call;
        data.user_data = entries[i].data;
        if (i == 1) data.compiletf = compile_call;
        vpi_register_systf(&data);
    }
}
void (*vlog_startup_routines[])(void) = {startup, NULL};
