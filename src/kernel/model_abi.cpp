// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/model_abi.h"
#include <cstring>
namespace {
bool sameField(const kirima_model_field& a, const kirima_model_field& b) {
    return a.row == b.row && a.offset == b.offset && a.logical_offset == b.logical_offset &&
           a.bytes == b.bytes && a.elements == b.elements &&
           a.cells_per_element == b.cells_per_element;
}
bool sameLayout(const kirima_model_layout& a, const kirima_model_layout& b) {
    if (a.kind > KIRIMA_MODEL_SUBROUTINE || a.kind != b.kind || a.row != b.row ||
        a.field_count != b.field_count || a.alignment != 8 || a.alignment != b.alignment ||
        a.bytes != b.bytes || a.logical_bytes != b.logical_bytes ||
        (a.field_count && (!a.fields || !b.fields)))
        return false;
    for (uint32_t i = 0; i < a.field_count; ++i)
        if (!sameField(a.fields[i], b.fields[i])) return false;
    return true;
}
}  // namespace
extern "C" uint32_t kirima_validate_module(const kirima_kernel_api* api,
                                           const kirima_module_table* m,
                                           const kirima_model_identity* expected,
                                           const kirima_model_layout* layouts, uint32_t count) {
    if (!api || !m || !expected || (count && !layouts)) return KIRIMA_MODULE_INVALID;
    if (m->descriptor_version != KIRIMA_MODULE_DESCRIPTOR || m->table_bytes < sizeof(*m) ||
        api->abi_major != KIRIMA_ABI_MAJOR || m->abi_major != api->abi_major ||
        m->minimum_minor > api->abi_minor || api->abi_minor < 5)
        return KIRIMA_MODULE_VERSION;
    if (m->identity.sir_version != expected->sir_version ||
        std::memcmp(m->identity.design_sha256, expected->design_sha256,
                    sizeof(expected->design_sha256)) != 0)
        return KIRIMA_MODULE_DESIGN;
    if (expected->layout_revision != KIRIMA_MODEL_LAYOUT_64_V1 ||
        m->identity.layout_revision != expected->layout_revision || count != m->body_count)
        return KIRIMA_MODULE_LAYOUT;
    if ((count && !m->bodies) || (m->service_count && (!m->services || !api->query_service)))
        return KIRIMA_MODULE_INVALID;
    for (uint32_t i = 0; i < m->service_count; ++i)
        if (!api->query_service(m->services[i].id, m->services[i].version))
            return KIRIMA_MODULE_SERVICE;
    for (uint32_t i = 0; i < count; ++i) {
        if (!m->bodies[i].resume) return KIRIMA_MODULE_INVALID;
        if (!sameLayout(m->bodies[i].layout, layouts[i])) return KIRIMA_MODULE_LAYOUT;
    }
    return KIRIMA_MODULE_OK;
}
