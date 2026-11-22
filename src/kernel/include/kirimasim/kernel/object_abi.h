// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/resource_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_OBJECT 8u
#define KIRIMA_OBJECT_VERSION 1u
enum kirima_object_status {
    KIRIMA_OBJ_OK,
    KIRIMA_OBJ_NULL,
    KIRIMA_OBJ_TOKEN,
    KIRIMA_OBJ_CLASS,
    KIRIMA_OBJ_FIELD,
    KIRIMA_OBJ_VALUE,
    KIRIMA_OBJ_STAGE,
    KIRIMA_OBJ_CONST_WRITE,
    KIRIMA_OBJ_METHOD,
    KIRIMA_OBJ_RESOURCE,
    KIRIMA_OBJ_BUSY,
    KIRIMA_OBJ_CAST,
    KIRIMA_OBJ_ABI
};
typedef struct kirima_class_field {
    uint32_t layout, constant, has_initializer;
} kirima_class_field;
typedef struct kirima_class_descriptor {
    kirima_bytes name;
    uint32_t base, weak_target, abstract_class, interface_class;
    const uint32_t* interfaces;
    const kirima_class_field* fields;
    const uint32_t* virtual_methods;
    uint32_t interface_count, field_count, method_count;
} kirima_class_descriptor;
typedef struct kirima_object_info {
    uint32_t dynamic_class, constructed;
    uint64_t identity, ordinal, random_state;
} kirima_object_info;
typedef struct kirima_pinned_object {
    kirima_heap_token value;
    kirima_resource pin;
} kirima_pinned_object;
/* Absent class IDs are UINT32_MAX. Word views are borrowed during the call.
 * Bindings pin the captured receiver and carry its access rights through
 * every subsequent selection. Finish consumes a successful stage ticket.
 * Drop any unused pin, binding or unfinished constructor before Context teardown. */
typedef struct kirima_object_api {
    uint32_t (*add_classes)(kirima_context*, const kirima_class_descriptor*, uint32_t count);
    uint32_t (*create)(kirima_context*, uint32_t type, uint64_t* parent_rng, kirima_pinned_object*);
    uint32_t (*copy)(kirima_context*, kirima_heap_token, kirima_pinned_object*);
    uint32_t (*pin)(kirima_context*, kirima_heap_token, kirima_pinned_object*);
    uint32_t (*info)(kirima_context*, kirima_heap_token, kirima_object_info*);
    uint32_t (*identity)(kirima_context*, kirima_heap_token, uint64_t*);
    uint32_t (*equal)(kirima_context*, kirima_heap_token, kirima_heap_token, uint32_t*);
    uint32_t (*truth)(kirima_context*, kirima_heap_token, uint32_t*);
    uint32_t (*select)(kirima_context*, kirima_w4 condition, kirima_heap_token yes,
                       kirima_heap_token no, kirima_heap_token*);
    uint32_t (*cast)(kirima_context*, kirima_heap_token, uint32_t source, uint32_t destination,
                     uint32_t required, uint32_t*);
    uint32_t (*seed)(kirima_context*, kirima_heap_token, uint32_t seed);
    uint32_t (*virtual_method)(kirima_context*, kirima_heap_token, uint32_t view, uint32_t slot,
                               uint32_t*);
    uint32_t (*to_word)(kirima_context*, kirima_heap_token, kirima_w4*);
    uint32_t (*from_word)(kirima_context*, kirima_w4, uint32_t view, kirima_heap_token*);
    uint32_t (*bind)(kirima_context*, kirima_heap_token, uint32_t declaring, uint32_t field,
                     uint32_t readonly, kirima_resource*);
    uint32_t (*select_words)(kirima_context*, kirima_resource, uint64_t first, uint64_t count,
                             kirima_resource*);
    uint32_t (*select_bits)(kirima_context*, kirima_resource, uint64_t lsb, uint64_t width,
                            kirima_resource*);
    uint32_t (*read)(kirima_context*, kirima_resource, kirima_words_out);
    uint32_t (*write)(kirima_context*, kirima_resource, kirima_words_in, kirima_resource stage);
    uint32_t (*begin_stage)(kirima_context*, kirima_heap_token, uint32_t declaring,
                            kirima_resource*);
    uint32_t (*begin_body)(kirima_context*, kirima_resource);
    uint32_t (*finish_stage)(kirima_context*, kirima_resource);
    uint32_t (*new_weak)(kirima_context*, uint32_t type, kirima_heap_token target,
                         uint64_t* parent_rng, kirima_pinned_object*);
    uint32_t (*lock_weak)(kirima_context*, kirima_heap_token, kirima_pinned_object*);
    uint32_t (*clear_weak)(kirima_context*, kirima_heap_token);
    uint32_t (*release)(kirima_context*, kirima_resource);
} kirima_object_api;
#ifdef __cplusplus
}
#endif
