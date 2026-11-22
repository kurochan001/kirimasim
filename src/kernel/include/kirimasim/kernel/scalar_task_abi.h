// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/io_abi.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_SCALAR_TASK 5u
#define KIRIMA_SCALAR_TASK_VERSION 1u
typedef struct kirima_index {
    int64_t value;
    uint32_t known, reserved;
} kirima_index;
typedef struct kirima_string_stats {
    uint64_t live, entries, bytes;
    uint32_t pressure, reserved;
} kirima_string_stats;
enum kirima_distribution {
    KIRIMA_RANDOM,
    KIRIMA_DIST_UNIFORM,
    KIRIMA_DIST_NORMAL,
    KIRIMA_DIST_EXPONENTIAL,
    KIRIMA_DIST_POISSON,
    KIRIMA_DIST_CHI_SQUARE,
    KIRIMA_DIST_T,
    KIRIMA_DIST_ERLANG
};
enum kirima_dist_argument {
    KIRIMA_DIST_VALID,
    KIRIMA_DIST_MEAN,
    KIRIMA_DIST_DF,
    KIRIMA_DIST_STAGE
};
typedef struct kirima_dist_result {
    int32_t value;
    uint32_t bad_argument;
} kirima_dist_result;
/* Text handles belong to one Context. As in the native string table an
 * expired handle reads as empty, and a mark of it fails. The host publishes
 * all roots before a collection. Return KIRIMA_VALUE_OK, INVALID or RESOURCE. */
typedef struct kirima_scalar_task_api {
    uint32_t (*intern)(kirima_context*, kirima_bytes, uint32_t discard_nul, uint64_t* handle);
    uint32_t (*get)(kirima_context*, uint64_t handle, kirima_byte_sink, void*);
    uint32_t (*byte_at)(kirima_context*, uint64_t, kirima_index, uint32_t* byte);
    uint32_t (*put_byte)(kirima_context*, uint64_t, kirima_index, uint32_t byte, uint64_t*);
    uint32_t (*substring)(kirima_context*, uint64_t, kirima_index first, kirima_index last,
                          uint64_t*);
    uint32_t (*change_case)(kirima_context*, uint64_t, uint32_t upper, uint64_t*);
    uint32_t (*compare)(kirima_context*, uint64_t lhs, uint64_t rhs, uint32_t ignore_case,
                        int32_t*);
    uint32_t (*to_integer)(kirima_context*, uint64_t, uint32_t base, uint32_t*);
    uint32_t (*to_real)(kirima_context*, uint64_t, double*);
    uint32_t (*begin_collection)(kirima_context*, const uint64_t* roots, uint64_t count);
    uint32_t (*mark)(kirima_context*, uint64_t);
    uint32_t (*end_collection)(kirima_context*, uint32_t commit);
    uint32_t (*stats)(kirima_context*, kirima_string_stats*);
    uint32_t (*initial_rng)(uint64_t seed, kirima_bytes name, uint64_t* state);
    uint32_t (*child_rng)(uint64_t* parent, uint64_t* child);
    uint32_t (*draw)(uint64_t* state, const uint32_t* seed, uint32_t* value);
    uint32_t (*draw_range)(uint64_t* state, uint32_t upper, uint32_t lower, uint32_t* value);
    /* Only KIRIMA_RANDOM accepts a NULL seed to select the Context's shared signed seed. */
    uint32_t (*distribution)(kirima_context*, uint32_t kind, int32_t* seed, int32_t a, int32_t b,
                             kirima_dist_result*);
} kirima_scalar_task_api;
#ifdef __cplusplus
}
#endif
