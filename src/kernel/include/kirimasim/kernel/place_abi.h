// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/reference_abi.h"
#include "kirimasim/kernel/signal_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_PLACE 16u
#define KIRIMA_PLACE_VERSION 1u
enum kirima_place_origin {
    KIRIMA_PLACE_SLOT,
    KIRIMA_PLACE_ORDINARY,
    KIRIMA_PLACE_LEXICAL,
    KIRIMA_PLACE_CELL,
    KIRIMA_PLACE_OBJECT
};
typedef struct kirima_place_info {
    kirima_ref_window window;
    uint32_t kind, read_only, local, actual_type;
} kirima_place_info;
typedef struct kirima_place_rights {
    uint32_t read_only, local, actual_type;
} kirima_place_rights;
typedef struct kirima_frame_identity {
    uint64_t row, generation;
} kirima_frame_identity;
typedef struct kirima_frame_provider {
    const kirima_context* owner;
    void* frame;
    uint32_t (*retain)(void*, kirima_frame_identity);
    void (*release)(void*, kirima_frame_identity);
    uint32_t (*resolve)(void*, kirima_frame_identity, kirima_words_out* current_cells);
} kirima_frame_provider;
typedef uint32_t (*kirima_frame_visitor)(void*, const kirima_frame_provider*,
                                         kirima_frame_identity);
typedef struct kirima_run_hooks {
    const kirima_context* owner;
    void* frame;
    void (*safe_point)(void*);
    void (*pulse)(void*, uint32_t kind, uint32_t id, uint64_t time);
} kirima_run_hooks;
typedef struct kirima_lexical_region {
    uint64_t extent;
    uint32_t field, read_only;
    kirima_ref_window window;
} kirima_lexical_region;
/* The slot or frame declaration supplies its actual type (UINT32_MAX when
 * absent). Offsets and strides of ordinary fields count 16-byte W4 cells.
 * Retain/release pin a generation. Resolve supplies a fresh borrowed span
 * for one synchronous access and must never publish a pointer on failure.
 * Each acquired/narrowed ticket must be released. Root visitors and frame
 * providers cannot reenter or mutate the Context. Actions return REF status.
 * A zero stage is required for a write outside a native constructor. */
typedef struct kirima_place_api {
    uint32_t (*slot)(kirima_context*, uint32_t slot, kirima_place_rights, kirima_resource*);
    uint32_t (*ordinary)(kirima_context*, kirima_frame_provider*, kirima_frame_identity,
                         uint64_t offset, uint32_t elements, uint32_t width, kirima_place_rights,
                         kirima_resource*);
    uint32_t (*lexical)(kirima_context*, kirima_reference, kirima_place_rights, kirima_resource*);
    uint32_t (*cell)(kirima_context*, kirima_heap_token, uint32_t layout, uint32_t width,
                     kirima_place_rights, kirima_resource*);
    uint32_t (*object)(kirima_context*, kirima_resource field, uint32_t width, kirima_place_rights,
                       kirima_resource*);
    uint32_t (*select)(kirima_context*, kirima_resource, kirima_ref_window, uint32_t read_only,
                       kirima_resource*);
    uint32_t (*describe)(kirima_context*, kirima_resource, kirima_place_info*);
    uint32_t (*read)(kirima_context*, kirima_resource, uint32_t element, kirima_value_out);
    uint32_t (*write)(kirima_context*, kirima_resource, uint32_t element, kirima_value_in,
                      kirima_resource stage);
    uint32_t (*release)(kirima_context*, kirima_resource);
    uint32_t (*lexical_region)(kirima_context*, kirima_reference, kirima_lexical_region*);
    uint32_t (*visit_frames)(kirima_context*, kirima_frame_visitor, void*, uint32_t* completed);
    uint32_t (*attach_hooks)(kirima_context*, kirima_run_hooks*, kirima_resource*);
    uint32_t (*detach_hooks)(kirima_context*, kirima_resource);
} kirima_place_api;
#ifdef __cplusplus
}
#endif
