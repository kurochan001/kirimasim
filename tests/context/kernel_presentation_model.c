// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/presentation_abi.h"
#include <stddef.h>
#include <string.h>

struct result {
    char text[160];
    uint64_t size;
    uint32_t kind, calls, elements[8];
    kirima_w4 words[8];
};
static void take(void* user, kirima_bytes b) {
    struct result* r = user;
    ++r->calls;
    r->size = b.size;
    if (b.size && b.size <= sizeof(r->text)) memcpy(r->text, b.data, (size_t)b.size);
}
static void diagnostic(void* user, uint32_t kind, kirima_bytes b) {
    struct result* r = user;
    r->kind = kind;
    take(user, b);
}
static void element(void* user, uint32_t elem) {
    struct result* r = user;
    if (r->calls < 8) r->elements[r->calls] = elem;
    ++r->calls;
}
static void word(void* user, uint32_t elem, const kirima_w4* w, uint32_t n) {
    struct result* r = user;
    if (r->calls < 8 && n == 1) r->words[r->calls] = w[0];
    element(user, elem);
}
#define TEXT(s) ((kirima_bytes){(s), sizeof(s) - 1})
int check_c_presentation(const kirima_kernel_api* api) {
    const kirima_presentation_api* p = api->query_service(KIRIMA_SERVICE_PRESENTATION, 1);
    if (!p || api->query_service(KIRIMA_SERVICE_PRESENTATION, 0)) return 0;
    struct result r = {0};
    uint32_t mismatch = 2;
    kirima_format_context c = {TEXT("top"), TEXT("module"), TEXT("ns"), 1, -9, -9, 0, 0};
    kirima_format_arg args[4] = {0};
    const kirima_w4 wide[] = {{15, 0}, {0, 1}};
    args[0].kind = KIRIMA_FORMAT_STRING;
    args[0].text = TEXT("A\0B");
    args[1].kind = KIRIMA_FORMAT_PATTERN;
    args[1].has_pattern = 1;
    args[1].positional_pattern = TEXT("pos");
    args[2].kind = KIRIMA_FORMAT_INTEGRAL;
    args[2].four_state = 1;
    args[2].integral = (kirima_value_in){wide, 65, 2};
    args[3].kind = KIRIMA_FORMAT_REAL;
    args[3].real = 3.0;
    const char expected[] = "top module A\0B pos z000000000000000f 3ns";
    if (p->format(TEXT("%m %l %s %0p %0h %0t"), args, 4, &c, take, take, &r, &mismatch) ||
        mismatch || r.calls != 1 || r.size != sizeof(expected) - 1 ||
        memcmp(r.text, expected, sizeof(expected) - 1))
        return 0;
    r.calls = 0;
    if (p->format(TEXT("%s %h"), args, 1, &c, take, take, &r, &mismatch) || !mismatch ||
        r.calls != 1)
        return 0;
    args[2].four_state = 0;
    if (p->format(TEXT(""), args, 4, &c, take, take, &r, &mismatch) != KIRIMA_VALUE_INVALID ||
        r.calls != 1)
        return 0;

    kirima_activation_dim dims[] = {{INT64_MAX, INT64_MAX - 1}, {1, 0}};
    kirima_memory_range range = {dims, 2, 0, {0, 0, 0}, {0, 0, 0}};
    const kirima_w4 one = {1, 0}, two = {2, 0};
    kirima_value_in items[] = {{&one, 4, 1}, {&two, 4, 1}};
    memset(&r, 0, sizeof(r));
    if (p->load_memory(TEXT("1 2 3"), &range, 4, 1, 0, items, 2, word, diagnostic, &r) ||
        r.calls != 3 || r.elements[0] != 3 || r.elements[1] != 2 || r.words[0].a != 1 ||
        r.words[1].a != 2 || r.kind != KIRIMA_MEM_INPUT || !r.size)
        return 0;
    r.calls = 0;
    if (p->dump_order(&range, element, &r) || r.calls != 4 || r.elements[0] != 3 ||
        r.elements[3] != 0)
        return 0;
    r.calls = 0;
    if (p->load_memory(TEXT("1 2 3 4 5"), &range, 4, 1, 0, NULL, 0, word, diagnostic, &r) ||
        r.calls != 5 || r.kind != KIRIMA_MEM_COUNT)
        return 0;
    range.start = (kirima_index){INT64_MIN, 1, 0};
    range.finish = (kirima_index){INT64_MAX, 1, 0};
    r.calls = 0;
    if (p->load_memory(TEXT(""), &range, 4, 1, 0, NULL, 0, word, diagnostic, &r) || r.calls != 1 ||
        r.kind != KIRIMA_MEM_COUNT || r.size >= sizeof(r.text))
        return 0;
    r.text[r.size] = 0;
    if (!strstr(r.text, "36893488147419103232")) return 0;
    r.calls = 0;
    if (p->load_memory(TEXT("1"), &range, 4, 1, 0, NULL, 0, word, diagnostic, &r) || r.calls != 1 ||
        r.kind != KIRIMA_MEM_RANGE)
        return 0;
    dims[0].right = INT64_MIN;
    if (p->dump_order(&range, element, &r) != KIRIMA_VALUE_INVALID || r.calls != 1) return 0;
    return 1;
}
