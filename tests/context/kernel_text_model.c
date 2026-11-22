// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/text_abi.h"
#include <stddef.h>
#include <string.h>
#define TEXT(s) ((kirima_bytes){s, sizeof(s) - 1})
struct result {
    kirima_bytes expected;
    uint32_t matched, errors, pieces;
};
static void bytes(void* raw, kirima_bytes v) {
    struct result* x = raw;
    if (v.size == x->expected.size && (!v.size || !memcmp(v.data, x->expected.data, v.size)))
        ++x->matched;
    else
        ++x->errors;
}
static void error(void* raw, kirima_bytes v) {
    (void)v;
    ++((struct result*)raw)->errors;
}
static void piece(void* raw, const kirima_format_piece* p) {
    struct result* x = raw;
    if (p->specifier && (p->conversion != 'f' || p->real_format.size != 5)) ++x->errors;
    ++x->pieces;
}
int check_c_text(const kirima_kernel_api* api) {
    const kirima_text_api* t = api->query_service(KIRIMA_SERVICE_TEXT, 1);
    if (!t || api->query_service(KIRIMA_SERVICE_TEXT, 2)) return 0;
    kirima_format_context c = {0};
    c.precision = -12;
    c.unit = -9;
    c.decimals = 2;
    c.min_width = 0;
    c.suffix = TEXT(" ns");
    kirima_format_arg a = {0};
    a.kind = KIRIMA_FORMAT_REAL;
    a.real = 1250;
    struct result r = {TEXT("1.25 ns"), 0, 0, 0};
    uint32_t valid = 0;
    if (t->parse(TEXT("x:%3.1f"), piece, error, &r, &valid) || !valid || r.pieces != 2 ||
        t->scalar(TEXT("%0t"), &a, &c, bytes, error, &r) || r.matched != 1 || r.errors)
        return 0;
    kirima_pattern_node nodes[2] = {0};
    nodes[0].kind = KIRIMA_PATTERN_ARRAY;
    nodes[0].child_first = 1;
    nodes[0].child_count = 1;
    nodes[1].kind = KIRIMA_PATTERN_STRING;
    nodes[1].text = TEXT("p");
    r.expected = TEXT("'{\"p\"}");
    uint32_t rc = 99;
    if (t->pattern(nodes, 2, 0, 0, bytes, &r, &rc) || rc != KIRIMA_PATTERN_OK || r.matched != 2 ||
        r.errors)
        return 0;
    nodes[0].child_first = 0;
    if (t->pattern(nodes, 2, 0, 0, bytes, &r, &rc) || rc != KIRIMA_PATTERN_CYCLE ||
        r.matched != 2 || r.errors)
        return 0;
    r.expected = TEXT("ab");
    if (t->as_string(TEXT("a\0b"), bytes, &r) || r.matched != 3 || r.errors) return 0;
    kirima_w4 scan = {0x2568, 0};
    r.expected = TEXT("%h");
    if (t->packed_scan_format((kirima_value_in){&scan, 16, 1}, bytes, &r, &valid) || !valid ||
        r.matched != 4 || r.errors)
        return 0;
    scan.b = 1;
    if (t->packed_scan_format((kirima_value_in){&scan, 16, 1}, bytes, &r, &valid) || valid ||
        r.matched != 4)
        return 0;
    r.expected = TEXT("the mean");
    return t->distribution(TEXT("$dist_normal"), &rc) == 0 && rc == KIRIMA_DIST_NORMAL &&
           t->argument_name(KIRIMA_DIST_MEAN, bytes, &r) == 0 && r.matched == 5 && !r.errors;
}
