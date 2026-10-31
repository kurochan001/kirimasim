// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "object_values.h"

#include <new>
#include <stdexcept>

namespace kirimasim::vm::detail {
namespace {
bool classView(sir::Type type) noexcept {
    return sir::isClass(type) && type.width == 0 && type.detail == sir::kNone &&
           type.object != sir::kNone && !type.readOnlyRef;
}
}  // namespace
kernel::ObjectResult<kernel::ObjectToken> localObject(kernel::Heap& heap, sir::Type type,
                                                      kernel::W4 value) noexcept {
    if (!classView(type)) return {{}, kernel::ObjectError::InvalidClass};
    if (value.b != 0) return {{}, kernel::ObjectError::InvalidValue};
    const kernel::ObjectToken token{value.a, value.a == 0 ? nullptr : &heap};
    return heap.wordObject(heap.objectWord(token), kernel::ObjectClassId{type.object.index()});
}
kernel::ObjectResult<kernel::W4> objectValue(kernel::Heap& heap, sir::Type type,
                                             kernel::ObjectToken value) noexcept {
    if (!classView(type)) return {{}, kernel::ObjectError::InvalidClass};
    const auto checked =
        heap.wordObject(heap.objectWord(value), kernel::ObjectClassId{type.object.index()});
    return checked ? kernel::ObjectResult<kernel::W4>{{checked.value.bits, 0}, {}}
                   : kernel::ObjectResult<kernel::W4>{{}, checked.error};
}
kernel::ObjectError ObjectTransfers::retain(sir::Type type,
                                            std::span<const kernel::W4> values) noexcept {
    const auto view = localObject(heap_, type, {});
    if (!view) return view.error;
    const auto before = pins_.size();
    try {
        for (const auto value : values) {
            const auto token = localObject(heap_, type, value);
            if (!token) {
                rewind(before);
                return token.error;
            }
            auto pin = heap_.pinObject(token.value);
            if (!pin) {
                rewind(before);
                return pin.error;
            }
            if (pin.value) pins_.push_back(std::move(pin.value));
        }
        return kernel::ObjectError::None;
    } catch (const std::bad_alloc&) {
        rewind(before);
        return kernel::ObjectError::ResourceLimit;
    } catch (const std::length_error&) {
        rewind(before);
        return kernel::ObjectError::ResourceLimit;
    }
}
kernel::ObjectError
ObjectTransfers::retainArguments(std::span<const sir::Param> parameters,
                                 std::span<const std::vector<kernel::W4>> values) noexcept {
    if (values.size() != parameters.size()) return kernel::ObjectError::InvalidValue;
    const auto before = pins_.size();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto& parameter = parameters[i];
        if (!sir::isClass(parameter.type) || parameter.dir == sir::Direction::Ref ||
            parameter.dir == sir::Direction::ConstRef)
            continue;
        const auto error = retain(parameter.type, values[i]);
        if (error != kernel::ObjectError::None) {
            rewind(before);
            return error;
        }
    }
    return kernel::ObjectError::None;
}
}  // namespace kirimasim::vm::detail
