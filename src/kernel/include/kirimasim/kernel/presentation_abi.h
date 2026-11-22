// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/reference_abi.h"
#include "kirimasim/kernel/scalar_task_abi.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_PRESENTATION 6u
#define KIRIMA_PRESENTATION_VERSION 1u
enum kirima_format_kind {
    KIRIMA_FORMAT_INTEGRAL,
    KIRIMA_FORMAT_REAL,
    KIRIMA_FORMAT_STRING,
    KIRIMA_FORMAT_EMPTY,
    KIRIMA_FORMAT_PATTERN
};
typedef struct kirima_format_arg {
    uint32_t kind, four_state, is_signed, has_pattern;
    kirima_value_in integral;
    double real;
    kirima_bytes text, named_pattern, positional_pattern;
    const kirima_w4* strength;
    uint32_t strength_width, strength_count;
} kirima_format_arg;
typedef struct kirima_format_context {
    kirima_bytes scope, cell, suffix;
    uint64_t tick_scale;
    int32_t precision, unit, decimals, min_width;
} kirima_format_context;
typedef struct kirima_memory_range {
    const kirima_activation_dim* dimensions;
    uint32_t dimension_count, reserved;
    kirima_index start, finish; /* known means supplied; finish requires start */
} kirima_memory_range;
enum kirima_mem_diagnostic { KIRIMA_MEM_RANGE, KIRIMA_MEM_COUNT, KIRIMA_MEM_INPUT };
typedef void (*kirima_mem_diagnostic_sink)(void*, uint32_t kind, kirima_bytes);
typedef void (*kirima_element_sink)(void*, uint32_t element);

/* All views, including result views delivered to a sink, expire on return.
 * Malformed ABI input returns INVALID before any sink runs. For an SV input
 * error, the kernel delivers the valid prefix of the memory, then the diagnostic. */
typedef struct kirima_presentation_api {
    uint32_t (*format)(kirima_bytes, const kirima_format_arg*, uint32_t count,
                       const kirima_format_context*, kirima_byte_sink formatted,
                       kirima_byte_sink diagnostic, void*, uint32_t* argument_mismatch);
    uint32_t (*load_memory)(kirima_bytes text, const kirima_memory_range*, uint32_t width,
                            uint32_t hex, uint32_t two_state, const kirima_value_in* enum_items,
                            uint32_t item_count, kirima_scan_sink word,
                            kirima_mem_diagnostic_sink diagnostic, void*);
    uint32_t (*dump_order)(const kirima_memory_range*, kirima_element_sink, void*);
} kirima_presentation_api;
#ifdef __cplusplus
}
#endif
