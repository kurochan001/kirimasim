// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <stddef.h>
#include <string.h>

#include "kernel_process_model.h"
_Static_assert(sizeof(kirima_process) == 24 && offsetof(kirima_process, index) == 16,
               "process identity");

static void delivered(kirima_context* c, void* user, uint32_t token, int run) {
    process_fixture* f = user;
    uint32_t* signals = run ? &f->delivered : &f->dropped;
    kirima_scheduler_state state;
    if (token >= 6 || (*signals & (1u << token)) || f->api->state(c, &state)) {
        f->error = 1;
        return;
    }
    *signals |= 1u << token;
    if (run && ((state.flags & KIRIMA_POSTPONED) != 0) != (token == 2)) f->error = 2;
}
static kirima_status child(kirima_context* c, void* user) {
    process_fixture* f = user;
    uint32_t flags;
    if (f->api->flags(c, f->child, &flags) || !(flags & KIRIMA_REACTIVE_GROUP)) f->error = 3;
    ++f->child_calls;
    return KIRIMA_DONE;
}
static kirima_status parent(kirima_context* c, void* user) {
    process_fixture* f = user;
    const kirima_process_api* p = f->api;
    char line[] = "queued text";
    uint32_t flags = 0;
    ++f->calls;
    if (p->add_child(c, f->parent, child, f, &f->child) ||
        p->action(c, f->child, KIRIMA_START_PROCESS) ||
        p->defer_call(c, f->parent, 0, delivered, f, 3, 7) || p->cancel_assertion(c, 7) ||
        p->defer_call(c, f->parent, 1, delivered, f, 4, 8) || p->flush_assertions(c, f->parent) ||
        p->defer_report(c, f->parent, (kirima_bytes){line, sizeof(line) - 1}) ||
        p->defer_call(c, f->parent, 0, delivered, f, 1, 9) ||
        p->defer_call(c, f->parent, 1, delivered, f, 2, 10) || p->flags(c, f->parent, &flags) ||
        !(flags & KIRIMA_PENDING_REPORT))
        f->error = 4;
    memset(line, '?', sizeof(line) - 1);
    return KIRIMA_DONE;
}
static kirima_status end_self(kirima_context* c, void* user) {
    process_fixture* f = user;
    ++f->self_calls;
    if (f->api->action(c, f->self, KIRIMA_END_PROCESS) ||
        f->api->action(c, f->self, KIRIMA_RELEASE_PROCESS) != KIRIMA_PROCESS_BUSY)
        f->error = 5;
    return KIRIMA_DONE;
}
static void note(void* where, kirima_bytes text) {
    *(int*)where = text.size == 3 && !memcmp(text.data, "abc", 3);
}
int start_process_fixture(const kirima_kernel_api* kernel, kirima_context* c, kirima_context* other,
                          process_fixture* f) {
    const kirima_process_api* p =
        kernel->query_service(KIRIMA_SERVICE_PROCESS, KIRIMA_PROCESS_VERSION);
    f->api = p;
    kirima_scheduler_state state;
    kirima_process dummy;
    uint32_t taken = 3;
    int ok = 0;
    if (!p || kernel->query_service(KIRIMA_SERVICE_PROCESS, 9) ||
        p->add(c, KIRIMA_REACTIVE_PROCESS, parent, f, &f->parent) ||
        p->add(c, KIRIMA_ACTIVE_PROCESS, end_self, f, &f->self) ||
        p->add_child(other, f->parent, child, f, &dummy) != KIRIMA_PROCESS_INVALID ||
        p->action(c, f->parent, KIRIMA_RELEASE_PROCESS) != KIRIMA_PROCESS_BUSY ||
        p->state(c, &state) || !(state.flags & KIRIMA_COLLECT_EVENTS) || state.time ||
        p->checkpoint(c, KIRIMA_TAKE_INTERRUPT, &taken) || taken ||
        p->checkpoint(c, KIRIMA_TAKE_OVERFLOW, &taken) || taken ||
        p->set_end_note(c, (kirima_bytes){"abc", 3}) || p->text(c, KIRIMA_END_NOTE, note, &ok) ||
        !ok || p->action(c, f->self, KIRIMA_START_PROCESS) ||
        p->action(c, f->parent, KIRIMA_START_PROCESS))
        return 0;
    return 1;
}
int finish_process_fixture(kirima_context* c, process_fixture* f) {
    const kirima_process_api* p = f->api;
    kirima_process next;
    uint32_t flags = 0;
    if (f->error || f->calls != 1 || f->child_calls != 1 || f->self_calls != 1 ||
        f->delivered != 6 || f->dropped != 24 || p->flags(c, f->parent, &flags) ||
        !(flags & KIRIMA_PROCESS_FINISHED) || p->action(c, f->parent, KIRIMA_RELEASE_PROCESS) ||
        p->add(c, KIRIMA_NET_RESOLVER, child, f, &next) || next.index != f->parent.index ||
        next.serial == f->parent.serial ||
        p->action(c, f->parent, KIRIMA_KILL_PROCESS) != KIRIMA_PROCESS_INVALID ||
        p->action(c, next, KIRIMA_CANCEL_WAIT) || p->action(c, next, KIRIMA_KILL_PROCESS) ||
        p->action(c, next, KIRIMA_RELEASE_PROCESS))
        return 0;
    return 1;
}
static kirima_status program_exit(kirima_context* c, void* user) {
    process_fixture* f = user;
    uint32_t flags;
    if (f->api->add_child(c, f->parent, child, f, &f->child) ||
        f->api->action(c, f->parent, KIRIMA_EXIT_PROGRAM) || f->api->flags(c, f->child, &flags) ||
        !(flags & KIRIMA_PROGRAM_STOPPED))
        f->error = 6;
    return KIRIMA_DONE;
}
int start_program_fixture(const kirima_kernel_api* kernel, kirima_context* c, process_fixture* f) {
    f->api = kernel->query_service(KIRIMA_SERVICE_PROCESS, 1);
    kirima_program program;
    return f->api && !f->api->add_program(c, &program) &&
           !f->api->add_initial(c, program, program_exit, f, &f->parent) &&
           !f->api->action(c, f->parent, KIRIMA_START_PROCESS);
}
