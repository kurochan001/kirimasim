// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/abi.h"
#include "kirimasim/kernel/value_abi.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_REFERENCE 3u
#define KIRIMA_REFERENCE_VERSION 1u

typedef struct kirima_reference {
    const kirima_context* owner;
    uint64_t ticket;
} kirima_reference;
typedef struct kirima_activation_layout {
    const kirima_context* owner;
    uint32_t index, reserved;
} kirima_activation_layout;
typedef struct kirima_activation_dim {
    int64_t left, right;
} kirima_activation_dim;
enum kirima_activation_kind {
    KIRIMA_FIELD_TWO,
    KIRIMA_FIELD_FOUR,
    KIRIMA_FIELD_REAL,
    KIRIMA_FIELD_SHORTREAL,
    KIRIMA_FIELD_STRING,
    KIRIMA_FIELD_CHANDLE,
    KIRIMA_FIELD_CONTAINER,
    KIRIMA_FIELD_CELL,
    KIRIMA_FIELD_EVENT,
    KIRIMA_FIELD_OBJECT
};
typedef struct kirima_activation_field {
    uint32_t kind, width, class_view, dimension_count;
    const kirima_activation_dim* dimensions;
} kirima_activation_field;
typedef struct kirima_ref_window {
    uint32_t first_element, elements, lsb, width;
} kirima_ref_window;
enum kirima_ref_status {
    KIRIMA_REF_OK,
    KIRIMA_REF_LAYOUT,
    KIRIMA_REF_INVALID,
    KIRIMA_REF_BOUNDS,
    KIRIMA_REF_READONLY,
    KIRIMA_REF_VALUE,
    KIRIMA_REF_RESOURCE
};
/* class_view is UINT32_MAX except for object fields. Bind with width zero for
 * the whole field, or select with width zero for the rest of the current window.
 * A bind/select creates a borrowed ticket. retain creates an independent owning
 * ticket. Release every ticket, including stale borrows. Outputs change only on
 * success. No entry stores a pointer into caller-owned buffers. */
typedef struct kirima_reference_api {
    uint32_t (*add_layout)(kirima_context*, const kirima_activation_field*, uint32_t count,
                           kirima_activation_layout*);
    uint32_t (*create)(kirima_context*, kirima_activation_layout, kirima_reference*);
    uint32_t (*bind)(kirima_context*, kirima_reference lease, uint32_t field, kirima_ref_window,
                     uint32_t read_only, kirima_reference*);
    uint32_t (*select)(kirima_context*, kirima_reference view, kirima_ref_window relative,
                       uint32_t read_only, kirima_reference*);
    uint32_t (*retain)(kirima_context*, kirima_reference, kirima_reference*);
    uint32_t (*release)(kirima_context*, kirima_reference);
    uint32_t (*read)(kirima_context*, kirima_reference, uint32_t element, kirima_value_out);
    uint32_t (*write)(kirima_context*, kirima_reference, uint32_t element, kirima_value_in);
} kirima_reference_api;
#ifdef __cplusplus
}
#endif
