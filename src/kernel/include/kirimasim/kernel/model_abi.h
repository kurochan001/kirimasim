// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/abi.h"
#include <signal.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_MODULE_DESCRIPTOR 1u
#define KIRIMA_MODEL_LAYOUT_64_V1 1u
#if defined(_WIN32)
#define KIRIMA_MODULE_EXPORT __declspec(dllexport)
#else
#define KIRIMA_MODULE_EXPORT __attribute__((visibility("default")))
#endif

enum kirima_body_kind { KIRIMA_MODEL_PROCESS, KIRIMA_MODEL_SUBROUTINE };
typedef struct kirima_model_identity {
    uint32_t sir_version, layout_revision;
    uint8_t design_sha256[32];
} kirima_model_identity;
typedef struct kirima_model_field {
    uint64_t offset, logical_offset, bytes, elements;
    uint32_t row, cells_per_element;
} kirima_model_field;
typedef struct kirima_model_layout {
    uint32_t kind, row, field_count, alignment;
    uint64_t bytes, logical_bytes;
    const kirima_model_field* fields;
} kirima_model_layout;
/* Passed as the opaque frame of a module resume. The host owns this
 * record and aligned payload for the run. continuation and private_state
 * start at zero. A model with private resources supplies dispose; the host
 * calls it once after the scheduler releases every use, including on kill. */
typedef struct kirima_model_activation {
    const kirima_kernel_api* kernel;
    uint32_t process, continuation;
    void* payload;
    void* private_state;
} kirima_model_activation;
typedef struct kirima_model_body {
    kirima_model_layout layout;
    kirima_resume_fn resume;
    void (*dispose)(kirima_context*, kirima_model_activation*);
} kirima_model_body;
typedef struct kirima_service_requirement {
    uint32_t id, version;
} kirima_service_requirement;
typedef struct kirima_module_table {
    uint32_t descriptor_version, table_bytes, abi_major, minimum_minor;
    kirima_model_identity identity;
    uint32_t body_count, service_count;
    const kirima_model_body* bodies;
    const kirima_service_requirement* services;
} kirima_module_table;
/* Optional versioned tail of the descriptor. The ordinary descriptor remains
 * the prefix. Its entries do not require the fusion service. */
#define KIRIMA_SERVICE_FUSION 19u
#define KIRIMA_FUSION_VERSION 1u
typedef struct kirima_fusion_api {
    uint32_t table_bytes;
} kirima_fusion_api;
typedef struct kirima_fusion_slot {
    uint32_t slot, width, planes;
} kirima_fusion_slot;
typedef struct kirima_fusion_edge {
    uint64_t bits, members;
} kirima_fusion_edge;
typedef struct kirima_fusion_step {
    uint64_t consumers;
    const kirima_fusion_edge* partial;
    uint32_t partial_count;
} kirima_fusion_step;
typedef struct kirima_fusion_write {
    uint32_t member, slot;
    uint64_t old_a, old_b, new_a, new_b;
} kirima_fusion_write;
typedef struct kirima_fusion_trace {
    kirima_fusion_write* records;
    uint64_t capacity, count;
} kirima_fusion_trace;
/* Borrowed only until evaluate returns. The generated code may access the
 * validated slot words and the dirty set, with no observer reentry. Its
 * visited mask records the completed prefix for wait rearm and accounting. */
typedef struct kirima_fusion_batch {
    const volatile sig_atomic_t* interrupt;
    uint64_t* domains[2];
    const uint64_t* offsets;
    const kirima_fusion_step* steps;
    uint64_t ready, visited, writes, changes;
    kirima_fusion_trace* trace;
} kirima_fusion_batch;
typedef struct kirima_fusion_body {
    uint32_t slot_count, member_count;
    const kirima_fusion_slot* slots;
    const uint32_t* members;
    void (*evaluate)(kirima_context*, kirima_fusion_batch*);
} kirima_fusion_body;
typedef struct kirima_fusion_module {
    kirima_module_table base;
    kirima_service_requirement service;
    kirima_fusion_body fusion;
} kirima_fusion_module;
typedef const kirima_module_table* (*kirima_module_query_fn)(uint32_t, uint32_t);
const kirima_module_table* kirima_get_module_table(uint32_t kernel_major, uint32_t kernel_minor);
enum kirima_module_status {
    KIRIMA_MODULE_OK,
    KIRIMA_MODULE_INVALID,
    KIRIMA_MODULE_VERSION,
    KIRIMA_MODULE_DESIGN,
    KIRIMA_MODULE_LAYOUT,
    KIRIMA_MODULE_SERVICE
};
/* Host-side check, before registering any model entry. expected_layouts are
 * computed from the verified Design for the model's ordered list of bodies.
 * All pointers are borrowed for this call. The host owns the library lifetime. */
uint32_t kirima_validate_module(const kirima_kernel_api*, const kirima_module_table*,
                                const kirima_model_identity* expected,
                                const kirima_model_layout* expected_layouts, uint32_t body_count);
#ifdef __cplusplus
}
#endif
