// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/scalar_task_abi.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_CONTAINER 7u
#define KIRIMA_CONTAINER_VERSION 1u
enum kirima_heap_kind { KIRIMA_CONTAINER, KIRIMA_CELL, KIRIMA_OBJECT };
enum kirima_sequence_kind { KIRIMA_DYNAMIC_ARRAY, KIRIMA_QUEUE };
enum kirima_heap_error {
    KIRIMA_HEAP_OK,
    KIRIMA_HEAP_TYPE,
    KIRIMA_HEAP_HANDLE,
    KIRIMA_HEAP_SIZE,
    KIRIMA_HEAP_RESOURCE
};
enum kirima_heap_warning {
    KIRIMA_HEAP_QUIET,
    KIRIMA_HEAP_INDEX,
    KIRIMA_HEAP_EMPTY,
    KIRIMA_HEAP_BOUND,
    KIRIMA_HEAP_QUEUE_WRITE
};
typedef struct kirima_heap_status {
    uint32_t error, warning;
} kirima_heap_status;
/* The all-zero token is the empty value. Nonempty tokens retain owner,
 * kind and generation intact. They are borrowed and must be rooted at GC. */
typedef struct kirima_heap_token {
    const kirima_context* owner;
    uint64_t bits;
    uint32_t kind, reserved;
} kirima_heap_token;
typedef struct kirima_heap_word {
    uint64_t mask;
    kirima_w4 initial;
    uint32_t four_state, string_edge, nested_type, object_class;
} kirima_heap_word;
typedef struct kirima_heap_type {
    uint32_t kind, element_layout;
    uint64_t capacity;
} kirima_heap_type;
typedef struct kirima_heap_key_part {
    uint32_t width, signed_integral, string_key;
} kirima_heap_key_part;
typedef struct kirima_heap_position {
    int64_t index;
    uint32_t found, valid;
} kirima_heap_position;
typedef uint64_t (*kirima_fresh_word)(void*);
typedef uint32_t (*kirima_string_mark)(void*, uint64_t string_handle);
typedef struct kirima_heap_stats {
    uint64_t containers, cells, container_entries, cell_entries;
    uint32_t needs_collection, under_pressure;
} kirima_heap_stats;
typedef struct kirima_words_in {
    const kirima_w4* data;
    uint32_t count;
} kirima_words_in;
typedef struct kirima_words_out {
    kirima_w4* data;
    uint32_t count;
} kirima_words_out;

/* Rows belong to their originating Context; absent nested/class rows use
 * UINT32_MAX. Word counts are flattened layout words. ABI call status is
 * independent of the returned SV/native error and warning. Synchronous
 * callbacks cannot throw, reenter or collect. A nonzero mark result means
 * success. A model must publish all roots on every permitted collection. */
typedef struct kirima_container_api {
    uint32_t (*add_layout)(kirima_context*, const kirima_heap_word*, uint32_t count,
                           uint32_t* layout, kirima_heap_status*);
    uint32_t (*add_type)(kirima_context*, kirima_heap_type, uint32_t* type, kirima_heap_status*);
    uint32_t (*describe_type)(kirima_context*, uint32_t type, kirima_heap_type*,
                              kirima_heap_status*);
    uint32_t (*describe_word)(kirima_context*, uint32_t layout, uint32_t at, kirima_heap_word*,
                              kirima_heap_status*);
    uint32_t (*word_count)(kirima_context*, uint32_t layout, uint64_t*);
    uint32_t (*actual_type)(kirima_context*, kirima_heap_token, uint32_t*, kirima_heap_status*);
    uint32_t (*construct)(kirima_context*, uint32_t type, kirima_index size,
                          kirima_heap_token initializer, kirima_fresh_word fresh, void*,
                          kirima_heap_token*, kirima_heap_status*);
    uint32_t (*copy)(kirima_context*, uint32_t destination_type, kirima_heap_token,
                     kirima_heap_token*, kirima_heap_status*);
    uint32_t (*snapshot)(kirima_context*, kirima_heap_token, kirima_heap_token*,
                         kirima_heap_status*);
    uint32_t (*from_words)(kirima_context*, uint32_t type, kirima_words_in, kirima_heap_token*,
                           kirima_heap_status*);
    uint32_t (*size)(kirima_context*, uint32_t type, kirima_heap_token, uint64_t*,
                     kirima_heap_status*);
    uint32_t (*read)(kirima_context*, uint32_t type, kirima_heap_token, kirima_index,
                     kirima_words_out, kirima_heap_status*);
    uint32_t (*write)(kirima_context*, uint32_t type, kirima_heap_token*, kirima_index,
                      kirima_words_in, kirima_heap_status*);
    uint32_t (*cell)(kirima_context*, uint32_t type, kirima_heap_token, kirima_index,
                     kirima_heap_token*, kirima_heap_status*);
    uint32_t (*read_cell)(kirima_context*, uint32_t layout, kirima_heap_token, kirima_words_out,
                          kirima_heap_status*);
    uint32_t (*write_cell)(kirima_context*, uint32_t layout, kirima_heap_token, kirima_words_in,
                           kirima_heap_status*);
    uint32_t (*store_nested)(kirima_context*, uint32_t layout, kirima_heap_token cell,
                             kirima_heap_token container, kirima_heap_status*);
    uint32_t (*insert)(kirima_context*, uint32_t type, kirima_heap_token*, kirima_index,
                       kirima_words_in, kirima_heap_status*);
    uint32_t (*push)(kirima_context*, uint32_t type, kirima_heap_token*, uint32_t front,
                     kirima_words_in, kirima_heap_status*);
    uint32_t (*pop)(kirima_context*, uint32_t type, kirima_heap_token*, uint32_t front,
                    kirima_words_out, kirima_heap_status*);
    uint32_t (*erase)(kirima_context*, uint32_t type, kirima_heap_token*, kirima_index,
                      kirima_heap_status*);
    uint32_t (*clear)(kirima_context*, uint32_t type, kirima_heap_token*, kirima_heap_status*);
    uint32_t (*swap_values)(kirima_context*, uint32_t type, kirima_heap_token, kirima_index first,
                            kirima_index second, kirima_heap_status*);
    uint32_t (*locate_key)(kirima_context*, uint32_t type, kirima_heap_token,
                           const kirima_heap_key_part*, uint32_t parts, kirima_words_in key,
                           kirima_words_out cell, kirima_heap_position*, kirima_heap_status*);
    uint32_t (*reaches)(kirima_context*, const kirima_heap_token* roots, uint32_t count,
                        kirima_heap_token target, uint32_t*, kirima_heap_status*);
    uint32_t (*collect)(kirima_context*, const kirima_heap_token* roots, uint32_t count,
                        kirima_string_mark, void*, kirima_heap_status*);
    uint32_t (*stats)(kirima_context*, kirima_heap_stats*);
} kirima_container_api;
#ifdef __cplusplus
}
#endif
