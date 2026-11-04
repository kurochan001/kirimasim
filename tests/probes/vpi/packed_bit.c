/* Copyright (c) 2026 The kirimasim Authors */
/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include <string.h>
#include <vpi_user.h>

/* External plugin state is private to this original, single-design fixture. */
static int errors, callbacks, missing_bits, four_state, parent_signed, bit_kind;
static int low_index, high_index, middle_index;
static vpiHandle parent, named_parent, selected, callback_token;

static int number(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    errors += vpi_chk_error(0) != 0;
    return value.value.integer;
}

static void inspect(vpiHandle object, int state) {
    if (!object) {
        ++errors;
        ++missing_bits;
        return;
    }
    errors += vpi_get(vpiSigned, object) != 0;
    errors += vpi_get(vpiSize, object) != 1;
    errors += vpi_get(vpiType, object) != bit_kind;
    errors += number(object) != (state == vpi1);
    s_vpi_value value = {0};
    value.format = vpiVectorVal;
    vpi_get_value(object, &value);
    if (!value.value.vector) {
        ++errors;
        return;
    }
    errors += value.value.vector[0].aval != (state == vpi1 || state == vpiX);
    errors += value.value.vector[0].bval != (state == vpiX || state == vpiZ);
#ifndef PACKED_BIT_REFERENCE_READS
    /* The approved host chooses scalar; the reference chooses integer here. */
    value.format = vpiObjTypeVal;
    vpi_get_value(object, &value);
    errors += value.format != vpiScalarVal || value.value.scalar != state;
#endif
    if (state == vpi0 || state == vpi1) {
        value.format = vpiRealVal;
        vpi_get_value(object, &value);
        errors += value.value.real != (state == vpi1 ? 1.0 : 0.0);
        value.format = vpiDecStrVal;
        vpi_get_value(object, &value);
        errors += !value.value.str || strcmp(value.value.str, state == vpi1 ? "1" : "0");
    }
    errors += vpi_chk_error(0) != 0;
}

#ifndef PACKED_BIT_REFERENCE_READS
static PLI_INT32 changed(p_cb_data data) {
    const int expected = callbacks == 0 ? vpi0 : !four_state ? vpi1 : callbacks == 1 ? vpiX : vpiZ;
    ++callbacks;
    /* Read the callback payload before reentering the host's conversion buffers. */
    errors += !data->value || data->value->format != vpiIntVal ||
              data->value->value.integer != (expected == vpi1);
    inspect(data->obj, expected);
    return 0;
}
#endif

static PLI_INT32 start(PLI_BYTE8* unused) {
    (void)unused;
    errors = callbacks = missing_bits = 0;
    vpiHandle args = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    parent = vpi_scan(args);
    middle_index = number(vpi_scan(args));
    low_index = number(vpi_scan(args));
    high_index = number(vpi_scan(args));
    parent_signed = number(vpi_scan(args));
    four_state = number(vpi_scan(args));
    bit_kind = number(vpi_scan(args)) ? vpiNetBit : vpiRegBit;
    while (vpi_scan(args)) {
    }
    named_parent = vpi_handle_by_name(vpi_get_str(vpiFullName, parent), 0);
    errors += !named_parent;
    errors += vpi_get(vpiSigned, parent) != parent_signed;
    errors += vpi_get(vpiSigned, named_parent) != parent_signed;
    const int indices[] = {low_index, middle_index, high_index};
    for (unsigned i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i) {
        inspect(vpi_handle_by_index(parent, indices[i]), vpi1);
        inspect(vpi_handle_by_index(named_parent, indices[i]), vpi1);
    }
    selected = vpi_handle_by_index(parent, middle_index);
#ifndef PACKED_BIT_REFERENCE_READS
    s_vpi_time time = {0};
    time.type = vpiSuppressTime;
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    s_cb_data cb = {0};
    cb.reason = cbValueChange;
    cb.cb_rtn = changed;
    cb.obj = selected;
    cb.time = &time;
    cb.value = &value;
    callback_token = vpi_register_cb(&cb);
    errors += !callback_token || vpi_chk_error(0) != 0;
#endif
    return 0;
}

static PLI_INT32 check(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle args = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    const int state = number(vpi_scan(args));
    while (vpi_scan(args)) {
    }
    inspect(selected, state);
    inspect(vpi_handle_by_index(named_parent, middle_index), state);
    if (low_index != middle_index) inspect(vpi_handle_by_index(parent, low_index), vpi1);
    if (high_index != middle_index) inspect(vpi_handle_by_index(parent, high_index), vpi1);
    errors += vpi_get(vpiSigned, parent) != parent_signed;
    errors += vpi_get(vpiSigned, named_parent) != parent_signed;
    return 0;
}

static PLI_INT32 finish(PLI_BYTE8* unused) {
    (void)unused;
    if (callback_token) vpi_remove_cb(callback_token);
#ifdef PACKED_BIT_REFERENCE_READS
    vpi_printf("packed-bit-read errors=%d missing=%d\n", errors, missing_bits);
#else
    vpi_printf("packed-bit errors=%d callbacks=%d\n", errors, callbacks);
#endif
    return 0;
}

static void startup(void) {
    s_vpi_systf_data routine = {0};
    routine.type = vpiSysTask;
    routine.tfname = "$packed_bit_start";
    routine.calltf = start;
    vpi_register_systf(&routine);
    routine.tfname = "$packed_bit_check";
    routine.calltf = check;
    vpi_register_systf(&routine);
    routine.tfname = "$packed_bit_finish";
    routine.calltf = finish;
    vpi_register_systf(&routine);
}
void (*vlog_startup_routines[])(void) = {startup, 0};
