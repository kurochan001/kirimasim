// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/execution_abi.h"
#include "kirimasim/kernel/run_policy_abi.h"
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
int check_c_run_policy(const kirima_kernel_api* api, kirima_context* c, kirima_context* other) {
    const kirima_run_policy_api* p = api->query_service(KIRIMA_SERVICE_RUN_POLICY, 1);
    const kirima_scalar_task_api* s = api->query_service(KIRIMA_SERVICE_SCALAR_TASK, 1);
    if (!p || !s || api->query_service(KIRIMA_SERVICE_RUN_POLICY, 2)) return 0;
    const kirima_execution_api* e = api->query_service(KIRIMA_SERVICE_EXECUTION, 1);
    kirima_w4 word;
    const kirima_w4 negative_zero = {UINT64_C(1) << 63, 0}, zero = {0, 0}, x = {1, 1};
    uint32_t row, arity;
    const double args[2] = {3, 4};
    double number;
    struct result desc = {
        TEXT("multichannel descriptor 32'h00000001 is not a file descriptor, which input takes"), 0,
        0, 0};
    if (!e || api->query_service(KIRIMA_SERVICE_EXECUTION, 2) ||
        e->math_name(TEXT("$hypot"), &row, &arity) || arity != 2 ||
        e->math(row, args, 2, &number) || number != 5 ||
        e->math(row, args, 1, &number) != KIRIMA_VALUE_INVALID ||
        e->select_real(x, negative_zero, zero, 0, &word) || word.a != negative_zero.a ||
        e->select_real(x, (kirima_w4){UINT64_C(0x7ff8000000000001), 0},
                       (kirima_w4){UINT64_C(0x7ff8000000000001), 0}, 0, &word) ||
        word.a || e->logical(KIRIMA_IMPLIES, zero, x, &word) || word.a != 1 || word.b ||
        e->logical(KIRIMA_EQUIVALENT, x, x, &word) || word.b != 1 ||
        e->descriptor_error((kirima_w4){1, 0}, 1, bytes, &desc) || !desc.matched || desc.errors)
        return 0;
    kirima_resource run;
    kirima_position site = {1, 20, 7}, another = {1, 20, 8};
    uint32_t flag, action;
    uint64_t total, sites, a, b, selected;
    if (p->create(c, &run) || p->first_instruction(c, run, 71, &flag) || !flag ||
        p->first_instruction(c, run, 71, &flag) || flag || p->first_heap(c, run, site, &flag) ||
        !flag || p->first_heap(c, run, site, &flag) || flag ||
        p->occurrence(c, run, KIRIMA_CHECK_BOUNDS, site, &flag) || !flag ||
        p->occurrence(c, run, KIRIMA_CHECK_BOUNDS, site, &flag) || flag ||
        p->occurrence(c, run, KIRIMA_CHECK_BOUNDS, another, &flag) || !flag ||
        p->counts(c, run, KIRIMA_CHECK_BOUNDS, &total, &sites) || total != 3 || sites != 2 ||
        p->counts(c, run, KIRIMA_CHECK_CONDITION, &total, &sites) || total || sites ||
        p->first_heap(other, run, site, &flag) != KIRIMA_VALUE_INVALID)
        return 0;
    struct result r = {TEXT("pop from an empty queue"), 0, 0, 0};
    if (p->heap_diagnostic((kirima_heap_status){KIRIMA_HEAP_OK, KIRIMA_HEAP_EMPTY}, &action, bytes,
                           &r) ||
        action != KIRIMA_DIAG_WARN || r.matched != 1)
        return 0;
    r.expected = TEXT("null class handle access");
    if (p->object_diagnostic(KIRIMA_OBJ_NULL, &action, bytes, &r) || action != KIRIMA_DIAG_DESIGN ||
        r.matched != 2)
        return 0;
    r.expected = TEXT("case expression contains X/Z bits (--cond-x=error)");
    if (p->condition(TEXT("case"), 1, bytes, &r) || r.matched != 3) return 0;
    kirima_w4 words[2] = {{0}};
    kirima_value_out out = {words, 65, 2};
    uint64_t rng = 327, duplicate = rng;
    if (p->initialize(KIRIMA_INIT_X, 1, &rng, out) || words[0].b != UINT64_MAX || words[1].a != 1 ||
        words[1].b != 1 || rng != duplicate || p->initialize(KIRIMA_INIT_RANDOM, 0, &rng, out) ||
        words[0].a || words[0].b || words[1].a || rng != duplicate ||
        p->initialize(KIRIMA_INIT_RANDOM, 1, &rng, out) || rng == duplicate || words[1].a > 1 ||
        words[0].b || words[1].b)
        return 0;
    kirima_w4 again[2];
    if (p->initialize(KIRIMA_INIT_RANDOM, 1, &duplicate, (kirima_value_out){again, 65, 2}) ||
        duplicate != rng || memcmp(words, again, sizeof words))
        return 0;
    r.expected = TEXT("");
    if (p->replicate(c, run, TEXT(""), (kirima_index){INT64_MAX, 1, 0}, bytes, &r, &flag) || flag ||
        p->replicate(c, run, TEXT("a"), (kirima_index){0, 0, 0}, bytes, &r, &flag) || !flag ||
        p->replicate(c, run, TEXT("a"), (kirima_index){-1, 1, 0}, bytes, &r, &flag) || flag ||
        r.matched != 6)
        return 0;
    r.expected = TEXT("abab");
    if (p->replicate(c, run, TEXT("ab"), (kirima_index){2, 1, 0}, bytes, &r, &flag) || flag ||
        r.matched != 7 || s->intern(c, TEXT("a"), 0, &a) || s->intern(c, TEXT("b"), 0, &b) ||
        p->select_string(c, (kirima_w4){1, 1}, a, b, &selected) || selected ||
        p->select_string(c, (kirima_w4){0, 1}, b, b, &selected) || selected != b ||
        p->select_string(c, (kirima_w4){1, 0}, a, b, &selected) || selected != a ||
        p->write_memory(TEXT(""), TEXT("body"), &action) || action != KIRIMA_MEMORY_OPEN_ERROR ||
        p->write_memory(TEXT("a\0b"), TEXT(""), &action) != KIRIMA_VALUE_INVALID ||
        p->release(c, run) || p->release(c, run) != KIRIMA_VALUE_INVALID)
        return 0;
    return r.errors == 0;
}
