// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <stddef.h>

#include "kirimasim/kernel/reference_abi.h"

_Static_assert(sizeof(kirima_reference) == 16 && _Alignof(kirima_reference) == 8 &&
                   offsetof(kirima_reference, ticket) == 8,
               "reference cell");
_Static_assert(sizeof(kirima_ref_window) == 16 && sizeof(kirima_activation_dim) == 16,
               "activation windows and dimensions");

int check_c_reference_model(const kirima_kernel_api* table, kirima_context* c,
                            kirima_context* other, uint64_t string_handle,
                            kirima_reference* exported) {
    const kirima_reference_api* r =
        table->query_service(KIRIMA_SERVICE_REFERENCE, KIRIMA_REFERENCE_VERSION);
    if (!r || table->query_service(KIRIMA_SERVICE_REFERENCE, 2)) return 0;
    kirima_activation_dim dims[] = {{2, 1}};
    kirima_activation_field fields[] = {{KIRIMA_FIELD_FOUR, 129, UINT32_MAX, 1, dims},
                                        {KIRIMA_FIELD_STRING, 64, UINT32_MAX, 0, NULL}};
    kirima_activation_layout layout = {0};
    kirima_reference lease = {0}, borrowed = {0}, held = {0}, readonly = {0}, sub = {0}, str = {0};
    kirima_reference untouched = {other, 88};
    kirima_ref_window whole = {0, 2, 0, 0};
    if (r->add_layout(c, fields, 2, &layout) ||
        r->create(other, layout, &untouched) != KIRIMA_REF_LAYOUT || r->create(c, layout, &lease) ||
        r->bind(c, lease, 0, whole, 0, &borrowed) || r->retain(c, borrowed, &held) ||
        held.ticket == borrowed.ticket ||
        r->select(c, held, (kirima_ref_window){1, 1, 63, 65}, 1, &readonly) ||
        r->select(c, readonly, (kirima_ref_window){0, 1, 0, 0}, 0, &sub) ||
        r->select(c, held, (kirima_ref_window){1, 2, 0, 0}, 0, &untouched) != KIRIMA_REF_BOUNDS ||
        r->select(c, readonly, (kirima_ref_window){0, 1, UINT32_MAX, 1}, 0, &untouched) !=
            KIRIMA_REF_BOUNDS ||
        r->bind(c, lease, 1, (kirima_ref_window){0, 1, 0, 0}, 0, &str) ||
        r->bind(other, lease, 0, whole, 0, &untouched) != KIRIMA_REF_INVALID ||
        untouched.owner != other || untouched.ticket != 88)
        return 0;
    kirima_w4 words[] = {{UINT64_C(1) << 63, 0}, {5, 4}, {1, 1}};
    kirima_w4 string_cell = {string_handle, 0};
    kirima_w4 result[3] = {{99, 0}, {99, 0}, {99, 0}};
    if (r->write(c, str, 0, (kirima_value_in){&string_cell, 64, 1}) ||
        r->write(c, borrowed, 1, (kirima_value_in){words, 129, 3}) ||
        r->write(c, sub, 0, (kirima_value_in){words, 65, 2}) != KIRIMA_REF_READONLY ||
        r->read(c, readonly, 0, (kirima_value_out){result, 65, 2}) || result[0].a != 11 ||
        result[0].b != 8 || result[1].a || result[1].b ||
        r->read(other, borrowed, 1, (kirima_value_out){result, 129, 3}) != KIRIMA_REF_INVALID ||
        r->read(c, readonly, 1, (kirima_value_out){result, 65, 2}) != KIRIMA_REF_BOUNDS ||
        result[2].a != 99 || r->release(c, lease) || r->release(c, lease) != KIRIMA_REF_INVALID ||
        r->retain(c, lease, &untouched) != KIRIMA_REF_INVALID ||
        r->read(c, borrowed, 1, (kirima_value_out){result, 129, 3}) || result[2].b != 1 ||
        r->retain(c, held, exported) || r->release(c, held) || r->release(c, sub) ||
        r->release(c, readonly) || r->release(c, str) || r->release(c, borrowed))
        return 0;

    kirima_reference next, stale;
    if (r->create(c, layout, &next) || r->bind(c, next, 0, whole, 0, &stale) ||
        r->release(c, next) || r->create(c, layout, &next) ||
        r->read(c, stale, 0, (kirima_value_out){result, 129, 3}) != KIRIMA_REF_INVALID ||
        r->retain(c, stale, &untouched) != KIRIMA_REF_INVALID || r->release(c, stale) ||
        r->release(c, next))
        return 0;
    fields[0].width = 0;
    kirima_activation_layout unchanged = {other, 4, 0};
    return r->add_layout(c, fields, 2, &unchanged) == KIRIMA_REF_LAYOUT &&
           unchanged.owner == other && unchanged.index == 4 && untouched.owner == other &&
           untouched.ticket == 88;
}
