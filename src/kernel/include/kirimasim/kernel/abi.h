/* The C boundary between the kernel and code built separately from it
 * (tier 2, later tier 3): the kernel's function table (RUNTIME_ABI.md 4.2)
 * and the resume-function form (4.3).
 *
 * Only C types cross this boundary: no classes, templates, exceptions or
 * RTTI. The table holds the waits, writes, NBAs, events and ending of
 * SCHEDULER_SPEC.md; system tasks, objects and reports come later. Tier 1
 * (vm) is built together with the kernel and calls the C++ API directly;
 * both paths reach the same implementation.
 *
 * This header is part of the kernel's public interface and must stay
 * compilable as C++20. Stage M4 of P1 declares the table; its fields are
 * settled with the tier-2 work of P2 (RUNTIME_ABI.md 8). */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ABI version (RUNTIME_ABI.md 2). 0.x while the table is a draft. */
#define KIRIMA_ABI_MAJOR 0
#define KIRIMA_ABI_MINOR 1

/* A simulation context; the kernel's Context behind an opaque pointer. */
typedef struct kirima_context kirima_context;

/* What a resume function reports (RUNTIME_ABI.md 4.3). */
typedef enum kirima_status {
    KIRIMA_WAITING = 0, /* registered a wait */
    KIRIMA_DONE = 1,    /* ran to its end */
    KIRIMA_KILLED = 2,  /* ended by kill or disable */
    KIRIMA_ERROR = 3,   /* failed; see kirima_kernel_api.fail */
    KIRIMA_STOP = 4     /* saw the request to end */
} kirima_status;

typedef kirima_status (*kirima_resume_fn)(kirima_context* ctx, void* frame);
typedef int (*kirima_guard_fn)(kirima_context* ctx, void* frame);
typedef void (*kirima_eval_fn)(kirima_context* ctx, void* frame, uint64_t* out);
/* A deferred assertion's call (SCHEDULER_SPEC.md 15.2): run it (run
 * nonzero) or drop it (run 0); either way the token is used up. */
typedef void (*kirima_deferred_fn)(kirima_context* ctx, void* frame, uint32_t token, int run);

/* Edges: any change, posedge, negedge, either edge. */
enum { KIRIMA_EDGE_ANY = 0, KIRIMA_EDGE_POS = 1, KIRIMA_EDGE_NEG = 2, KIRIMA_EDGE_BOTH = 3 };
/* Thread groups (SCHEDULER_SPEC.md 9.5). */
enum { KIRIMA_GROUP_ACTIVE = 0, KIRIMA_GROUP_REACTIVE = 1 };
/* Storage domains (SIR_SPEC.md 3.2). */
enum {
    KIRIMA_DOMAIN_TWO = 0,
    KIRIMA_DOMAIN_FOUR = 1,
    KIRIMA_DOMAIN_REAL = 2,
    KIRIMA_DOMAIN_HANDLE = 3
};

/* A part of a slot: elements [first_elem, first_elem + num_elems) (0: all)
 * and bits [lsb, lsb + width) (0: all). */
typedef struct kirima_slot_part {
    uint32_t slot;
    uint32_t first_elem;
    uint32_t num_elems;
    uint32_t lsb;
    uint32_t width;
} kirima_slot_part;

/* One item of an event control on a slot part. */
typedef struct kirima_wait_item {
    kirima_slot_part part;
    uint32_t edge;
    kirima_guard_fn guard; /* NULL: no iff */
    void* guard_frame;
} kirima_wait_item;

/* A monitor record: an expression evaluated again when its slots change
 * or its events are triggered. */
typedef struct kirima_monitor {
    const kirima_slot_part* slots;
    uint32_t num_slots;
    const uint32_t* events;
    uint32_t num_events;
    kirima_eval_fn eval;
    void* frame;
    uint32_t domain;
    uint32_t width;
    uint32_t edge;
    uint32_t until_true; /* nonzero for wait (expr) */
} kirima_monitor;

/* The kernel's function table. Fields are only ever added at the end
 * (a minor version step); anything else is a major step. */
typedef struct kirima_kernel_api {
    uint32_t abi_major;
    uint32_t abi_minor;

    /* Processes. */
    uint32_t (*add_process)(kirima_context* ctx, kirima_resume_fn fn, void* frame, uint32_t group);
    void (*start)(kirima_context* ctx, uint32_t process);

    /* Waits: called by a running process just before it returns
     * KIRIMA_WAITING. */
    void (*wait_delay)(kirima_context* ctx, uint32_t process, uint64_t ticks);
    void (*wait_event)(kirima_context* ctx, uint32_t process, const kirima_wait_item* items,
                       uint32_t num_items);
    void (*wait_monitor)(kirima_context* ctx, uint32_t process, const kirima_monitor* monitor);
    void (*wait_named)(kirima_context* ctx, uint32_t process, uint32_t event);

    /* Writes, NBAs and events. Values are laid out like a slot element. */
    int (*write)(kirima_context* ctx, uint32_t slot, uint32_t elem, uint32_t lsb, uint32_t width,
                 const uint64_t* value);
    void (*schedule_nba)(kirima_context* ctx, uint32_t process, uint32_t slot, uint32_t elem,
                         uint32_t lsb, uint32_t width, const uint64_t* value, uint64_t delay);
    void (*trigger)(kirima_context* ctx, uint32_t event);
    void (*trigger_nb)(kirima_context* ctx, uint32_t process, uint32_t event, uint64_t delay);
    int (*triggered)(kirima_context* ctx, uint32_t event);

    /* Time and ending. */
    uint64_t (*time)(kirima_context* ctx);
    void (*request_end)(kirima_context* ctx, uint32_t reason, int32_t level);
    int (*ending)(kirima_context* ctx);
    void (*fail)(kirima_context* ctx, const char* message);

    /* Direct reads: the first word of element `elem` of a slot. */
    uint64_t* (*slot_words)(kirima_context* ctx, uint32_t slot, uint32_t elem);
} kirima_kernel_api;

/* The kernel's table. */
const kirima_kernel_api* kirima_get_kernel_api(void);

#ifdef __cplusplus
}
#endif
