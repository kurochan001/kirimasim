// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/observation_abi.h"
#include <stddef.h>
struct state {
    uint32_t cleaned, resumed, evaluated;
    const kirima_kernel_api* api;
};
static void cleanup(void* raw) { ++((struct state*)raw)->cleaned; }
static kirima_status resume(kirima_context* ctx, void* raw) {
    (void)ctx;
    ++((struct state*)raw)->resumed;
    return KIRIMA_DONE;
}
static void evaluate(kirima_context* c, void* raw, uint64_t* out) {
    struct state* s = raw;
    ++s->evaluated;
    *out = *s->api->slot_words(c, 1, 0);
}
int check_c_observation(const kirima_kernel_api* api, kirima_context* c, kirima_context* other,
                        void (*run)(kirima_context*)) {
    const kirima_observation_api* o = api->query_service(KIRIMA_SERVICE_OBSERVATION, 1);
    const kirima_process_api* p = api->query_service(KIRIMA_SERVICE_PROCESS, 1);
    const kirima_event_api* e = api->query_service(KIRIMA_SERVICE_EVENT, 1);
    if (!o || !p || !e || api->query_service(KIRIMA_SERVICE_OBSERVATION, 2)) return 0;
    struct state s = {0, 0, 0, api};
    kirima_process origin, waiter, prepared_waiter;
    kirima_resource prepared;
    kirima_wait_item arm = {{1, 0, 0, 0, 0}, KIRIMA_EDGE_POS, NULL, NULL};
    if (p->add(c, KIRIMA_ACTIVE_PROCESS, resume, &s, &origin) ||
        p->add(c, KIRIMA_ACTIVE_PROCESS, resume, &s, &waiter) ||
        p->add(c, KIRIMA_ACTIVE_PROCESS, resume, &s, &prepared_waiter) ||
        o->prepare(c, &arm, 1, &prepared) ||
        o->wait_prepared(other, prepared_waiter, prepared) != KIRIMA_OBSERVE_TOKEN ||
        o->wait_prepared(c, prepared_waiter, prepared) || o->release_prepared(c, prepared) ||
        o->release_prepared(c, prepared) != KIRIMA_OBSERVE_TOKEN)
        return 0;
    kirima_pinned_event live;
    if (e->create(c, &live)) return 0;
    kirima_watch watch = {0};
    watch.slots = &arm.part;
    watch.slot_count = 1;
    watch.domain = KIRIMA_DOMAIN_TWO;
    watch.width = 1;
    watch.eval = evaluate;
    watch.frame = &s;
    kirima_observer mixed[2] = {0};
    mixed[0].kind = KIRIMA_OBSERVE_EXPRESSION;
    mixed[0].item.watch = &watch;
    mixed[1].kind = KIRIMA_OBSERVE_EVENT;
    mixed[1].item.event.value = live.value;
    mixed[1].item.event.flush_point = 1;
    if (o->wait_mixed(c, waiter, mixed, 2) || s.evaluated != 1) return 0;
    uint64_t v[2] = {41, 0};
    kirima_nba_write w = {0};
    w.value = (kirima_domain_value){v, 2};
    kirima_nba_ticket payload, control, immediate;
    kirima_w4 count[2] = {{2, 0}, {0, 0}};
    kirima_nba_repeat repeat = {{count, 65, 2}, 0, 1};
    kirima_cleanup release = {cleanup, &s};
    uint32_t action = 0;
    if (o->capture(c, origin, &w, 1, &payload) || o->classify(repeat, &action) ||
        action != KIRIMA_COUNT_BIND ||
        o->bind(other, origin, payload, repeat, mixed, 2, &release, &control) ==
            KIRIMA_OBSERVE_OK ||
        s.cleaned || o->bind(c, origin, payload, repeat, mixed + 1, 1, &release, &control) ||
        !control.serial || o->ready(c, control) != KIRIMA_OBSERVE_TOKEN)
        return 0;
    v[0] = 14;
    if (o->capture(c, origin, &w, 1, &payload)) return 0;
    count[1].b = 1;
    if (o->classify(repeat, &action) || action != KIRIMA_COUNT_READY ||
        o->bind(c, origin, payload, repeat, NULL, 0, &release, &immediate) || immediate.serial ||
        s.cleaned != 1)
        return 0;
    count[1].b = 0;
    if (e->trigger(c, live.value) || s.cleaned != 1 || e->trigger(c, live.value) ||
        s.cleaned != 2 || e->release(c, live.pin) || o->discard(c, control) != KIRIMA_OBSERVE_TOKEN)
        return 0;
    uint64_t one = 1;
    api->write(c, 1, 0, 0, 0, &one);
    run(c);
    // The first issued payload wins the first position even though it became ready last.
    return s.resumed == 2 && *api->slot_words(c, 0, 0) == 14 && s.cleaned == 2;
}
