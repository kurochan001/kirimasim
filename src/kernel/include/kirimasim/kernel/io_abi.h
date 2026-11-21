// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

/* Service 2, version 1. SYSTEM_TASKS 2, 6 and RUNTIME_ABI 4.2. */
#pragma once
#include "kirimasim/kernel/abi.h"
#include "kirimasim/kernel/value_abi.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_IO 2u
#define KIRIMA_IO_VERSION 1u
#define KIRIMA_MCD_FILES 30u

typedef struct kirima_bytes {
    const char* data;
    uint64_t size;
} kirima_bytes;
typedef struct kirima_file_row {
    uint32_t row, reserved;
    uint64_t serial;
} kirima_file_row;
enum kirima_target_flags { KIRIMA_FILE_OUT = 1, KIRIMA_FILE_ERR = 2, KIRIMA_FILE_NOT_OPEN = 4 };
/* Caller owns the copy. Preserve it unchanged while its originating Context lives. */
typedef struct kirima_file_targets {
    const kirima_context* owner;
    uint32_t flags, count;
    kirima_file_row files[KIRIMA_MCD_FILES];
} kirima_file_targets;
typedef struct kirima_io_result {
    int64_t value;
    uint32_t not_open, bad_mode;
} kirima_io_result;
enum kirima_scan_domain {
    KIRIMA_SCAN_TWO,
    KIRIMA_SCAN_FOUR,
    KIRIMA_SCAN_REAL,
    KIRIMA_SCAN_SHORTREAL,
    KIRIMA_SCAN_STRING
};
/* Integral width > 0; real 64, shortreal 32, string 0. */
typedef struct kirima_scan_target {
    uint32_t domain, width;
} kirima_scan_target;
typedef struct kirima_scan_result {
    int32_t assigned;
    uint32_t not_open, missing_targets, bad_format;
} kirima_scan_result;
typedef void (*kirima_byte_sink)(void* user, kirima_bytes bytes);
typedef void (*kirima_scan_sink)(void* user, uint32_t index, const kirima_w4* words,
                                 uint32_t count);
typedef struct kirima_scan_args {
    kirima_bytes format;
    const kirima_scan_target* targets;
    uint32_t count;
    kirima_scan_sink assign;
    kirima_byte_sink diagnostic;
    void* user;
} kirima_scan_args;
enum kirima_output_kind { KIRIMA_OUTPUT_WRITE, KIRIMA_OUTPUT_ERROR, KIRIMA_OUTPUT_MESSAGE };

/* Return KIRIMA_VALUE_OK, INVALID or RESOURCE. Language-level results use the
 * out parameter, including a closed/unknown descriptor or EOF. Sinks may not
 * throw or reenter the Context. Their views expire on return. NULL input data
 * is valid only with zero length. No entry retains model storage. */
typedef struct kirima_io_api {
    uint32_t (*open_mcd)(kirima_context*, kirima_bytes path, kirima_io_result*);
    uint32_t (*open_fd)(kirima_context*, kirima_bytes path, kirima_bytes mode, kirima_io_result*);
    uint32_t (*capture)(kirima_context*, kirima_w4 descriptor, kirima_file_targets*);
    uint32_t (*write)(kirima_context*, const kirima_file_targets*, kirima_bytes);
    uint32_t (*live)(kirima_context*, const kirima_file_targets*, uint32_t*);
    uint32_t (*close)(kirima_context*, kirima_w4, kirima_io_result*);
    uint32_t (*flush)(kirima_context*, kirima_w4, kirima_io_result*);
    uint32_t (*flush_all)(kirima_context*);
    uint32_t (*get_char)(kirima_context*, kirima_w4, kirima_io_result*);
    uint32_t (*unget_char)(kirima_context*, kirima_w4, int32_t byte, kirima_io_result*);
    uint32_t (*read_line)(kirima_context*, kirima_w4, uint64_t max, kirima_byte_sink, void*,
                          kirima_io_result*);
    uint32_t (*read_bytes)(kirima_context*, kirima_w4, uint64_t max, kirima_byte_sink, void*,
                           kirima_io_result*);
    uint32_t (*tell)(kirima_context*, kirima_w4, kirima_io_result*);
    uint32_t (*seek)(kirima_context*, kirima_w4, int64_t offset, int64_t origin, kirima_io_result*);
    uint32_t (*at_end)(kirima_context*, kirima_w4, kirima_io_result*);
    uint32_t (*error)(kirima_context*, kirima_w4, kirima_byte_sink, void*, kirima_io_result*);
    uint32_t (*scan_file)(kirima_context*, kirima_w4, const kirima_scan_args*, kirima_scan_result*);
    uint32_t (*scan_string)(kirima_context*, kirima_bytes, const kirima_scan_args*,
                            kirima_scan_result*);
    uint32_t (*output)(kirima_context*, uint32_t kind, kirima_bytes);
    uint32_t (*flush_output)(kirima_context*);
    uint32_t (*at_line_start)(kirima_context*, uint32_t*);
} kirima_io_api;
#ifdef __cplusplus
}
#endif
