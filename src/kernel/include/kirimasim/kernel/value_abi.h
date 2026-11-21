// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

/* Service 1, version 1 of kirima_kernel_api.query_service (RUNTIME_ABI 4.2).
 * C and C++ see the same POD layout. The C++ spelling preserves W4's native
 * type, so a kernel operation can directly use the caller's storage. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
#include "kirimasim/kernel/w4.h"
using kirima_w4 = kirimasim::kernel::W4;
extern "C" {
#else
typedef struct kirima_w4 {
    uint64_t a, b;
} kirima_w4;
#endif

#define KIRIMA_SERVICE_VALUE 1u
#define KIRIMA_VALUE_VERSION 1u

typedef struct kirima_value_in {
    const kirima_w4* words;
    uint32_t width, count;
} kirima_value_in;
typedef struct kirima_value_out {
    kirima_w4* words;
    uint32_t width, count;
} kirima_value_out;

enum kirima_value_status {
    KIRIMA_VALUE_OK,
    KIRIMA_VALUE_INVALID,
    KIRIMA_VALUE_CAPACITY,
    KIRIMA_VALUE_RESOURCE
};
enum kirima_value_unary { KIRIMA_TO_TWO, KIRIMA_BIT_NOT, KIRIMA_NEGATE };
enum kirima_value_binary {
    KIRIMA_BIT_AND,
    KIRIMA_BIT_OR,
    KIRIMA_BIT_XOR,
    KIRIMA_BIT_XNOR,
    KIRIMA_ADD,
    KIRIMA_SUBTRACT,
    KIRIMA_MULTIPLY,
    KIRIMA_DIVIDE,
    KIRIMA_MODULO
};
enum kirima_value_comparison {
    KIRIMA_LT,
    KIRIMA_LE,
    KIRIMA_GT,
    KIRIMA_GE,
    KIRIMA_EQ,
    KIRIMA_NE,
    KIRIMA_CASE_EQ,
    KIRIMA_CASE_NE,
    KIRIMA_WILD_EQ,
    KIRIMA_WILD_NE,
    KIRIMA_CASE_Z,
    KIRIMA_CASE_X
};
enum kirima_value_reduction {
    KIRIMA_RED_AND,
    KIRIMA_RED_OR,
    KIRIMA_RED_XOR,
    KIRIMA_TRUTH,
    KIRIMA_HAS_UNKNOWN
};
enum kirima_value_shift { KIRIMA_SHIFT_LEFT, KIRIMA_SHIFT_RIGHT, KIRIMA_SHIFT_ARITH_RIGHT };
enum kirima_value_count_mask {
    KIRIMA_COUNT_ZERO = 1,
    KIRIMA_COUNT_ONE = 2,
    KIRIMA_COUNT_Z = 4,
    KIRIMA_COUNT_X = 8
};

/* Spans describe caller-owned, nonoverlapping buffers, least significant cell
 * first. All spans have exactly ceil(width/64) cells. Inputs and initialized
 * destinations have zero unused high bits. Boolean flags accept 0 or 1. */
typedef struct kirima_value_api {
    uint32_t (*fill_x)(kirima_value_out dst);
    uint32_t (*unary)(uint32_t op, kirima_value_out dst, kirima_value_in x);
    uint32_t (*binary)(uint32_t op, kirima_value_out dst, kirima_value_in x, kirima_value_in y,
                       uint32_t is_signed);
    uint32_t (*power)(kirima_value_out dst, kirima_value_in x, uint32_t x_signed,
                      kirima_value_in exponent, uint32_t exp_signed);
    uint32_t (*compare)(uint32_t op, kirima_w4* dst, kirima_value_in x, kirima_value_in y,
                        uint32_t is_signed);
    uint32_t (*reduce)(uint32_t op, kirima_w4* dst, kirima_value_in x);
    uint32_t (*count_bits)(uint32_t* dst, kirima_value_in x, uint32_t mask);
    uint32_t (*shift)(uint32_t op, kirima_value_out dst, kirima_value_in x, kirima_value_in amount,
                      uint32_t is_signed);
    uint32_t (*resize)(kirima_value_out dst, kirima_value_in x, uint32_t sign_extend);
    uint32_t (*select)(kirima_value_out dst, kirima_w4 condition, kirima_value_in yes,
                       kirima_value_in no);
    uint32_t (*slice)(kirima_value_out dst, kirima_value_in x, int64_t lsb);
    /* Replace only the bits of the initialized destination inside its width. */
    uint32_t (*write_bits)(kirima_value_out dst, int64_t lsb, kirima_value_in value);
    uint32_t (*position)(int64_t* value, uint32_t* known, kirima_value_in x, uint32_t is_signed);
    uint32_t (*to_real)(double* dst, kirima_value_in x, uint32_t is_signed, uint32_t short_real);
    uint32_t (*from_real)(kirima_value_out dst, double value, uint32_t four_state,
                          uint32_t truncate);
    /* With dst NULL and capacity zero only the size is returned. No terminating NUL. */
    uint32_t (*to_text)(char* dst, uint64_t capacity, uint64_t* size, kirima_value_in x);
    uint32_t (*from_text)(kirima_value_out dst, const char* text, uint64_t size);
} kirima_value_api;

#ifdef __cplusplus
}
#endif
