// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <vpi_user.h>

static vpiHandle saved_bit, saved_parent;
static vpiHandle parallel_bit, parallel_parent;

static int integer(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    return value.value.integer;
}
static int scalar(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiScalarVal;
    vpi_get_value(object, &value);
    return value.value.scalar;
}
static void put(vpiHandle object, int value) {
    s_vpi_value data = {0};
    data.format = vpiScalarVal;
    data.value.scalar = value;
    vpi_put_value(object, &data, 0, vpiNoDelay);
}
static PLI_INT32 inspect(PLI_BYTE8* mode) {
    vpiHandle iterator = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle object = vpi_scan(iterator);
    int left = integer(vpi_scan(iterator)) - 1024, right = integer(vpi_scan(iterator)) - 1024;
    int is_scalar = integer(vpi_scan(iterator)), is_integer = integer(vpi_scan(iterator));
    int is_signed = integer(vpi_scan(iterator)), width = integer(vpi_scan(iterator));
    int four = integer(vpi_scan(iterator));
    (void)four;
    while (vpi_scan(iterator)) {
    }
    int errors = vpi_get(vpiSize, object) != width;
    errors += integer(vpi_handle(vpiLeftRange, object)) != left;
    errors += integer(vpi_handle(vpiRightRange, object)) != right;
    errors += vpi_get(vpiScalar, object) != is_scalar;
    errors += vpi_get(vpiVector, object) != !is_scalar;
    errors += vpi_get(vpiType, object) != (is_integer ? vpiIntegerVar : vpiReg);
    errors += vpi_get(vpiSigned, object) != is_signed;
    if (*mode == 'c') {
        if (errors)
            vpi_printf("compile errors=%d size=%d left=%d right=%d scalar=%d vector=%d type=%d "
                       "signed=%d\n",
                       errors, vpi_get(vpiSize, object), integer(vpi_handle(vpiLeftRange, object)),
                       integer(vpi_handle(vpiRightRange, object)), vpi_get(vpiScalar, object),
                       vpi_get(vpiVector, object), vpi_get(vpiType, object),
                       vpi_get(vpiSigned, object));
        if (errors)
            vpi_printf(
                "expected size=%d left=%d right=%d scalar=%d vector=%d integer=%d signed=%d\n",
                width, left, right, is_scalar, !is_scalar, is_integer, is_signed);
        return 0;
    }
    int indices[3] = {left, right, left + (right - left) / 2};
    for (int i = 0; i < 3; ++i) {
        vpiHandle bit = vpi_handle_by_index(object, indices[i]);
        if (!bit) {
            ++errors;
            continue;
        }
        errors += scalar(bit) != vpi1;
        errors += vpi_get(vpiSize, bit) != 1 || vpi_get(vpiSigned, bit) != 0;
#ifndef DECLARATION_REFERENCE
        vpiHandle parent = vpi_handle(vpiParent, bit);
        errors += !parent || vpi_get(vpiSize, parent) != width;
        errors += !parent || integer(vpi_handle(vpiLeftRange, parent)) != left;
        s_vpi_value nearest = {0};
        nearest.format = vpiObjTypeVal;
        vpi_get_value(bit, &nearest);
        errors += nearest.format != vpiScalarVal || nearest.value.scalar != vpi1;
#endif
        put(bit, vpi0);
        errors += scalar(bit) != vpi0;
        put(bit, vpi1);
        errors += scalar(bit) != vpi1;
#ifndef DECLARATION_REFERENCE
        put(bit, vpiX);
        errors += scalar(bit) != (four ? vpiX : vpi0);
        put(bit, vpiZ);
        errors += scalar(bit) != (four ? vpiZ : vpi0);
        put(bit, vpi1);
#endif
    }
    vpiHandle first = vpi_handle_by_index(object, left);
    if (first) put(first, vpi0);
    int low = left < right ? left : right, high = left > right ? left : right;
    errors += vpi_handle_by_index(object, low - 1) != 0;
    errors += vpi_handle_by_index(object, high + 1) != 0;
    vpi_printf("declaration errors=%d\n", errors);
    return 0;
}
static int expired(vpiHandle object) {
    s_vpi_value value = {0};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    return value.format == vpiSuppressVal && vpi_chk_error(0) == vpiError;
}
static PLI_INT32 compile(PLI_BYTE8* unused) {
    (void)unused;
    return inspect("c");
}
static PLI_INT32 readonly(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle iterator = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle object = vpi_scan(iterator);
    while (vpi_scan(iterator)) {
    }
    vpiHandle bit = vpi_handle_by_index(object, -3);
    int ok = bit && scalar(bit) == vpi1;
    if (bit) {
        put(bit, vpi0);
        ok = ok && vpi_chk_error(0) == vpiError;
        ok = ok && scalar(bit) == vpi1;
    }
    vpi_printf("readonly %s\n", ok ? "ok" : "BAD");
    return 0;
}
static PLI_INT32 parallel(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle iterator = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle object = vpi_scan(iterator);
    int second = integer(vpi_scan(iterator));
    while (vpi_scan(iterator)) {
    }
    if (!second) {
        parallel_bit = vpi_handle_by_index(object, -3);
        parallel_parent = parallel_bit ? vpi_handle(vpiParent, parallel_bit) : 0;
        vpi_printf("parallel saved\n");
        return 0;
    }
    int ok = parallel_bit && parallel_parent && scalar(parallel_bit) == vpi1;
    if (parallel_bit) put(parallel_bit, vpi0);
    vpiHandle old_bit = parallel_parent ? vpi_handle_by_index(parallel_parent, -3) : 0;
    vpiHandle new_bit = vpi_handle_by_index(object, -3);
    ok = ok && old_bit && new_bit && scalar(old_bit) == vpi0 && scalar(new_bit) == vpi1;
    vpi_printf("parallel %s\n", ok ? "ok" : "BAD");
    return 0;
}
static PLI_INT32 keep(PLI_BYTE8* unused) {
    (void)unused;
    vpiHandle iterator = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle object = vpi_scan(iterator);
    while (vpi_scan(iterator)) {
    }
    int old_bit_ok = !saved_bit || expired(saved_bit);
    int old_parent_ok = !saved_parent || expired(saved_parent);
    int ok = old_bit_ok && old_parent_ok;
    if (saved_bit) {
        put(saved_bit, vpi0);
        ok = ok && vpi_chk_error(0) == vpiError;
    }
    saved_bit = vpi_handle_by_index(object, -3);
    saved_parent = saved_bit ? vpi_handle(vpiParent, saved_bit) : 0;
    ok = ok && saved_bit && saved_parent && scalar(saved_bit) == vpi1;
    // Numeric transient views must not become globally discoverable names.
    char name[] = "value[-3]";
    ok = ok && vpi_handle_by_name(name, 0) == 0;
    if (!ok)
        vpi_printf("lifetime details bit=%d parent=%d new_bit=%d new_parent=%d\n", old_bit_ok,
                   old_parent_ok, saved_bit != 0, saved_parent != 0);
    vpi_printf("lifetime %s\n", ok ? "ok" : "BAD");
    return 0;
}
static void start(void) {
    s_vpi_systf_data data = {0};
    data.type = vpiSysTask;
    data.tfname = "$decl";
    data.compiletf = compile;
    data.calltf = inspect;
    data.user_data = "r";
    // Registration copies the data; compile and runtime use separate adapters.
    vpi_register_systf(&data);
    data.tfname = "$decl_keep";
    data.compiletf = 0;
    data.calltf = keep;
    vpi_register_systf(&data);
    data.tfname = "$decl_readonly";
    data.calltf = readonly;
    vpi_register_systf(&data);
    data.tfname = "$decl_parallel";
    data.calltf = parallel;
    vpi_register_systf(&data);
}
void (*vlog_startup_routines[])(void) = {start, 0};
