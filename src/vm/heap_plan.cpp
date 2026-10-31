// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "heap_plan.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/sir/classes.h"
#include "object_values.h"

namespace kirimasim::vm::detail {
namespace {
using Error = kernel::HeapError;
using ObjectError = kernel::ObjectError;
std::uint32_t chunks(sir::Type type) noexcept {
    return sir::isIntegral(type) ? sir::integralWords(type.width) : 1;
}
bool supported(const sir::Design& d, sir::Type type, std::size_t before) {
    if (type.readOnlyRef) return false;
    if (sir::isIntegral(type))
        return type.width != 0 && type.detail == sir::kNone && type.object == sir::kNone;
    if (type.width != 0) return false;
    if (sir::isClass(type))
        return type.detail == sir::kNone && type.object.index() < d.classes.size();
    if (type.object != sir::kNone) return false;
    if (sir::isContainer(type))
        return type.detail.index() < before && type.detail.index() < d.types.size() &&
               d.types[type.detail.index()].kind == (type.kind == sir::TypeKind::Queue
                                                         ? sir::TypeEntryKind::Queue
                                                         : sir::TypeEntryKind::DynArray);
    return type.detail == sir::kNone &&
           (type.kind == sir::TypeKind::String || type.kind == sir::TypeKind::Chandle ||
            type.kind == sir::TypeKind::Real || type.kind == sir::TypeKind::ShortReal);
}
std::pmr::vector<kernel::HeapWord> words(sir::Type type, std::uint64_t elements,
                                         std::span<const kernel::HeapTypeId> containers,
                                         std::pmr::memory_resource* memory) {
    std::pmr::vector<kernel::HeapWord> output(memory);
    const auto count = chunks(type);
    if (elements > output.max_size() / count) throw std::length_error("heap layout capacity");
    output.reserve(static_cast<std::size_t>(elements * count));
    for (std::uint64_t element = 0; element < elements; ++element) {
        for (std::uint32_t chunk = 0; chunk < count; ++chunk) {
            kernel::HeapWord word;
            const auto width = sir::isIntegral(type) ? std::min(64U, type.width - chunk * 64U)
                               : type.kind == sir::TypeKind::ShortReal ? 32U
                                                                       : 64U;
            word.mask = kernel::mask(width);
            word.fourState = sir::isFourState(type);
            word.initial = word.fourState ? kernel::W4{word.mask, word.mask} : kernel::W4{};
            if (sir::isContainer(type)) word.nested = containers[type.detail.index()];
            word.string = type.kind == sir::TypeKind::String;
            if (sir::isClass(type)) word.object = kernel::ObjectClassId{type.object.index()};
            output.push_back(word);
        }
    }
    return output;
}
}  // namespace

HeapPlan::HeapPlan(HeapPlan&& other) noexcept
    : heap_(std::exchange(other.heap_, nullptr)), design_(std::exchange(other.design_, nullptr)),
      containers_(std::move(other.containers_)), fields_(std::move(other.fields_)) {}

kernel::HeapResult<HeapPlan> HeapPlan::compile(kernel::Heap& heap, const sir::Design& design,
                                               std::pmr::memory_resource* memory) noexcept {
    try {
        // Preflight every schema entry before registering any of it. Event
        // fields need their own managed roots and cannot be opaque handles.
        if (!sir::validateClasses(design).errors.empty()) return {{}, {Error::InvalidType}};
        for (std::size_t i = 0; i < design.types.size(); ++i) {
            const auto& type = design.types[i];
            if (type.kind != sir::TypeEntryKind::DynArray && type.kind != sir::TypeEntryKind::Queue)
                continue;
            if (!supported(design, type.value, i) ||
                (type.kind == sir::TypeEntryKind::DynArray && type.maxElements != 0))
                return {{}, {Error::InvalidType}};
        }
        for (const auto& type : design.classes)
            for (const auto& field : type.fields)
                if (!supported(design, field.type, design.types.size()))
                    return {{}, {Error::InvalidType}};
        HeapPlan plan(memory);
        plan.containers_.assign(design.types.size(), kernel::kNone);
        plan.fields_.resize(design.classes.size());
        for (std::size_t i = 0; i < design.types.size(); ++i) {
            const auto& type = design.types[i];
            if (type.kind != sir::TypeEntryKind::DynArray && type.kind != sir::TypeEntryKind::Queue)
                continue;
            const auto shape = words(type.value, 1, plan.containers_, memory);
            const auto layout = heap.addLayout(shape);
            if (!layout) return {{}, layout.status};
            const auto added = heap.addType({type.kind == sir::TypeEntryKind::Queue
                                                 ? kernel::ContainerKind::Queue
                                                 : kernel::ContainerKind::DynamicArray,
                                             layout.value, type.maxElements});
            if (!added) return {{}, added.status};
            plan.containers_[i] = added.value;
        }
        std::pmr::vector<std::pmr::vector<kernel::ObjectField>> fields(memory);
        std::pmr::vector<std::pmr::vector<kernel::ObjectClassId>> interfaces(memory);
        std::pmr::vector<std::pmr::vector<std::uint32_t>> methods(memory);
        fields.resize(design.classes.size());
        interfaces.resize(design.classes.size());
        methods.resize(design.classes.size());
        std::pmr::vector<kernel::ObjectClass> descriptions(memory);
        descriptions.reserve(design.classes.size());
        for (std::size_t i = 0; i < design.classes.size(); ++i) {
            const auto& type = design.classes[i];
            for (const auto& field : type.fields) {
                const auto shape =
                    words(field.type, sir::elementCount(field), plan.containers_, memory);
                const auto layout = heap.addLayout(shape);
                if (!layout) return {{}, layout.status};
                plan.fields_[i].push_back(layout.value);
                fields[i].push_back({layout.value, field.constant, field.hasInitializer});
            }
            for (const auto parent : type.interfaces)
                interfaces[i].push_back(kernel::ObjectClassId{parent.index()});
            for (const auto method : type.virtualMethods)
                methods[i].push_back(method.index());
            descriptions.push_back({type.name, kernel::ObjectClassId{type.base.index()},
                                    interfaces[i], fields[i], methods[i], type.abstract,
                                    type.interfaceClass,
                                    kernel::ObjectClassId{type.weakTarget.index()}});
        }
        if (!descriptions.empty()) {
            const auto error = heap.addClasses(descriptions);
            if (error != ObjectError::None)
                return {{},
                        {error == ObjectError::ResourceLimit ? Error::ResourceLimit
                                                             : Error::InvalidType}};
        }
        // No fallible operation remains after atomic class registration.
        plan.heap_ = &heap;
        plan.design_ = &design;
        return {std::move(plan), {}};
    } catch (const std::bad_alloc&) {
        return {{}, {Error::ResourceLimit}};
    } catch (const std::length_error&) {
        return {{}, {Error::ResourceLimit}};
    }
}
kernel::HeapResult<kernel::HeapLayoutId>
HeapPlan::fieldLayout(sir::ClassFieldId field) const noexcept {
    if (!heap_ || field.declaring.index() >= fields_.size() ||
        field.index >= fields_[field.declaring.index()].size())
        return {{}, {Error::InvalidType}};
    return {fields_[field.declaring.index()][field.index], {}};
}
kernel::ObjectResult<ObjectProperty> HeapPlan::property(kernel::ObjectToken object,
                                                        sir::ClassFieldId field,
                                                        bool readonly) const noexcept {
    if (!fieldLayout(field)) return {{}, ObjectError::InvalidField};
    const auto ref = heap_->objectRef(
        object, {kernel::ObjectClassId{field.declaring.index()}, field.index}, readonly);
    if (!ref) return {{}, ref.error};
    auto pin = heap_->pinObject(object);
    if (!pin) return {{}, pin.error};
    const auto& descriptor = design_->classes[field.declaring.index()].fields[field.index];
    return {ObjectProperty{*heap_, std::move(pin.value), ref.value, descriptor.type,
                           sir::elementCount(descriptor)},
            {}};
}
ObjectProperty::ObjectProperty(kernel::Heap& heap, kernel::ObjectPin pin, kernel::ObjectRef ref,
                               sir::Type type, std::uint64_t elements) noexcept
    : heap_(&heap), pin_(std::move(pin)), ref_(ref), type_(type), elements_(elements) {}
ObjectProperty::ObjectProperty(ObjectProperty&& other) noexcept
    : heap_(std::exchange(other.heap_, nullptr)), pin_(std::move(other.pin_)),
      ref_(std::exchange(other.ref_, {})), type_(other.type_),
      elements_(std::exchange(other.elements_, 0)) {}
ObjectProperty& ObjectProperty::operator=(ObjectProperty&& other) noexcept {
    if (this != &other) {
        heap_ = std::exchange(other.heap_, nullptr);
        pin_ = std::move(other.pin_);
        ref_ = std::exchange(other.ref_, {});
        type_ = other.type_;
        elements_ = std::exchange(other.elements_, 0);
    }
    return *this;
}
kernel::ObjectResult<ObjectProperty> ObjectProperty::element(std::uint64_t index) const noexcept {
    if (!heap_ || index >= elements_) return {{}, ObjectError::InvalidField};
    auto pin = heap_->pinObject(pin_.token());
    if (!pin) return {{}, pin.error};
    auto ref = ref_;
    ref.first += static_cast<std::size_t>(index * chunks(type_));
    ref.count = chunks(type_);
    return {ObjectProperty{*heap_, std::move(pin.value), ref, type_, 1}, {}};
}
ObjectError ObjectProperty::read(std::span<kernel::W4> output) const noexcept {
    if (!heap_ || output.size() != ref_.count) return ObjectError::InvalidField;
    try {
        std::pmr::vector<kernel::W4> values(output.size(), heap_->memoryResource());
        const auto error = heap_->readObject(ref_, values);
        if (error != ObjectError::None) return error;
        if (sir::isClass(type_))
            for (auto& value : values) {
                const auto token =
                    heap_->wordObject(value, kernel::ObjectClassId{type_.object.index()});
                if (!token) return token.error;
                value = {token.value.bits, 0};
            }
        std::copy(values.begin(), values.end(), output.begin());
        return ObjectError::None;
    } catch (const std::bad_alloc&) {
        return ObjectError::ResourceLimit;
    } catch (const std::length_error&) {
        return ObjectError::ResourceLimit;
    }
}
ObjectError ObjectProperty::write(std::span<const kernel::W4> input,
                                  const kernel::ObjectStage* stage) const noexcept {
    if (!heap_ || input.size() != ref_.count) return ObjectError::InvalidField;
    try {
        std::pmr::vector<kernel::W4> values(input.begin(), input.end(), heap_->memoryResource());
        if (sir::isClass(type_))
            for (auto& value : values) {
                const auto token = localObject(*heap_, type_, value);
                if (!token) return token.error;
                value = heap_->objectWord(token.value);
            }
        return heap_->writeObject(ref_, values, stage);
    } catch (const std::bad_alloc&) {
        return ObjectError::ResourceLimit;
    } catch (const std::length_error&) {
        return ObjectError::ResourceLimit;
    }
}
}  // namespace kirimasim::vm::detail
