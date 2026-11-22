// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/place_abi.h"
#include <stddef.h>
struct frame {
    kirima_w4 first[4], second[4];
    uint32_t pins, alive, moved, visits, safe;
};
static uint32_t retain(void* f, kirima_frame_identity id) {
    struct frame* a = f;
    if (!a->alive || id.row != 32 || id.generation != UINT64_C(0x100000003)) return 0;
    ++a->pins;
    return 1;
}
static void release(void* f, kirima_frame_identity id) {
    (void)id;
    --((struct frame*)f)->pins;
}
static uint32_t resolve(void* f, kirima_frame_identity id, kirima_words_out* out) {
    struct frame* a = f;
    if (!a->alive || !a->pins || id.generation != UINT64_C(0x100000003)) return 0;
    *out = (kirima_words_out){a->moved ? a->second : a->first, 4};
    return 1;
}
static uint32_t visit(void* f, const kirima_frame_provider* p, kirima_frame_identity id) {
    if (p->frame != f || id.row != 32) return 0;
    ++((struct frame*)f)->visits;
    return 1;
}
static void safe(void* f) { ++((struct frame*)f)->safe; }
static kirima_status finish(kirima_context* c, void* raw) {
    (void)c;
    (void)raw;
    return KIRIMA_DONE;
}
int check_c_places(const kirima_kernel_api* api, kirima_context* c, kirima_context* other,
                   void (*collect)(kirima_context*), void (*run)(kirima_context*)) {
    const kirima_place_api* p = api->query_service(KIRIMA_SERVICE_PLACE, 1);
    const kirima_container_api* h = api->query_service(KIRIMA_SERVICE_CONTAINER, 1);
    const kirima_reference_api* r = api->query_service(KIRIMA_SERVICE_REFERENCE, 1);
    if (!p || !h || !r || api->query_service(KIRIMA_SERVICE_PLACE, 2)) return 0;
    struct frame f = {0};
    f.alive = 1;
    kirima_frame_provider provider = {c, &f, retain, release, resolve};
    kirima_frame_identity identity = {32, UINT64_C(0x100000003)};
    kirima_place_rights rights = {0, 1, UINT32_MAX};
    kirima_resource whole, part, ro, slot;
    kirima_w4 raw = {2, 1}, out = {0, 0};
    kirima_value_in in = {&raw, 2, 1};
    kirima_value_out result = {&out, 2, 1};
    const kirima_resource empty = {0};
    kirima_place_info info;
    uint32_t completed;
    if (p->ordinary(c, &provider, identity, 0, 2, 65, rights, &whole) || f.pins != 1 ||
        p->select(c, whole, (kirima_ref_window){1, 1, 63, 2}, 0, &part) || f.pins != 2 ||
        p->select(c, part, (kirima_ref_window){0, 1, 0, 2}, 1, &ro) ||
        p->describe(c, part, &info) || info.window.first_element != 1 || info.window.lsb != 63 ||
        !info.local || p->visit_frames(c, visit, &f, &completed) || !completed || f.visits != 3 ||
        p->write(c, part, 0, in, empty) || f.first[3].a != 1 ||
        f.first[2].b != (UINT64_C(1) << 63) ||
        p->write(c, ro, 0, in, empty) != KIRIMA_REF_READONLY ||
        p->read(other, part, 0, result) != KIRIMA_REF_INVALID)
        return 0;
    f.moved = 1;
    f.second[2].a = UINT64_C(1) << 63;
    if (p->read(c, part, 0, result) || out.a != 1 || out.b) return 0;
    f.alive = 0;
    if (p->read(c, part, 0, result) != KIRIMA_REF_INVALID || p->release(c, whole) ||
        p->release(c, part) || p->release(c, ro) || f.pins)
        return 0;
    rights.local = 0;
    if (p->slot(c, 0, rights, &slot) ||
        p->select(c, slot, (kirima_ref_window){1, 1, 63, 2}, 0, &part) ||
        p->write(c, part, 0, in, empty) || p->read(c, part, 0, result) || out.a != 2 ||
        out.b != 1 || p->release(c, slot) || p->release(c, part))
        return 0;
    kirima_activation_field field = {KIRIMA_FIELD_FOUR, 65, UINT32_MAX, 0, NULL};
    kirima_activation_layout layout;
    kirima_reference lease, view;
    kirima_lexical_region region;
    if (r->add_layout(c, &field, 1, &layout) || r->create(c, layout, &lease) ||
        r->bind(c, lease, 0, (kirima_ref_window){0, 1, 63, 2}, 0, &view) ||
        p->lexical_region(c, view, &region) || !region.extent || region.window.lsb != 63 ||
        p->lexical(c, view, rights, &part) || r->release(c, view) || r->release(c, lease) ||
        p->write(c, part, 0, in, empty) || p->read(c, part, 0, result) || out.a != 2 ||
        out.b != 1 || p->release(c, part))
        return 0;
    kirima_heap_word word = {255, {0, 0}, 1, 0, UINT32_MAX, UINT32_MAX};
    kirima_heap_token container, cell;
    kirima_heap_status hs;
    uint32_t physical, type;
    kirima_w4 data = {19, 0};
    kirima_words_in ws = {&data, 1};
    kirima_value_out byte = {&out, 8, 1};
    if (h->add_layout(c, &word, 1, &physical, &hs) || hs.error ||
        h->add_type(c, (kirima_heap_type){KIRIMA_QUEUE, physical, 3}, &type, &hs) || hs.error ||
        h->from_words(c, type, ws, &container, &hs) || hs.error ||
        h->cell(c, type, container, (kirima_index){0, 1, 0}, &cell, &hs) || hs.error)
        return 0;
    rights.actual_type = type;
    if (p->cell(c, cell, physical, 5, rights, &part) != KIRIMA_REF_BOUNDS ||
        p->cell(c, cell, physical, 32, rights, &part) != KIRIMA_REF_BOUNDS ||
        p->cell(c, cell, physical, 8, rights, &part) || p->describe(c, part, &info) ||
        info.actual_type != type)
        return 0;
    collect(c);
    if (p->read(c, part, 0, byte) || out.a != 19 || p->release(c, part)) return 0;
    collect(c);
    if (p->cell(c, cell, physical, 8, rights, &part) != KIRIMA_REF_INVALID) return 0;
    struct frame nested = {0};
    kirima_run_hooks outer_hooks = {c, &f, safe, NULL}, inner_hooks = {c, &nested, safe, NULL};
    kirima_resource first, second;
    if (p->attach_hooks(c, &outer_hooks, &first) || p->attach_hooks(c, &inner_hooks, &second) ||
        p->detach_hooks(c, first) != KIRIMA_REF_INVALID)
        return 0;
    uint32_t process = api->add_process(c, finish, NULL, KIRIMA_GROUP_ACTIVE);
    api->start(c, process);
    run(c);
    if (!nested.safe || f.safe || p->detach_hooks(c, second)) return 0;
    process = api->add_process(c, finish, NULL, KIRIMA_GROUP_ACTIVE);
    api->start(c, process);
    run(c);
    return f.safe && !p->detach_hooks(c, first) && p->detach_hooks(c, first) == KIRIMA_REF_INVALID;
}
