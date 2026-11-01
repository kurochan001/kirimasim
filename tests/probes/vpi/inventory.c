// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original black-box VPI inventory plugin. No simulator implementation code.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vpi_user.h>

static vpiHandle argument(void) {
    vpiHandle call = vpi_handle(vpiSysTfCall, 0);
    vpiHandle it = vpi_iterate(vpiArgument, call);
    vpiHandle value = it ? vpi_scan(it) : 0;
    if (it) vpi_free_object(it);
    return value;
}
static PLI_INT32 read_int(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    return value.value.integer;
}
static void put_int(vpiHandle object, PLI_INT32 number, PLI_INT32 flag) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = number;
    vpi_put_value(object, &value, 0, flag);
}
static PLI_INT32 task_call(PLI_BYTE8* unused) {
    (void)unused;
    vpi_printf("task %d\n", read_int(argument()));
    return 0;
}
static PLI_INT32 trace_put_call(PLI_BYTE8* unused) {
    (void)unused;
    vpi_printf("before put\n");
    put_int(argument(), 1, vpiNoDelay);
    vpi_printf("after put\n");
    return 0;
}
static PLI_INT32 check_compile(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle call = vpi_handle(vpiSysTfCall, 0);
    vpiHandle it = vpi_iterate(vpiArgument, call);
    vpiHandle value = it ? vpi_scan(it) : 0;
    vpi_printf("compile %d\n", value ? vpi_get(vpiSize, value) : -1);
    if (it) vpi_free_object(it);
    return 0;
}
static PLI_INT32 modules_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle it = vpi_iterate(vpiModule, 0), item;
    unsigned count = 0;
    while (it && (item = vpi_scan(it))) {
        if (!strcmp(vpi_get_str(vpiFullName, item), "top")) ++count;
    }
    vpi_printf("modules %u\n", count);
    return 0;
}
static PLI_INT32 info_call(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_vlog_info info = {0};
    int found = 0;
    if (vpi_get_vlog_info(&info))
        for (int i = 0; i < info.argc; ++i)
            if (!strcmp(info.argv[i], "+vpi_probe=42")) found = 1;
    vpi_printf("argv %d\n", found);
    return 0;
}
static PLI_INT32 finish_call(PLI_BYTE8* unused) {
    (void)unused;
    vpi_printf("finish\n");
    vpi_control(vpiFinish, 0);
    return 0;
}
static PLI_INT32 error_call(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_error_info info = {0};
    vpi_printf("error %d\n", vpi_chk_error(&info));
    return 0;
}
static PLI_INT32 add_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle call = vpi_handle(vpiSysTfCall, 0);
    vpiHandle it = vpi_iterate(vpiArgument, call);
    vpiHandle left = vpi_scan(it), right = vpi_scan(it);
    put_int(call, read_int(left) + read_int(right), vpiNoDelay);
    vpi_free_object(it);
    return 0;
}
static PLI_INT32 wide_size(PLI_BYTE8* unused) {
    (void)unused;
    return 129;
}
static PLI_INT32 wide_call(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_vecval words[5] = {{0}};
    words[0].aval = (PLI_INT32)UINT32_C(0xa9abcdef);
    words[0].bval = UINT32_C(0x30000000);
    words[4].aval = 1;
    s_vpi_value value = {0};
    value.format = vpiVectorVal;
    value.value.vector = words;
    vpi_put_value(vpi_handle(vpiSysTfCall, 0), &value, 0, vpiNoDelay);
    return 0;
}
static PLI_INT32 vector_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle object = argument();
    s_vpi_vecval words[5] = {{0}};
    words[0].aval = (PLI_INT32)UINT32_C(0xa9abcdef);
    words[0].bval = UINT32_C(0x30000000);
    words[4].aval = 1;
    s_vpi_value value = {0};
    value.format = vpiVectorVal;
    value.value.vector = words;
    vpi_put_value(object, &value, 0, vpiNoDelay);
    vpi_get_value(object, &value);
    int same = vpi_get(vpiSize, object) == 129;
    for (unsigned i = 0; i < 5; ++i)
        same = same && value.value.vector[i].aval == words[i].aval &&
               value.value.vector[i].bval == words[i].bval;
    vpi_printf("vector %s\n", same ? "ok" : "BAD");
    return 0;
}
static PLI_INT32 hierarchy_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle object = vpi_handle_by_name("top.u.value", 0);
    if (!object) {
        vpi_printf("hierarchy BAD\n");
        return 0;
    }
    vpi_printf("hierarchy %s %d\n", vpi_get_str(vpiFullName, object), vpi_get(vpiSize, object));
    vpiHandle scope = vpi_handle(vpiScope, object);
    vpi_printf("scope %s\n", vpi_get_str(vpiFullName, scope));
    vpi_printf("missing %s\n", vpi_handle_by_name("top.no_such_object", 0) ? "BAD" : "null");
    return 0;
}
static PLI_INT32 memory_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle memory = vpi_handle_by_name("top.values", 0);
    vpiHandle word = vpi_handle_by_index(memory, 1);
    if (!word) {
        vpi_printf("memory BAD\n");
        return 0;
    }
    put_int(word, 13, vpiNoDelay);
    vpi_printf("memory %d\n", read_int(word));
    return 0;
}
static PLI_INT32 real_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle object = argument();
    s_vpi_value value = {0};
    value.format = vpiRealVal;
    vpi_get_value(object, &value);
    vpi_printf("real %.2f\n", value.value.real);
    value.value.real = 2.25;
    vpi_put_value(object, &value, 0, vpiNoDelay);
    return 0;
}
static PLI_INT32 string_call(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle object = argument();
    s_vpi_value value = {0};
    value.format = vpiStringVal;
    vpi_get_value(object, &value);
    vpi_printf("string %s\n", value.value.str);
    value.value.str = "ok";
    vpi_put_value(object, &value, 0, vpiNoDelay);
    return 0;
}
static PLI_INT32 time_call(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    vpi_get_time(0, &time);
    vpi_printf("time %u %u\n", time.high, time.low);
    return 0;
}
static PLI_INT32 lifecycle(p_cb_data data) {
    if (!strcmp(data->user_data, "start") && !vpi_handle_by_name("top.lifecycle_marker", 0))
        return 0;
    vpi_printf("%s\n", data->user_data);
    return 0;
}
static PLI_INT32 lifecycle_setup(PLI_BYTE8* unused) {
    (void)unused;
    s_cb_data cb = {0};
    cb.cb_rtn = lifecycle;
    cb.reason = cbEndOfSimulation;
    cb.user_data = "end";
    vpi_register_cb(&cb);
    return 0;
}
static PLI_INT32 after(p_cb_data data) {
    (void)data;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    vpi_get_time(0, &time);
    vpi_printf("after %u\n", time.low);
    return 0;
}
static PLI_INT32 after_setup(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    time.low = 2;
    s_cb_data cb = {0};
    cb.reason = cbAfterDelay;
    cb.cb_rtn = after;
    cb.time = &time;
    vpi_register_cb(&cb);
    return 0;
}
static PLI_INT32 cancel_setup(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    time.low = 2;
    s_cb_data cb = {0};
    cb.reason = cbAfterDelay;
    cb.cb_rtn = after;
    cb.time = &time;
    vpiHandle handle = vpi_register_cb(&cb);
    vpi_printf("cancel %d\n", vpi_remove_cb(handle));
    return 0;
}
static PLI_INT32 next_time(p_cb_data data) {
    (void)data;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    vpi_get_time(0, &time);
    vpi_printf("next %u\n", time.low);
    return 0;
}
static PLI_INT32 next_setup(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    s_cb_data cb = {0};
    cb.reason = cbNextSimTime;
    cb.cb_rtn = next_time;
    cb.time = &time;
    vpi_register_cb(&cb);
    return 0;
}
static PLI_INT32 synchronized(p_cb_data data) {
    vpi_printf("%s %d\n", data->user_data, read_int(data->obj));
    if (data->reason == cbReadWriteSynch) put_int(data->obj, 2, vpiNoDelay);
    return 0;
}
static PLI_INT32 regions_setup(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle object = argument();
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    s_cb_data cb = {0};
    cb.obj = object;
    cb.time = &time;
    cb.cb_rtn = synchronized;
    cb.reason = cbReadWriteSynch;
    cb.user_data = "rw";
    vpi_register_cb(&cb);
    cb.reason = cbReadOnlySynch;
    cb.user_data = "ro";
    vpi_register_cb(&cb);
    return 0;
}
struct watch_state {
    vpiHandle registration;
};
static PLI_INT32 watched(p_cb_data data) {
    struct watch_state* state = (struct watch_state*)data->user_data;
    vpi_printf("change %d\n", read_int(data->obj));
    vpi_remove_cb(state->registration);
    free(state);
    return 0;
}
static PLI_INT32 watch_setup(PLI_BYTE8* unused) {
    (void)unused;
    struct watch_state* state = calloc(1, sizeof(*state));
    if (!state) {
        vpi_printf("allocation BAD\n");
        return 0;
    }
    s_vpi_time time = {0};
    time.type = vpiSuppressTime;
    s_vpi_value value = {0};
    value.format = vpiSuppressVal;
    s_cb_data cb = {0};
    cb.reason = cbValueChange;
    cb.cb_rtn = watched;
    cb.obj = argument();
    cb.time = &time;
    cb.value = &value;
    cb.user_data = (PLI_BYTE8*)state;
    state->registration = vpi_register_cb(&cb);
    if (!state->registration) {
        free(state);
        vpi_printf("registration BAD\n");
    }
    return 0;
}
static PLI_INT32 delayed_call(PLI_BYTE8* unused) {
    (void)unused;
    s_vpi_time time = {0};
    time.type = vpiSimTime;
    time.low = 2;
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    value.value.integer = 9;
    vpi_put_value(argument(), &value, &time, vpiInertialDelay);
    return 0;
}
static PLI_INT32 force_call(PLI_BYTE8* release) {
    put_int(argument(), 1, release ? vpiReleaseFlag : vpiForceFlag);
    return 0;
}
static void register_task(const char* name, PLI_INT32 (*call)(PLI_BYTE8*), PLI_BYTE8* data) {
    s_vpi_systf_data task = {0};
    task.type = vpiSysTask;
    task.tfname = (PLI_BYTE8*)name;
    task.calltf = call;
    task.user_data = data;
    vpi_register_systf(&task);
}
static void register_all(void) {
    register_task("$k_task", task_call, 0);
    register_task("$k_put_trace", trace_put_call, 0);
    register_task("$k_modules", modules_call, 0);
    register_task("$k_info", info_call, 0);
    register_task("$k_finish", finish_call, 0);
    register_task("$k_error", error_call, 0);
    register_task("$k_vector", vector_call, 0);
    register_task("$k_hierarchy", hierarchy_call, 0);
    register_task("$k_memory", memory_call, 0);
    register_task("$k_real", real_call, 0);
    register_task("$k_string", string_call, 0);
    register_task("$k_time", time_call, 0);
    register_task("$k_lifecycle", lifecycle_setup, 0);
    register_task("$k_after", after_setup, 0);
    register_task("$k_cancel", cancel_setup, 0);
    register_task("$k_next", next_setup, 0);
    register_task("$k_regions", regions_setup, 0);
    register_task("$k_watch", watch_setup, 0);
    register_task("$k_delayed", delayed_call, 0);
    register_task("$k_force", force_call, 0);
    register_task("$k_release", force_call, "release");
    s_vpi_systf_data checked = {0};
    checked.type = vpiSysTask;
    checked.tfname = "$k_checked";
    checked.calltf = task_call;
    checked.compiletf = check_compile;
    vpi_register_systf(&checked);
    s_vpi_systf_data fun = {0};
    fun.type = vpiSysFunc;
    fun.sysfunctype = vpiIntFunc;
    fun.tfname = "$k_add";
    fun.calltf = add_call;
    vpi_register_systf(&fun);
    fun.sysfunctype = vpiSizedFunc;
    fun.tfname = "$k_wide";
    fun.calltf = wide_call;
    fun.sizetf = wide_size;
    vpi_register_systf(&fun);
    s_cb_data cb = {0};
    cb.reason = cbStartOfSimulation;
    cb.cb_rtn = lifecycle;
    cb.user_data = "start";
    vpi_register_cb(&cb);
}
// The external plugin ABI declares a non-const startup table. The fixture
// never changes it; all callback state is allocated per registration.
void (*vlog_startup_routines[])(void) = {register_all, 0};
