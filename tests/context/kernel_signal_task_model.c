// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/signal_abi.h"
#include "kirimasim/kernel/task_abi.h"
#include <stddef.h>
#include <string.h>

#define TEXT(s) ((kirima_bytes){(s), sizeof(s) - 1})
struct observation {
    uint64_t low, high;
    uint32_t count, errors;
};
static void scan(void* user, uint32_t i, const kirima_w4* words, uint32_t count) {
    struct observation* r = user;
    if (i || count != 2) {
        ++r->errors;
        return;
    }
    r->low = words[0].a;
    r->high = words[1].a;
    ++r->count;
}
static void unexpected(void* user, kirima_bytes b) {
    (void)b;
    ++((struct observation*)user)->errors;
}
static void text(void* user, kirima_bytes b) {
    struct observation* r = user;
    if (b.size < 20 || b.size > UINT32_MAX) {
        ++r->errors;
        return;
    }
    const char end[] = "$enddefinitions";
    for (size_t i = 0; i + sizeof(end) - 1 <= b.size; ++i) {
        if (memcmp(b.data + i, end, sizeof(end) - 1) == 0) {
            ++r->count;
            return;
        }
    }
    ++r->errors;
}
static uint32_t pending(void* user, uint32_t slot, const uint64_t* value, uint64_t count) {
    struct observation* o = user;
    if (slot == 0 && count && value[0] == 9) ++o->count;
    return 1;
}
static kirima_status done(kirima_context* c, void* frame) {
    (void)c;
    (void)frame;
    return KIRIMA_DONE;
}
int check_c_signals_tasks(const kirima_kernel_api* api, kirima_context* c, kirima_context* other,
                          kirima_bytes wave, void (*run)(kirima_context*)) {
    const kirima_signal_api* s = api->query_service(KIRIMA_SERVICE_SIGNAL, 1);
    const kirima_task_api* t = api->query_service(KIRIMA_SERVICE_TASK, 1);
    const kirima_process_api* p = api->query_service(KIRIMA_SERVICE_PROCESS, 1);
    const kirima_event_api* e = api->query_service(KIRIMA_SERVICE_EVENT, 1);
    if (!s || !t || !e || !p || api->query_service(KIRIMA_SERVICE_SIGNAL, 3) ||
        api->query_service(KIRIMA_SERVICE_TASK, 2))
        return 0;
    kirima_dump_scope scope = {TEXT("top"), TEXT("top"), KIRIMA_DUMP_MODULE, UINT32_MAX};
    kirima_dump_var var = {TEXT("a"), TEXT("[7:0]"), 0, KIRIMA_DUMP_REG, 8, 0, 0};
    kirima_wave_setup setup = {&scope,        &var,          NULL,         1,   1, 0, 0,
                               TEXT("today"), TEXT("model"), TEXT("1 ns"), wave};
    uint32_t disposition = 0, found = 0;
    const kirima_bytes args[] = {TEXT("V=10000000000000003"), TEXT("V=2"), TEXT("A")};
    struct observation o = {0};
    if (t->wave_setup(c, &setup) || t->wave_setup(c, &setup) != KIRIMA_VALUE_INVALID ||
        t->dump_file(c, TEXT("unused"), TEXT("$dumpfile"), &disposition) ||
        disposition != KIRIMA_DUMP_COMMAND_LINE ||
        t->dump_vars(c, 0, NULL, 0, NULL, 0, 1, &disposition) ||
        disposition != KIRIMA_DUMP_COMMAND_LINE || t->test_plusargs(args, 3, TEXT("A"), &found) ||
        !found ||
        t->value_plusargs(c, args, 3, TEXT("V=%h"), (kirima_scan_target){KIRIMA_SCAN_FOUR, 65},
                          scan, unexpected, &o, &found) ||
        !found || o.errors || o.count != 1 || o.low != 3 || o.high != 1 ||
        t->display(c, TEXT("a\0b"), 1))
        return 0;
    kirima_process proc;
    const kirima_slot_bits target = {0, 0, 0, 0};
    const kirima_slot_part whole = {0, 0, 0, 0, 0};
    uint64_t words[] = {5, 0};
    kirima_domain_value val = {words, 2};
    uint32_t changed = 0;
    if (p->add(c, KIRIMA_ACTIVE_PROCESS, done, NULL, &proc) ||
        s->hold(c, KIRIMA_HOLD_FORCE, proc, whole) != KIRIMA_VALUE_INVALID ||
        s->note_driven(c, 0) || s->mark_held(c, 0, 1, 1, 0) || s->write(c, target, val, &changed) ||
        !changed || s->hold(c, KIRIMA_HOLD_FORCE, proc, whole) ||
        s->holds_any(c, KIRIMA_HOLD_FORCE, proc, &found) || !found ||
        s->write_held(c, KIRIMA_HOLD_FORCE, proc, target, val) ||
        s->release(c, KIRIMA_HOLD_FORCE, whole) ||
        s->holds_any(c, KIRIMA_HOLD_FORCE, proc, &found) || found)
        return 0;
    const kirima_side_update side = {{1, 0, 0, 1}, 1};
    kirima_signal_driver driver;
    words[0] = 7;
    if (s->add_driver(c, &driver) ||
        s->drive_delayed(other, driver, target, val, 2, 2, 2, &side, 1) != KIRIMA_VALUE_INVALID ||
        s->drive_delayed(c, driver, target, val, 2, 2, 2, &side, 1))
        return 0;
    words[0] = 9;
    if (s->nba(c, proc, target, val, 3) ||
        s->nba(c, proc, (kirima_slot_bits){0, 0, 7, 2}, val, 3) != KIRIMA_VALUE_INVALID)
        return 0;
    o.count = 0;
    uint32_t completed = 0;
    if (s->visit_pending(c, pending, &o, &completed) || !completed || !o.count) return 0;
    words[0] = 0;
    run(c);
    if (t->dump_control(c, KIRIMA_DUMP_FINISH)) return 0;
    uint32_t file = KIRIMA_READ_OPEN_ERROR;
    o.count = 0;
    if (t->read_file(wave, text, &o, &file) || file != KIRIMA_READ_OK || o.errors || !o.count)
        return 0;
    return 1;
}
