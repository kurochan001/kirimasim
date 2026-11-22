// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/reference_abi.h"
#include "kirimasim/kernel/signal_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_OBSERVATION 12u
#define KIRIMA_OBSERVATION_VERSION 1u
enum kirima_observation_status {
    KIRIMA_OBSERVE_OK,
    KIRIMA_OBSERVE_INVALID,
    KIRIMA_OBSERVE_TOKEN,
    KIRIMA_OBSERVE_PHASE,
    KIRIMA_OBSERVE_RESOURCE
};
enum kirima_watch_flag {
    KIRIMA_WATCH_UNTIL = 1u,
    KIRIMA_WATCH_EVENTS = 2u,
    KIRIMA_WATCH_HEAP = 4u,
    KIRIMA_WATCH_AUTOMATIC = 8u,
    KIRIMA_WATCH_STRING = 16u
};
/* The immutable filter and its frame are borrowed until registration ends.
 * Return an ABI heap error, and set keep to zero or one on success. */
typedef struct kirima_heap_filter {
    const kirima_context* owner;
    uint32_t (*test)(void*, kirima_heap_token, uint32_t* keep);
    void* frame;
} kirima_heap_filter;
typedef struct kirima_implicit_container {
    uint32_t slot, type;
} kirima_implicit_container;
typedef struct kirima_watch {
    const kirima_slot_part* slots;
    const kirima_event_value* events;
    const kirima_reference* capture_extents;
    const kirima_reference* static_parts;
    uint32_t slot_count, event_count, extent_count, part_count;
    uint32_t flags, domain, width, edge;
    kirima_eval_fn eval;
    void* frame;
    kirima_guard_fn guard;
    void* guard_frame;
    const kirima_heap_filter* filter;
    const kirima_implicit_container* implicit_container;
} kirima_watch;
enum kirima_observer_kind { KIRIMA_OBSERVE_SLOT, KIRIMA_OBSERVE_EVENT, KIRIMA_OBSERVE_EXPRESSION };
typedef struct kirima_observer {
    uint32_t kind, reserved;
    union {
        kirima_wait_item slot;
        kirima_event_wait event;
        const kirima_watch* watch;
    } item;
} kirima_observer;
typedef struct kirima_nba_write {
    kirima_slot_bits target;
    kirima_domain_value value;
    kirima_index index;
    uint32_t character, byte;
} kirima_nba_write;
typedef struct kirima_nba_repeat {
    kirima_value_in value;
    uint32_t is_signed, four_state;
} kirima_nba_repeat;
enum kirima_count_action { KIRIMA_COUNT_INVALID, KIRIMA_COUNT_READY, KIRIMA_COUNT_BIND };
typedef struct kirima_nba_ticket {
    const kirima_context* owner;
    uint64_t serial;
    uint32_t row, kind;
} kirima_nba_ticket;
/* Callback state must remain valid through its final invocation. A successful
 * immediate registration returns the all-zero (already completed) control. */
typedef struct kirima_cleanup {
    void (*call)(void*);
    void* frame;
} kirima_cleanup;
typedef struct kirima_observation_api {
    uint32_t (*prepare)(kirima_context*, const kirima_wait_item*, uint32_t, kirima_resource*);
    uint32_t (*wait_prepared)(kirima_context*, kirima_process, kirima_resource);
    uint32_t (*release_prepared)(kirima_context*, kirima_resource);
    uint32_t (*wait_mixed)(kirima_context*, kirima_process, const kirima_observer*, uint32_t);
    uint32_t (*capture)(kirima_context*, kirima_process, const kirima_nba_write*, uint32_t,
                        kirima_nba_ticket*);
    uint32_t (*classify)(kirima_nba_repeat, uint32_t* action);
    uint32_t (*ready)(kirima_context*, kirima_nba_ticket);
    uint32_t (*discard)(kirima_context*, kirima_nba_ticket);
    uint32_t (*bind)(kirima_context*, kirima_process, kirima_nba_ticket, kirima_nba_repeat,
                     const kirima_observer*, uint32_t, kirima_cleanup*, kirima_nba_ticket*);
    uint32_t (*cancel_all)(kirima_context*);
    uint32_t (*display_monitor)(kirima_context*, kirima_process, const kirima_slot_part*, uint32_t,
                                const kirima_file_targets*, uint32_t heap, uint32_t strength,
                                const kirima_heap_filter*);
    uint32_t (*file_strobe)(kirima_context*, kirima_process, const kirima_file_targets*);
    uint32_t (*detach_display)(kirima_context*, kirima_process);
    uint32_t (*monitoring)(kirima_context*, uint32_t on);
    uint32_t (*flush_changes)(kirima_context*);
    uint32_t (*monitor_limit)(kirima_context*, uint32_t* limited, kirima_process*);
    uint32_t (*visit_strings)(kirima_context*, kirima_string_mark, void*, uint32_t* completed);
} kirima_observation_api;
#ifdef __cplusplus
}
#endif
