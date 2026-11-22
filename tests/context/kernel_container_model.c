// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/reference_abi.h"
#include <stddef.h>
static uint64_t fresh(void* raw) { return ++*(uint64_t*)raw; }
static uint32_t reject_mark(void* raw, uint64_t h) {
    *(uint64_t*)raw = h;
    return 0;
}

int check_c_container(const kirima_kernel_api* api, kirima_context* c, kirima_context* other,
                      kirima_heap_token* result, uint32_t* result_type) {
    const kirima_container_api* h = api->query_service(KIRIMA_SERVICE_CONTAINER, 1);
    if (!h || api->query_service(KIRIMA_SERVICE_CONTAINER, 2)) return 0;
    kirima_heap_word w = {255, {255, 255}, 1, 0, UINT32_MAX, UINT32_MAX}, back;
    kirima_heap_status s = {0};
    uint32_t layout, type, array_type, decl;
    const kirima_heap_token empty = {0};
    kirima_heap_token value = {0}, held, copied, array;
    const kirima_index zero = {0, 1, 0}, one = {1, 1, 0}, two = {2, 1, 0}, unknown = {0, 0, 0};
    uint64_t size = 0, seed = 0;
    if (h->add_layout(c, &w, 1, &layout, &s) || s.error || h->word_count(c, layout, &size) ||
        size != 1 || h->describe_word(c, layout, 0, &back, &s) || s.error || back.mask != 255 ||
        back.initial.b != 255 ||
        h->add_type(c, (kirima_heap_type){KIRIMA_QUEUE, layout, 2}, &type, &s) || s.error ||
        h->add_type(c, (kirima_heap_type){KIRIMA_DYNAMIC_ARRAY, layout, 0}, &array_type, &s) ||
        s.error || h->construct(c, array_type, two, empty, fresh, &seed, &array, &s) || s.error ||
        seed != 2 || h->copy(c, type, array, &value, &s) || s.error ||
        h->actual_type(c, value, &decl, &s) || s.error || decl != type ||
        h->cell(c, type, value, one, &held, &s) || s.error || held.kind != KIRIMA_CELL)
        return 0;
    kirima_w4 v = {248, 0}, out = {0, 0};
    const kirima_words_in input = {&v, 1};
    const kirima_words_out output = {&out, 1};
    if (h->read(c, type, value, unknown, output, &s) || s.error || out.b != 255 ||
        h->write_cell(other, layout, held, input, &s) != KIRIMA_VALUE_INVALID ||
        h->read_cell(c, layout, value, output, &s) != KIRIMA_VALUE_INVALID ||
        h->swap_values(c, type, value, zero, one, &s) || s.error ||
        h->read_cell(c, layout, held, output, &s) || s.error || out.a != 1 ||
        h->write_cell(c, layout, held, input, &s) || s.error ||
        h->snapshot(c, value, &copied, &s) || s.error || copied.bits == value.bits ||
        h->pop(c, type, &value, 0, output, &s) || s.error || out.a != 248 ||
        h->push(c, type, &value, 1, input, &s) || s.error ||
        h->read_cell(c, layout, held, output, &s) || s.error || out.a != 248)
        return 0;
    kirima_heap_token roots[] = {value, held};
    uint32_t reachable = 2;
    if (h->reaches(c, roots, 1, held, &reachable, &s) || s.error || reachable ||
        h->reaches(c, roots, 2, held, &reachable, &s) || s.error || !reachable ||
        h->collect(c, roots, 2, NULL, NULL, &s) || s.error || h->size(c, type, copied, &size, &s) ||
        s.error != KIRIMA_HEAP_HANDLE || h->read_cell(c, layout, held, output, &s) || s.error ||
        out.a != 248)
        return 0;
    // Failure to publish a root or mark a live string cannot sweep the heap.
    kirima_heap_word text = {UINT64_MAX, {0, 0}, 0, 1, UINT32_MAX, UINT32_MAX};
    uint32_t string_layout, string_type;
    kirima_w4 token = {7, 0};
    kirima_heap_token strings;
    uint64_t marked = 0;
    if (h->add_layout(c, &text, 1, &string_layout, &s) || s.error ||
        h->add_type(c, (kirima_heap_type){KIRIMA_DYNAMIC_ARRAY, string_layout, 0}, &string_type,
                    &s) ||
        s.error || h->from_words(c, string_type, (kirima_words_in){&token, 1}, &strings, &s) ||
        s.error || h->collect(c, &strings, 1, reject_mark, &marked, &s) || !s.error ||
        marked != 7 || h->read_cell(c, layout, held, output, &s) || s.error ||
        h->collect(c, &value, 1, NULL, NULL, &s) || s.error ||
        h->read_cell(c, layout, held, output, &s) || s.error != KIRIMA_HEAP_HANDLE)
        return 0;
    kirima_heap_stats stats;
    if (h->stats(c, &stats) || stats.containers != 1 || stats.cells != 2) return 0;
    const kirima_reference_api* refs = api->query_service(KIRIMA_SERVICE_REFERENCE, 1);
    kirima_activation_field field = {KIRIMA_FIELD_CONTAINER, 64, UINT32_MAX, 0, NULL};
    kirima_activation_layout lexical;
    kirima_reference lease, binding;
    kirima_w4 container_word = {value.bits, 0};
    if (!refs || refs->add_layout(c, &field, 1, &lexical) || refs->create(c, lexical, &lease) ||
        refs->bind(c, lease, 0, (kirima_ref_window){0, 1, 0, 0}, 0, &binding) ||
        refs->write(c, binding, 0, (kirima_value_in){&container_word, 64, 1}) ||
        h->collect(c, NULL, 0, NULL, NULL, &s) || s.error || h->size(c, type, value, &size, &s) ||
        s.error || size != 2 || refs->release(c, binding) || refs->release(c, lease))
        return 0;
    *result = value;
    *result_type = type;
    return 1;
}
