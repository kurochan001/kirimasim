// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/process_abi.h"
#include "kirimasim/kernel/resource_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_EVENT 9u
#define KIRIMA_EVENT_VERSION 1u
enum kirima_event_status {
    KIRIMA_EVENT_OK,
    KIRIMA_EVENT_NULL,
    KIRIMA_EVENT_TOKEN,
    KIRIMA_EVENT_BACKEND,
    KIRIMA_EVENT_BUSY,
    KIRIMA_EVENT_RESOURCE,
    KIRIMA_EVENT_ABI
};
typedef struct kirima_event_value {
    const kirima_context* owner;
    uint64_t bits;
} kirima_event_value;
typedef struct kirima_pinned_event {
    kirima_event_value value;
    kirima_resource pin;
} kirima_pinned_event;
typedef struct kirima_event_wait {
    kirima_event_value value;
    kirima_guard_fn guard;
    void* frame;
    uint32_t flush_point, reserved;
} kirima_event_wait;
typedef struct kirima_event_stats {
    uint64_t live, allocated, retired;
    uint32_t reusable, eligible;
} kirima_event_stats;
/* A zero handle represents null; otherwise keep owner and bits intact.
 * Pins retain managed generations. Join waits use flush_point=0.
 * Trigger delay is normalized simulation ticks; overflow=1 retains the
 * kernel's distinction between a missing time and a null target. */
typedef struct kirima_event_api {
    uint32_t (*add_permanent)(kirima_context*, kirima_event_value*);
    uint32_t (*create)(kirima_context*, kirima_pinned_event*);
    uint32_t (*retain)(kirima_context*, kirima_event_value, kirima_pinned_event*);
    uint32_t (*release)(kirima_context*, kirima_resource);
    uint32_t (*resolve)(kirima_context*, kirima_event_value, uint32_t* event);
    uint32_t (*trigger)(kirima_context*, kirima_event_value);
    uint32_t (*trigger_nb)(kirima_context*, kirima_process, kirima_event_value, uint64_t ticks,
                           uint32_t overflow);
    uint32_t (*triggered)(kirima_context*, kirima_event_value, uint32_t*);
    uint32_t (*wait_named)(kirima_context*, kirima_process, kirima_event_value,
                           uint32_t flush_point);
    uint32_t (*wait_many)(kirima_context*, kirima_process, const kirima_event_wait*,
                          uint32_t count);
    uint32_t (*collect)(kirima_context*, const kirima_event_value* roots, uint32_t count);
    uint32_t (*stats)(kirima_context*, kirima_event_stats*);
} kirima_event_api;
#ifdef __cplusplus
}
#endif
