// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/net_abi.h"
#include "kirimasim/kernel/reference_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_FOREIGN 15u
#define KIRIMA_FOREIGN_VERSION 1u
typedef struct kirima_foreign_type {
    uint32_t width, four_state, is_signed, real;
} kirima_foreign_type;
typedef struct kirima_foreign_access {
    uint64_t first, second, generation;
    uint32_t root_width, lsb, width;
} kirima_foreign_access;
typedef struct kirima_foreign_adapter {
    const kirima_context* owner;
    void* frame;
    uint32_t (*read)(void*, kirima_foreign_access, kirima_words_out);
    uint32_t (*write)(void*, kirima_foreign_access, kirima_words_in);
} kirima_foreign_adapter;
enum kirima_foreign_right {
    KIRIMA_FOREIGN_SLOT = 1u,
    KIRIMA_FOREIGN_WRITABLE = 2u,
    KIRIMA_FOREIGN_NET = 4u,
    KIRIMA_FOREIGN_WHOLE_NET = 8u
};
typedef struct kirima_foreign_binding {
    kirima_slot_bits selection;
    kirima_foreign_adapter* adapter;
    uint64_t first, second, generation;
    uint32_t root_width, flags;
} kirima_foreign_binding;
typedef struct kirima_foreign_declaration {
    int64_t left, right;
    uint32_t scalar, integer;
} kirima_foreign_declaration;
typedef struct kirima_foreign_arg {
    kirima_foreign_type type;
    kirima_words_in words;
    kirima_foreign_binding binding;
    kirima_bytes name;
    const kirima_foreign_declaration* declaration;
    uint32_t available;
} kirima_foreign_arg;
typedef struct kirima_foreign_object {
    kirima_bytes name, file, scope, parent, leaf;
    int32_t kind, unit, precision;
    uint32_t line, indexed, scalar;
    int64_t left, right, index;
    kirima_foreign_type type;
    kirima_foreign_binding binding;
    const kirima_activation_dim* dimensions;
    uint32_t dimension_count;
} kirima_foreign_object;
typedef struct kirima_foreign_site {
    uint32_t site, routine, line;
    int32_t unit, precision;
    kirima_bytes scope, file;
    kirima_foreign_type result;
} kirima_foreign_site;
typedef void (*kirima_routine_sink)(void*, uint32_t row, kirima_bytes name, kirima_foreign_type,
                                    uint32_t task);
typedef void (*kirima_foreign_result)(void*, kirima_foreign_type, kirima_words_in);
enum kirima_foreign_phase { KIRIMA_FOREIGN_BEGIN, KIRIMA_FOREIGN_START, KIRIMA_FOREIGN_FINISH };
/* Object kind is a public vpi_user.h constant. All word and text views passed
 * to a sink are borrowed during that call. The binding adapter is retained by
 * pointer. Keep it live until detach and check the supplied generation for
 * every transient read/write. A Context's driver loads VPI libraries. */
typedef struct kirima_foreign_api {
    uint32_t (*state)(kirima_context*, uint32_t* enabled, uint32_t* stopped);
    uint32_t (*routines)(kirima_context*, kirima_routine_sink, void*);
    uint32_t (*object)(kirima_context*, const kirima_foreign_object*);
    uint32_t (*compile)(kirima_context*, const kirima_foreign_site*, const kirima_foreign_arg*,
                        uint32_t, uint32_t* accepted);
    uint32_t (*call)(kirima_context*, uint32_t site, const kirima_foreign_arg*, uint32_t,
                     kirima_foreign_result, void*);
    uint32_t (*detach)(kirima_context*, kirima_foreign_adapter*);
    uint32_t (*phase)(kirima_context*, uint32_t phase);
    uint32_t (*error)(kirima_context*, kirima_byte_sink, void*);
    uint32_t (*report_error)(kirima_context*, kirima_bytes);
} kirima_foreign_api;
#ifdef __cplusplus
}
#endif
