// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/event_abi.h"
#include "kirimasim/kernel/object_abi.h"
#include <stddef.h>

int check_c_objects(const kirima_kernel_api* api, kirima_context* c, kirima_context* other) {
    const kirima_object_api* o = api->query_service(KIRIMA_SERVICE_OBJECT, 1);
    const kirima_container_api* h = api->query_service(KIRIMA_SERVICE_CONTAINER, 1);
    if (!o || !h || api->query_service(KIRIMA_SERVICE_OBJECT, 2)) return 0;
    const kirima_heap_word words[] = {{UINT64_MAX, {0, 0}, 1, 0, UINT32_MAX, UINT32_MAX},
                                      {1, {0, 0}, 1, 0, UINT32_MAX, UINT32_MAX}};
    kirima_heap_status hs;
    uint32_t layout;
    if (h->add_layout(c, words, 2, &layout, &hs) || hs.error) return 0;
    const kirima_class_field fields[] = {{layout, 0, 0}, {layout, 1, 1}};
    const uint32_t methods[] = {17};
    kirima_class_descriptor classes[2] = {0};
    classes[0].name = (kirima_bytes){"C", 1};
    classes[0].base = UINT32_MAX;
    classes[0].weak_target = UINT32_MAX;
    classes[0].fields = fields;
    classes[0].field_count = 2;
    classes[0].virtual_methods = methods;
    classes[0].method_count = 1;
    classes[1].name = (kirima_bytes){"W", 1};
    classes[1].base = UINT32_MAX;
    classes[1].weak_target = 0;
    uint64_t rng = 3, id = 0;
    kirima_pinned_object obj, copy, weak, locked;
    kirima_object_info info;
    uint32_t method = 0, yes = 0;
    kirima_resource stage, field, low, bit, readonly, constant;
    const kirima_resource absent = {0};
    const kirima_w4 initial[] = {{7, 0}, {1, 0}}, input = {1, 0};
    kirima_w4 output = {0, 0}, typed;
    kirima_heap_token decoded;
    if (o->add_classes(c, classes, 2) || o->create(c, 0, &rng, &obj) || !obj.pin.serial ||
        o->info(c, obj.value, &info) || info.dynamic_class != 0 || !info.identity ||
        o->identity(c, obj.value, &id) || id != info.identity ||
        o->virtual_method(c, obj.value, 0, 0, &method) || method != 17 ||
        o->to_word(c, obj.value, &typed) || o->from_word(c, typed, 0, &decoded) ||
        decoded.bits != obj.value.bits || o->info(other, obj.value, &info) != KIRIMA_OBJ_ABI ||
        o->begin_stage(c, obj.value, 0, &stage) || o->bind(c, obj.value, 0, 1, 0, &constant) ||
        o->write(c, constant, (kirima_words_in){initial, 2}, stage) || o->begin_body(c, stage) ||
        o->finish_stage(c, stage) || o->begin_body(c, stage) != KIRIMA_OBJ_ABI ||
        o->write(c, constant, (kirima_words_in){initial, 2}, absent) != KIRIMA_OBJ_CONST_WRITE ||
        o->bind(c, obj.value, 0, 0, 0, &field) || o->select_words(c, field, 0, 1, &low) ||
        o->select_bits(c, low, 7, 1, &bit) ||
        o->write(c, bit, (kirima_words_in){&input, 1}, absent) ||
        o->read(c, low, (kirima_words_out){&output, 1}) || output.a != 128 ||
        o->bind(c, obj.value, 0, 0, 1, &readonly) ||
        o->write(c, readonly, (kirima_words_in){initial, 2}, absent) != KIRIMA_OBJ_CONST_WRITE ||
        o->copy(c, obj.value, &copy) || o->equal(c, obj.value, copy.value, &yes) || yes ||
        o->release(c, copy.pin) || o->new_weak(c, 1, obj.value, &rng, &weak) ||
        o->release(c, obj.pin) || o->release(c, obj.pin) != KIRIMA_OBJ_ABI ||
        h->collect(c, NULL, 0, NULL, NULL, &hs) || hs.error ||
        o->lock_weak(c, weak.value, &locked) || locked.value.bits != obj.value.bits ||
        o->release(c, locked.pin))
        return 0;
    kirima_resource views[] = {constant, field, low, bit, readonly};
    for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); ++i)
        if (o->release(c, views[i])) return 0;
    if (h->collect(c, NULL, 0, NULL, NULL, &hs) || hs.error ||
        o->lock_weak(c, weak.value, &locked) || locked.value.bits || locked.pin.serial ||
        o->truth(c, obj.value, &yes) != KIRIMA_OBJ_TOKEN || o->release(c, weak.pin))
        return 0;
    return 1;
}

typedef struct model_events {
    const kirima_event_api* e;
    kirima_process proc;
    kirima_event_value value;
    uint32_t resumes, guards, failed;
} model_events;
static int enabled(kirima_context* c, void* raw) {
    (void)c;
    ++((model_events*)raw)->guards;
    return 1;
}
static kirima_status resume_event(kirima_context* c, void* raw) {
    model_events* m = raw;
    ++m->resumes;
    if (m->resumes == 1) {
        kirima_pinned_event e, retained;
        kirima_event_stats stats;
        if (m->e->create(c, &e) || m->e->retain(c, e.value, &retained)) {
            m->failed = 1;
            return KIRIMA_DONE;
        }
        m->value = e.value;
        const kirima_event_wait arm = {e.value, enabled, m, 1, 0};
        if (m->e->wait_many(c, m->proc, &arm, 1) || m->e->trigger_nb(c, m->proc, e.value, 4, 0) ||
            m->e->release(c, e.pin) || m->e->release(c, retained.pin) ||
            m->e->collect(c, NULL, 0) || m->e->stats(c, &stats) || stats.live != 1)
            m->failed = 2;
        return KIRIMA_WAITING;
    }
    uint32_t triggered = 0;
    if (m->e->triggered(c, m->value, &triggered) || !triggered || m->guards != 1) m->failed = 3;
    return KIRIMA_DONE;
}
int check_c_events(const kirima_kernel_api* api, kirima_context* c, kirima_context* other,
                   void (*run)(kirima_context*)) {
    const kirima_event_api* e = api->query_service(KIRIMA_SERVICE_EVENT, 1);
    const kirima_process_api* p = api->query_service(KIRIMA_SERVICE_PROCESS, 1);
    if (!e || !p || api->query_service(KIRIMA_SERVICE_EVENT, 2)) return 0;
    model_events m = {0};
    m.e = e;
    kirima_event_value permanent;
    uint32_t row;
    if (e->add_permanent(c, &permanent) || e->resolve(c, permanent, &row) ||
        e->trigger(other, permanent) != KIRIMA_EVENT_ABI ||
        p->add(c, KIRIMA_ACTIVE_PROCESS, resume_event, &m, &m.proc) ||
        p->action(c, m.proc, KIRIMA_START_PROCESS))
        return 0;
    run(c);
    kirima_event_stats stats;
    kirima_pinned_event next;
    if (m.resumes != 2 || m.failed || e->collect(c, &permanent, 1) || e->stats(c, &stats) ||
        stats.live || !stats.reusable || e->create(c, &next) || next.value.bits == m.value.bits ||
        e->resolve(c, m.value, &row) != KIRIMA_EVENT_TOKEN || e->release(c, next.pin) ||
        e->release(c, next.pin) != KIRIMA_EVENT_ABI)
        return 0;
    return 1;
}
