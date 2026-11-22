// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/io_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_TASK 11u
#define KIRIMA_TASK_VERSION 1u
enum kirima_dump_scope_kind {
    KIRIMA_DUMP_MODULE,
    KIRIMA_DUMP_BEGIN,
    KIRIMA_DUMP_FORK,
    KIRIMA_DUMP_TASK,
    KIRIMA_DUMP_FUNCTION
};
enum kirima_dump_var_type {
    KIRIMA_DUMP_REG,
    KIRIMA_DUMP_INTEGER,
    KIRIMA_DUMP_TIME,
    KIRIMA_DUMP_REAL,
    KIRIMA_DUMP_REALTIME,
    KIRIMA_DUMP_EVENT,
    KIRIMA_DUMP_WIRE,
    KIRIMA_DUMP_TRI,
    KIRIMA_DUMP_WAND,
    KIRIMA_DUMP_TRIAND,
    KIRIMA_DUMP_WOR,
    KIRIMA_DUMP_TRIOR,
    KIRIMA_DUMP_TRI0,
    KIRIMA_DUMP_TRI1,
    KIRIMA_DUMP_SUPPLY0,
    KIRIMA_DUMP_SUPPLY1
};
typedef struct kirima_dump_scope {
    kirima_bytes name, path;
    uint32_t kind, parent;
} kirima_dump_scope;
typedef struct kirima_dump_var {
    kirima_bytes name, range;
    uint32_t scope, type, width, slot, short_real;
} kirima_dump_var;
typedef struct kirima_wave_setup {
    const kirima_dump_scope* scopes;
    const kirima_dump_var* vars;
    const kirima_bytes* filters;
    uint32_t scope_count, var_count, filter_count, glitches;
    kirima_bytes date, version, timescale, filename;
} kirima_wave_setup;
enum kirima_dump_disposition { KIRIMA_DUMP_DONE, KIRIMA_DUMP_LATE, KIRIMA_DUMP_COMMAND_LINE };
enum kirima_dump_control {
    KIRIMA_DUMP_OFF,
    KIRIMA_DUMP_ON,
    KIRIMA_DUMP_ALL,
    KIRIMA_DUMP_FLUSH,
    KIRIMA_DUMP_FINISH
};
enum kirima_read_result { KIRIMA_READ_OK, KIRIMA_READ_OPEN_ERROR, KIRIMA_READ_IO_ERROR };
typedef struct kirima_task_api {
    uint32_t (*wave_setup)(kirima_context*, const kirima_wave_setup*);
    uint32_t (*dump_file)(kirima_context*, kirima_bytes name, kirima_bytes call,
                          uint32_t* disposition);
    uint32_t (*dump_vars)(kirima_context*, uint32_t levels, const uint32_t* scopes,
                          uint32_t scope_count, const uint32_t* vars, uint32_t var_count,
                          uint32_t all, uint32_t* disposition);
    uint32_t (*dump_control)(kirima_context*, uint32_t control);
    uint32_t (*dump_limit)(kirima_context*, uint64_t bytes);
    uint32_t (*display)(kirima_context*, kirima_bytes, uint32_t newline);
    uint32_t (*read_file)(kirima_bytes path, kirima_byte_sink, void*, uint32_t* result);
    uint32_t (*test_plusargs)(const kirima_bytes* args, uint32_t count, kirima_bytes prefix,
                              uint32_t* found);
    /* assign is called only on a match; an invalid SV format calls diagnostic instead. */
    uint32_t (*value_plusargs)(kirima_context*, const kirima_bytes* args, uint32_t count,
                               kirima_bytes format, kirima_scan_target, kirima_scan_sink assign,
                               kirima_byte_sink diagnostic, void*, uint32_t* found);
} kirima_task_api;
#ifdef __cplusplus
}
#endif
