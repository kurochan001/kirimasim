// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <memory_resource>
#include <span>
#include <vector>

#include "kirimasim/kernel/heap.h"
#include "kirimasim/sir/design.h"

namespace kirimasim::vm::detail {

/// A retained, originally selected field. Moves transfer its ownership;
/// rebinding the source variable never changes this target. Values crossing
/// this boundary use the VM representation, including class handles in arrays.
class ObjectProperty {
public:
    ObjectProperty() = default;
    ObjectProperty(const ObjectProperty&) = delete;
    ObjectProperty& operator=(const ObjectProperty&) = delete;
    ObjectProperty(ObjectProperty&& other) noexcept;
    ObjectProperty& operator=(ObjectProperty&& other) noexcept;
    kernel::ObjectResult<ObjectProperty> element(std::uint64_t index) const noexcept;
    kernel::ObjectError read(std::span<kernel::W4> output) const noexcept;
    kernel::ObjectError write(std::span<const kernel::W4> input,
                              const kernel::ObjectStage* stage = nullptr) const noexcept;
    [[nodiscard]] std::size_t words() const noexcept { return ref_.count; }
    [[nodiscard]] kernel::ObjectToken owner() const noexcept { return pin_.token(); }

private:
    friend class HeapPlan;
    ObjectProperty(kernel::Heap& heap, kernel::ObjectPin pin, kernel::ObjectRef ref, sir::Type type,
                   std::uint64_t elements) noexcept;
    kernel::Heap* heap_ = nullptr;
    kernel::ObjectPin pin_;
    kernel::ObjectRef ref_;
    sir::Type type_;
    std::uint64_t elements_ = 0;
};

/// One immutable SIR-to-kernel heap schema. Container IDs need not be SIR
/// row numbers; class IDs preserve the descriptor order, including forward
/// recursive fields. The design and Context outlive the plan. Registration
/// creates no source storage, objects, roots or random draws.
class HeapPlan {
public:
    HeapPlan() = default;
    HeapPlan(const HeapPlan&) = delete;
    HeapPlan& operator=(const HeapPlan&) = delete;
    HeapPlan(HeapPlan&& other) noexcept;
    HeapPlan& operator=(HeapPlan&&) = delete;
    static kernel::HeapResult<HeapPlan>
    compile(kernel::Heap& heap, const sir::Design& design,
            std::pmr::memory_resource* memory = std::pmr::new_delete_resource()) noexcept;
    [[nodiscard]] std::span<const kernel::HeapTypeId> containers() const noexcept {
        return containers_;
    }
    [[nodiscard]] kernel::HeapResult<kernel::HeapLayoutId>
    fieldLayout(sir::ClassFieldId field) const noexcept;
    [[nodiscard]] kernel::ObjectResult<ObjectProperty>
    property(kernel::ObjectToken object, sir::ClassFieldId field,
             bool readonly = false) const noexcept;
    [[nodiscard]] bool valid() const noexcept { return heap_ != nullptr; }

private:
    explicit HeapPlan(std::pmr::memory_resource* memory) : containers_(memory), fields_(memory) {}
    kernel::Heap* heap_ = nullptr;
    const sir::Design* design_ = nullptr;
    std::pmr::vector<kernel::HeapTypeId> containers_;
    std::pmr::vector<std::pmr::vector<kernel::HeapLayoutId>> fields_;
};
}  // namespace kirimasim::vm::detail
