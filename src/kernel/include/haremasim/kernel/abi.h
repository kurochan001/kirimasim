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
#define HS_ABI_MAJOR 0
#define HS_ABI_MINOR 1

/* A simulation context; the kernel's Context behind an opaque pointer. */
typedef struct hs_context hs_context;

/* What a resume function reports (RUNTIME_ABI.md 4.3). */
typedef enum hs_status {
    HS_WAITING = 0, /* registered a wait */
    HS_DONE = 1,    /* ran to its end */
    HS_KILLED = 2,  /* ended by kill or disable */
    HS_ERROR = 3,   /* failed; see hs_kernel_api.fail */
    HS_STOP = 4     /* saw the request to end */
} hs_status;

typedef hs_status (*hs_resume_fn)(hs_context* ctx, void* frame);
typedef int (*hs_guard_fn)(hs_context* ctx, void* frame);
typedef void (*hs_eval_fn)(hs_context* ctx, void* frame, uint64_t* out);

/* Edges: any change, posedge, negedge, either edge. */
enum { HS_EDGE_ANY = 0, HS_EDGE_POS = 1, HS_EDGE_NEG = 2, HS_EDGE_BOTH = 3 };
/* Thread groups (SCHEDULER_SPEC.md 9.5). */
enum { HS_GROUP_ACTIVE = 0, HS_GROUP_REACTIVE = 1 };
/* Storage domains (SIR_SPEC.md 3.2). */
enum { HS_DOMAIN_TWO = 0, HS_DOMAIN_FOUR = 1, HS_DOMAIN_REAL = 2, HS_DOMAIN_HANDLE = 3 };

/* A part of a slot: elements [first_elem, first_elem + num_elems) (0: all)
 * and bits [lsb, lsb + width) (0: all). */
typedef struct hs_slot_part {
    uint32_t slot;
    uint32_t first_elem;
    uint32_t num_elems;
    uint32_t lsb;
    uint32_t width;
} hs_slot_part;

/* One item of an event control on a slot part. */
typedef struct hs_wait_item {
    hs_slot_part part;
    uint32_t edge;
    hs_guard_fn guard; /* NULL: no iff */
    void* guard_frame;
} hs_wait_item;

/* A monitor record: an expression evaluated again when its slots change
 * or its events are triggered. */
typedef struct hs_monitor {
    const hs_slot_part* slots;
    uint32_t num_slots;
    const uint32_t* events;
    uint32_t num_events;
    hs_eval_fn eval;
    void* frame;
    uint32_t domain;
    uint32_t width;
    uint32_t edge;
    uint32_t until_true; /* nonzero for wait (expr) */
} hs_monitor;

/* The kernel's function table. Fields are only ever added at the end
 * (a minor version step); anything else is a major step. */
typedef struct hs_kernel_api {
    uint32_t abi_major;
    uint32_t abi_minor;

    /* Processes. */
    uint32_t (*add_process)(hs_context* ctx, hs_resume_fn fn, void* frame, uint32_t group);
    void (*start)(hs_context* ctx, uint32_t process);

    /* Waits: called by a running process just before it returns
     * HS_WAITING. */
    void (*wait_delay)(hs_context* ctx, uint32_t process, uint64_t ticks);
    void (*wait_event)(hs_context* ctx, uint32_t process, const hs_wait_item* items,
                       uint32_t num_items);
    void (*wait_monitor)(hs_context* ctx, uint32_t process, const hs_monitor* monitor);
    void (*wait_named)(hs_context* ctx, uint32_t process, uint32_t event);

    /* Writes, NBAs and events. Values are laid out like a slot element. */
    int (*write)(hs_context* ctx, uint32_t slot, uint32_t elem, uint32_t lsb, uint32_t width,
                 const uint64_t* value);
    void (*schedule_nba)(hs_context* ctx, uint32_t process, uint32_t slot, uint32_t elem,
                         uint32_t lsb, uint32_t width, const uint64_t* value, uint64_t delay);
    void (*trigger)(hs_context* ctx, uint32_t event);
    void (*trigger_nb)(hs_context* ctx, uint32_t process, uint32_t event, uint64_t delay);
    int (*triggered)(hs_context* ctx, uint32_t event);

    /* Time and ending. */
    uint64_t (*time)(hs_context* ctx);
    void (*request_end)(hs_context* ctx, uint32_t reason, int32_t level);
    int (*ending)(hs_context* ctx);
    void (*fail)(hs_context* ctx, const char* message);

    /* Direct reads: the first word of element `elem` of a slot. */
    uint64_t* (*slot_words)(hs_context* ctx, uint32_t slot, uint32_t elem);
} hs_kernel_api;

/* The kernel's table. */
const hs_kernel_api* hs_get_kernel_api(void);

#ifdef __cplusplus
}
#endif
