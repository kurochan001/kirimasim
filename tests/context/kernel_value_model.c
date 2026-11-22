// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <stddef.h>
#include <string.h>

#include "kernel_abi_model.h"
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/value_abi.h"

_Static_assert(sizeof(kirima_model_layout) == 40 && sizeof(kirima_model_field) == 40,
               "model layout");
_Static_assert(sizeof(kirima_model_body) == 56 && sizeof(kirima_module_table) == 80,
               "model descriptor");
_Static_assert(sizeof(kirima_model_activation) == 32, "model activation");

_Static_assert(sizeof(kirima_w4) == 16 && offsetof(kirima_w4, b) == 8, "four-state cell");
_Static_assert(sizeof(kirima_value_in) == 16 && offsetof(kirima_value_in, count) == 12, "span");
_Static_assert(offsetof(kirima_kernel_api, query_service) == 168, "0.4 prefix");

int check_c_value_model(const kirima_kernel_api* api) {
    if (!api || api->abi_minor < 5 || api->query_service(0, 1) ||
        api->query_service(KIRIMA_SERVICE_VALUE, 2))
        return 0;
    const kirima_value_api* v = api->query_service(KIRIMA_SERVICE_VALUE, KIRIMA_VALUE_VERSION);
    if (!v) return 0;
    kirima_w4 cells[2] = {{0, 0}, {0, 0}};
    const kirima_w4 operands[2] = {{UINT64_MAX, 0}, {0, 0}};
    const kirima_w4 other[2] = {{1, 0}, {0, 0}};
    const kirima_value_in x = {operands, 65, 2}, y = {other, 65, 2};
    const kirima_value_out dst = {cells, 65, 2};
    if (v->binary(KIRIMA_ADD, dst, x, y, 0) || cells[0].a || cells[1].a != 1 || cells[1].b)
        return 0;
    if (v->fill_x(dst) || cells[0].a != UINT64_MAX || cells[0].b != UINT64_MAX || cells[1].a != 1 ||
        cells[1].b != 1)
        return 0;
    uint32_t known = 1;
    int64_t pos = 9;
    if (v->position(&pos, &known, (kirima_value_in){cells, 65, 2}, 0) || known || pos) return 0;
    if (v->from_text(dst, "ABC", 3)) return 0;
    const kirima_value_in result = {cells, 65, 2};
    char text[3] = {'z', 'z', 'z'};
    uint64_t size = 0;
    if (v->to_text(NULL, 0, &size, result) || size != 3 ||
        v->to_text(text, 2, &size, result) != KIRIMA_VALUE_CAPACITY || text[0] != 'z' ||
        v->to_text(text, 3, &size, result) || memcmp(text, "ABC", 3))
        return 0;
    return v->binary(KIRIMA_ADD, dst, (kirima_value_in){other, 65, 1}, y, 0) ==
               KIRIMA_VALUE_INVALID &&
           v->binary(UINT32_MAX, dst, x, y, 0) == KIRIMA_VALUE_INVALID;
}
