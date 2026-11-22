// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/reference_abi.h"

#include <new>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/wide.h"
#include "value_services.h"

namespace kirimasim::kernel {
namespace {

#define CHECK_ENUM(c, native) static_assert((c) == static_cast<unsigned>(native))
CHECK_ENUM(KIRIMA_REF_OK, ActivationError::None);
CHECK_ENUM(KIRIMA_REF_LAYOUT, ActivationError::InvalidLayout);
CHECK_ENUM(KIRIMA_REF_INVALID, ActivationError::InvalidBinding);
CHECK_ENUM(KIRIMA_REF_BOUNDS, ActivationError::OutOfRange);
CHECK_ENUM(KIRIMA_REF_READONLY, ActivationError::ReadOnly);
CHECK_ENUM(KIRIMA_REF_VALUE, ActivationError::InvalidValue);
CHECK_ENUM(KIRIMA_REF_RESOURCE, ActivationError::ResourceLimit);
CHECK_ENUM(KIRIMA_FIELD_TWO, ActivationKind::Two);
CHECK_ENUM(KIRIMA_FIELD_FOUR, ActivationKind::Four);
CHECK_ENUM(KIRIMA_FIELD_REAL, ActivationKind::Real);
CHECK_ENUM(KIRIMA_FIELD_SHORTREAL, ActivationKind::ShortReal);
CHECK_ENUM(KIRIMA_FIELD_STRING, ActivationKind::String);
CHECK_ENUM(KIRIMA_FIELD_CHANDLE, ActivationKind::Chandle);
CHECK_ENUM(KIRIMA_FIELD_CONTAINER, ActivationKind::Container);
CHECK_ENUM(KIRIMA_FIELD_CELL, ActivationKind::Cell);
CHECK_ENUM(KIRIMA_FIELD_EVENT, ActivationKind::Event);
CHECK_ENUM(KIRIMA_FIELD_OBJECT, ActivationKind::Object);
#undef CHECK_ENUM
static_assert(sizeof(kirima_reference) == 16 && alignof(kirima_reference) == 8);

std::uint32_t addLayout(kirima_context* c, const kirima_activation_field* fields, std::uint32_t n,
                        kirima_activation_layout* out) noexcept {
    if (!c || !out || (n && !fields)) return KIRIMA_REF_INVALID;
    try {
        std::vector<ActivationField> native;
        native.reserve(n);
        for (const auto& field : std::span(fields, n)) {
            if (field.kind > KIRIMA_FIELD_OBJECT || (!field.dimensions && field.dimension_count))
                return KIRIMA_REF_LAYOUT;
            auto& f = native.emplace_back();
            f.kind = static_cast<ActivationKind>(field.kind);
            f.width = field.width;
            f.objectView = ObjectClassId{field.class_view};
            for (const auto& d : std::span(field.dimensions, field.dimension_count))
                f.dims.push_back({d.left, d.right});
        }
        const auto result = fromC(c).activations().addLayout(native);
        if (result) *out = {c, result.value.index(), 0};
        return static_cast<std::uint32_t>(result.error);
    } catch (const std::bad_alloc&) {
        return KIRIMA_REF_RESOURCE;
    } catch (const std::length_error&) {
        return KIRIMA_REF_RESOURCE;
    }
}
std::uint32_t ticket(kirima_context* c, ModelReferences::Ticket issued,
                     kirima_reference* out) noexcept {
    if (issued) *out = {c, issued.value};
    return static_cast<std::uint32_t>(issued.error);
}
std::uint32_t create(kirima_context* c, kirima_activation_layout layout,
                     kirima_reference* out) noexcept {
    if (!c || !out || layout.owner != c || layout.reserved) return KIRIMA_REF_LAYOUT;
    return ticket(c, fromC(c).references().create(ActivationLayoutId{layout.index}), out);
}
std::uint32_t bind(kirima_context* c, kirima_reference lease, std::uint32_t field,
                   kirima_ref_window w, std::uint32_t ro, kirima_reference* out) noexcept {
    if (!c || lease.owner != c || !out || ro > 1) return KIRIMA_REF_INVALID;
    return ticket(c,
                  fromC(c).references().bind(lease.ticket, field, w.first_element, w.elements,
                                             w.lsb, w.width, ro != 0),
                  out);
}
std::uint32_t select(kirima_context* c, kirima_reference view, kirima_ref_window w,
                     std::uint32_t ro, kirima_reference* out) noexcept {
    if (!c || view.owner != c || !out || ro > 1) return KIRIMA_REF_INVALID;
    return ticket(c,
                  fromC(c).references().select(view.ticket, w.first_element, w.elements, w.lsb,
                                               w.width, ro != 0),
                  out);
}
std::uint32_t retain(kirima_context* c, kirima_reference ref, kirima_reference* out) noexcept {
    if (!c || ref.owner != c || !out) return KIRIMA_REF_INVALID;
    return ticket(c, fromC(c).references().retain(ref.ticket), out);
}
std::uint32_t release(kirima_context* c, kirima_reference ref) noexcept {
    if (!c || ref.owner != c) return KIRIMA_REF_INVALID;
    return static_cast<std::uint32_t>(fromC(c).references().release(ref.ticket));
}
template <class Buffer>
std::uint32_t access(kirima_context* c, kirima_reference ref, std::uint32_t element,
                     Buffer buffer) noexcept {
    if (!c || ref.owner != c) return KIRIMA_REF_INVALID;
    if (!buffer.words || !buffer.width || buffer.count != wordsFor(buffer.width))
        return KIRIMA_REF_VALUE;
    auto& ctx = fromC(c);
    const auto source = ctx.references().part(ref.ticket);
    if (!source) return static_cast<std::uint32_t>(source.error);
    const auto& part = source.value;
    if (element >= part.numElems || buffer.width != part.width) return KIRIMA_REF_BOUNDS;
    const auto index = part.firstElem + element;
    const std::span cells(buffer.words, buffer.count);
    if constexpr (std::is_same_v<Buffer, kirima_value_in>) {
        return static_cast<std::uint32_t>(
            ctx.scheduler().writeActivation(part.binding, index, cells, part.lsb, part.width));
    } else {
        return static_cast<std::uint32_t>(
            ctx.activations().read(part.binding, index, cells, part.lsb, part.width));
    }
}
constexpr kirima_reference_api api{addLayout,
                                   create,
                                   bind,
                                   select,
                                   retain,
                                   release,
                                   access<kirima_value_out>,
                                   access<kirima_value_in>};
}  // namespace
const void* queryReferenceService(std::uint32_t id, std::uint32_t version) noexcept {
    return id == KIRIMA_SERVICE_REFERENCE && version == KIRIMA_REFERENCE_VERSION ? &api : nullptr;
}
}  // namespace kirimasim::kernel
