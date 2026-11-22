// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/io_abi.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_PROCESS 4u
#define KIRIMA_PROCESS_VERSION 1u

typedef struct kirima_process {
    const kirima_context* owner;
    uint64_t serial;
    uint32_t index, reserved;
} kirima_process;
typedef struct kirima_program {
    const kirima_context* owner;
    uint32_t index, reserved;
} kirima_program;
enum kirima_process_status {
    KIRIMA_PROCESS_OK,
    KIRIMA_PROCESS_INVALID,
    KIRIMA_PROCESS_BUSY,
    KIRIMA_PROCESS_RESOURCE,
    KIRIMA_PROCESS_STOPPED
};
enum kirima_process_kind { KIRIMA_ACTIVE_PROCESS, KIRIMA_REACTIVE_PROCESS, KIRIMA_NET_RESOLVER };
enum kirima_process_action {
    KIRIMA_START_PROCESS,
    KIRIMA_CANCEL_WAIT,
    KIRIMA_KILL_PROCESS,
    KIRIMA_END_PROCESS,
    KIRIMA_RELEASE_PROCESS,
    KIRIMA_EXIT_PROGRAM,
    KIRIMA_STROBE
};
enum kirima_process_flags {
    KIRIMA_PROCESS_FINISHED = 1,
    KIRIMA_PENDING_REPORT = 2,
    KIRIMA_CALLBACK_ORIGIN = 4,
    KIRIMA_PROGRAM_STOPPED = 8,
    KIRIMA_REACTIVE_GROUP = 16
};
enum kirima_scheduler_flags {
    KIRIMA_ENDING = 1,
    KIRIMA_POSTPONED = 2,
    KIRIMA_COLLECT_EVENTS = 4,
    KIRIMA_COLLECT_TRANSIENTS = 8,
    KIRIMA_END_NOTIFICATIONS = 16,
    KIRIMA_MEMORY_RAN_OUT = 32
};
enum kirima_checkpoint { KIRIMA_TAKE_INTERRUPT, KIRIMA_TAKE_OVERFLOW };
enum kirima_termination { KIRIMA_DESIGN_ERROR, KIRIMA_RESOURCE_LIMIT, KIRIMA_OUT_OF_MEMORY };
enum kirima_process_text { KIRIMA_ERROR_TEXT, KIRIMA_END_NOTE };
enum kirima_notification { KIRIMA_ENDED_PROGRAM_PROCESS, KIRIMA_ENDED_FILE_MONITOR };
typedef struct kirima_scheduler_state {
    uint64_t time, callback_release_epoch;
    uint32_t flags, reason;
    int32_t level;
    uint32_t reserved;
} kirima_scheduler_state;
typedef struct kirima_process_api {
    uint32_t (*add)(kirima_context*, uint32_t kind, kirima_resume_fn, void* frame, kirima_process*);
    uint32_t (*add_program)(kirima_context*, kirima_program*);
    uint32_t (*add_initial)(kirima_context*, kirima_program, kirima_resume_fn, void* frame,
                            kirima_process*);
    uint32_t (*add_child)(kirima_context*, kirima_process parent, kirima_resume_fn, void* frame,
                          kirima_process*);
    uint32_t (*action)(kirima_context*, kirima_process, uint32_t op);
    uint32_t (*flags)(kirima_context*, kirima_process, uint32_t*);
    uint32_t (*defer_report)(kirima_context*, kirima_process, kirima_bytes line);
    uint32_t (*defer_call)(kirima_context*, kirima_process, uint32_t final_region,
                           kirima_deferred_fn, void* frame, uint32_t token, uint32_t assertion);
    uint32_t (*flush_assertions)(kirima_context*, kirima_process);
    uint32_t (*cancel_assertion)(kirima_context*, uint32_t assertion);
    uint32_t (*take_ended)(kirima_context*, uint32_t kind, uint32_t* present, kirima_process*);
    uint32_t (*state)(kirima_context*, kirima_scheduler_state*);
    uint32_t (*checkpoint)(kirima_context*, uint32_t kind, uint32_t* taken);
    uint32_t (*terminate)(kirima_context*, uint32_t kind);
    uint32_t (*set_end_note)(kirima_context*, kirima_bytes);
    uint32_t (*text)(kirima_context*, uint32_t kind, kirima_byte_sink, void*);
} kirima_process_api;
#ifdef __cplusplus
}
#endif
