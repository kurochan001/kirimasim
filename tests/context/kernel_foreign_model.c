// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/foreign_abi.h"
#include "vpi_user.h"
#include <string.h>
#define TEXT(s) ((kirima_bytes){s, sizeof(s) - 1})
struct capture {
    kirima_foreign_type result;
    uint32_t list, reads, writes, value, answer;
};
static void routine(void* frame, uint32_t row, kirima_bytes name, kirima_foreign_type result,
                    uint32_t task) {
    struct capture* c = frame;
    if (!row && !task && name.size == 10 && !memcmp(name.data, "$model_vpi", 10)) {
        c->result = result;
        ++c->list;
    }
}
static void answer(void* frame, kirima_foreign_type type, kirima_words_in words) {
    if (type.width == 32 && words.count == 1 && !words.data[0].b)
        ((struct capture*)frame)->answer = (uint32_t)words.data[0].a;
}
static uint32_t read_val(void* frame, kirima_foreign_access a, kirima_words_out out) {
    struct capture* c = frame;
    if (a.first != 65 || a.second != 2 || a.generation != 7 || a.width != 8 || a.root_width != 8 ||
        out.count != 1)
        return 0;
    out.data[0] = (kirima_w4){c->value, 0};
    ++c->reads;
    return 1;
}
static uint32_t write_val(void* frame, kirima_foreign_access a, kirima_words_in in) {
    struct capture* c = frame;
    if (a.generation != 7 || a.width != 8 || in.count != 1 || in.data[0].b) return 0;
    c->value = (uint32_t)in.data[0].a;
    ++c->writes;
    return 1;
}
int check_c_foreign(const kirima_kernel_api* api, kirima_context* c, kirima_context* other) {
    const kirima_foreign_api* v = api->query_service(KIRIMA_SERVICE_FOREIGN, 1);
    if (!v || api->query_service(KIRIMA_SERVICE_FOREIGN, 2)) return 0;
    struct capture state = {0};
    state.value = 21;
    uint32_t enabled, stopped, accepted;
    if (v->state(c, &enabled, &stopped) || !enabled || stopped || v->routines(c, routine, &state) ||
        state.list != 1)
        return 0;
    kirima_foreign_object module = {0};
    module.kind = vpiModule;
    module.name = TEXT("top");
    module.unit = -9;
    module.precision = -12;
    if (v->object(c, &module)) return 0;
    kirima_foreign_site site = {0};
    site.scope = TEXT("top");
    site.file = TEXT("model.sv");
    site.unit = -9;
    site.precision = -12;
    site.result = state.result;
    kirima_foreign_adapter adapter = {c, &state, read_val, write_val};
    kirima_w4 value = {0, 0};
    kirima_foreign_arg a = {0};
    a.type = (kirima_foreign_type){8, 0, 0, 0};
    a.words = (kirima_words_in){&value, 1};
    a.name = TEXT("x");
    a.binding.adapter = &adapter;
    a.binding.flags = KIRIMA_FOREIGN_WRITABLE;
    a.binding.first = 65;
    a.binding.second = 2;
    a.binding.generation = 7;
    a.binding.root_width = a.binding.selection.width = 8;
    if (v->compile(other, &site, &a, 1, &accepted) != KIRIMA_VALUE_INVALID ||
        v->compile(c, &site, &a, 1, &accepted) || !accepted || v->phase(c, KIRIMA_FOREIGN_BEGIN) ||
        v->phase(c, KIRIMA_FOREIGN_START) ||
        v->call(c, 0, NULL, 0, answer, &state) != KIRIMA_VALUE_INVALID ||
        v->call(c, 0, &a, 1, answer, &state) || state.reads != 1 || state.writes != 1 ||
        state.answer != 42 || state.value != 22 ||
        v->detach(other, &adapter) != KIRIMA_VALUE_INVALID || v->detach(c, &adapter) ||
        v->phase(c, KIRIMA_FOREIGN_FINISH))
        return 0;
    return 1;
}
