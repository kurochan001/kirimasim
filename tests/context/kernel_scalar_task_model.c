// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <stddef.h>
#include <string.h>

#include "kirimasim/kernel/scalar_task_abi.h"
struct copy_result {
    char data[16];
    uint64_t size;
};
static void take(void* user, kirima_bytes bytes) {
    struct copy_result* out = user;
    out->size = bytes.size;
    if (bytes.size && bytes.size <= sizeof(out->data))
        memcpy(out->data, bytes.data, (size_t)bytes.size);
}
int check_c_scalar_tasks(const kirima_kernel_api* kernel, kirima_context* c) {
    const kirima_scalar_task_api* s = kernel->query_service(KIRIMA_SERVICE_SCALAR_TASK, 1);
    if (!s || kernel->query_service(KIRIMA_SERVICE_SCALAR_TASK, 2)) return 0;
    const kirima_bytes source = {"A\0bc", 4};
    uint64_t h = 0, sub = 0, upper = 0, other = 0, number = 0;
    kirima_string_stats stats;
    struct copy_result text = {0};
    kirima_index i = {1, 1, 0}, j = {2, 1, 0}, unknown = {0, 0, 0};
    uint32_t b = 0;
    int32_t comparison = 99;
    double real = 0;
    if (s->intern(c, source, 1, &h) || !h || s->get(c, h, take, &text) || text.size != 3 ||
        memcmp(text.data, "Abc", 3) || s->byte_at(c, h, i, &b) || b != 'b' ||
        s->byte_at(c, h, unknown, &b) || b || s->substring(c, h, i, j, &sub) ||
        s->change_case(c, sub, 1, &upper) || s->compare(c, sub, upper, 1, &comparison) ||
        comparison || s->put_byte(c, upper, i, 'x', &other) || s->get(c, other, take, &text) ||
        text.size != 2 || memcmp(text.data, "Bx", 2) ||
        s->intern(c, (kirima_bytes){"10", 2}, 0, &number) || s->to_integer(c, number, 16, &b) ||
        b != 16 || s->to_real(c, number, &real) || real != 10 ||
        s->to_integer(c, number, 1, &b) != KIRIMA_VALUE_INVALID)
        return 0;
    if (s->begin_collection(c, &h, 1) || s->mark(c, UINT64_MAX) != KIRIMA_VALUE_INVALID ||
        s->end_collection(c, 1) != KIRIMA_VALUE_INVALID || s->get(c, upper, take, &text) ||
        text.size != 2 || s->begin_collection(c, &h, 1) || s->mark(c, number) ||
        s->end_collection(c, 1) || s->stats(c, &stats) || stats.live != 2 ||
        s->get(c, upper, take, &text) || text.size ||
        s->end_collection(c, 1) != KIRIMA_VALUE_INVALID)
        return 0;
    return 1;
}
