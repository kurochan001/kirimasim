// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/presentation_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_TEXT 14u
#define KIRIMA_TEXT_VERSION 1u
typedef struct kirima_format_piece {
    kirima_bytes text, real_format;
    uint32_t specifier, conversion, left, zero;
    int32_t width;
} kirima_format_piece;
typedef void (*kirima_piece_sink)(void*, const kirima_format_piece*);
typedef struct kirima_pattern_name {
    kirima_bytes text;
    uint32_t escaped;
} kirima_pattern_name;
enum kirima_pattern_kind {
    KIRIMA_PATTERN_INTEGER,
    KIRIMA_PATTERN_REAL,
    KIRIMA_PATTERN_STRING,
    KIRIMA_PATTERN_ENUM,
    KIRIMA_PATTERN_RECORD,
    KIRIMA_PATTERN_ARRAY,
    KIRIMA_PATTERN_ASSOC,
    KIRIMA_PATTERN_UNION,
    KIRIMA_PATTERN_TAGGED,
    KIRIMA_PATTERN_HANDLE
};
typedef struct kirima_typed_integer {
    kirima_value_in value;
    uint32_t is_signed, four_state;
} kirima_typed_integer;
typedef struct kirima_pattern_enum {
    kirima_typed_integer integer;
    kirima_pattern_name name;
} kirima_pattern_enum;
typedef struct kirima_pattern_node {
    uint32_t kind, child_first, child_count, key_first, key_count;
    kirima_typed_integer integer;
    double real;
    kirima_bytes text;
    uint64_t ordinal;
    kirima_pattern_name tag;
    const kirima_pattern_name* names;
    const kirima_pattern_enum* enumerators;
    uint32_t name_count, enum_count;
} kirima_pattern_node;
enum kirima_pattern_status {
    KIRIMA_PATTERN_OK,
    KIRIMA_PATTERN_INVALID,
    KIRIMA_PATTERN_CYCLE,
    KIRIMA_PATTERN_RESOURCE
};
/* Array references are zero-based. Values form a borrowed graph during the call.
 * For static conversion supply one source specifier, or empty for a default.
 * On a language error the diagnostic sink receives the text. */
typedef struct kirima_text_api {
    uint32_t (*parse)(kirima_bytes, kirima_piece_sink, kirima_byte_sink diagnostic, void*,
                      uint32_t* valid);
    uint32_t (*scalar)(kirima_bytes, const kirima_format_arg*, const kirima_format_context*,
                       kirima_byte_sink result, kirima_byte_sink diagnostic, void*);
    uint32_t (*pattern)(const kirima_pattern_node*, uint32_t nodes, uint32_t root,
                        uint32_t positional, kirima_byte_sink result, void*,
                        uint32_t* pattern_status);
    uint32_t (*as_string)(kirima_bytes, kirima_byte_sink, void*);
    uint32_t (*packed_scan_format)(kirima_value_in, kirima_byte_sink, void*, uint32_t* known);
    uint32_t (*time_format_error)(const kirima_format_context*, kirima_byte_sink, void*);
    uint32_t (*distribution)(kirima_bytes, uint32_t* kind);
    uint32_t (*argument_name)(uint32_t bad_argument, kirima_byte_sink, void*);
} kirima_text_api;
#ifdef __cplusplus
}
#endif
