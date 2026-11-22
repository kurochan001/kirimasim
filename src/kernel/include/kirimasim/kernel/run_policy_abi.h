// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/object_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_RUN_POLICY 17u
#define KIRIMA_RUN_POLICY_VERSION 1u
enum kirima_diagnostic_action {
    KIRIMA_DIAG_QUIET,
    KIRIMA_DIAG_WARN,
    KIRIMA_DIAG_DESIGN,
    KIRIMA_DIAG_RESOURCE,
    KIRIMA_DIAG_INTERNAL
};
enum kirima_source_check { KIRIMA_CHECK_BOUNDS, KIRIMA_CHECK_CONDITION };
enum kirima_value_init { KIRIMA_INIT_X, KIRIMA_INIT_ZERO, KIRIMA_INIT_ONE, KIRIMA_INIT_RANDOM };
enum kirima_memory_write {
    KIRIMA_MEMORY_WRITTEN,
    KIRIMA_MEMORY_OPEN_ERROR,
    KIRIMA_MEMORY_WRITE_ERROR
};
typedef struct kirima_position {
    uint32_t file, line, column;
} kirima_position;
typedef struct kirima_run_policy_api {
    uint32_t (*create)(kirima_context*, kirima_resource*);
    uint32_t (*release)(kirima_context*, kirima_resource);
    uint32_t (*first_instruction)(kirima_context*, kirima_resource, uint64_t, uint32_t*);
    uint32_t (*first_heap)(kirima_context*, kirima_resource, kirima_position, uint32_t*);
    uint32_t (*occurrence)(kirima_context*, kirima_resource, uint32_t check, kirima_position,
                           uint32_t* first);
    uint32_t (*counts)(kirima_context*, kirima_resource, uint32_t check, uint64_t* total,
                       uint64_t* positions);
    uint32_t (*heap_diagnostic)(kirima_heap_status, uint32_t* action, kirima_byte_sink, void*);
    uint32_t (*object_diagnostic)(uint32_t error, uint32_t* action, kirima_byte_sink, void*);
    uint32_t (*condition)(kirima_bytes kind, uint32_t error, kirima_byte_sink, void*);
    uint32_t (*initialize)(uint32_t mode, uint32_t four_state, uint64_t* rng_state,
                           kirima_value_out);
    uint32_t (*replicate)(kirima_context*, kirima_resource, kirima_bytes, kirima_index,
                          kirima_byte_sink, void*, uint32_t* warn);
    uint32_t (*select_string)(kirima_context*, kirima_w4 condition, uint64_t yes, uint64_t no,
                              uint64_t*);
    uint32_t (*write_memory)(kirima_bytes name, kirima_bytes contents, uint32_t* disposition);
} kirima_run_policy_api;
#ifdef __cplusplus
}
#endif
