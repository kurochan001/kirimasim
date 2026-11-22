// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/model_references.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace kirimasim::kernel {
using Error = ActivationError;

ModelReferences::Ticket ModelReferences::publish(Entry entry) noexcept {
    if (issued_ == UINT64_MAX) return {{}, Error::ResourceLimit};
    try {
        entries_.emplace(issued_ + 1, std::move(entry));
        return {++issued_};
    } catch (const std::bad_alloc&) {
        return {{}, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, Error::ResourceLimit};
    }
}
ModelReferences::Ticket ModelReferences::create(ActivationLayoutId layout) noexcept {
    auto lease = store_.create(layout);
    if (!lease) return {{}, lease.error};
    return publish({std::move(lease.value), {}, false});
}
ModelReferences::Ticket ModelReferences::publishView(ActivationPart part) noexcept {
    const auto region = store_.region(part);
    if (!region) return {{}, region.error};
    part.width = region.value.width;
    return publish({{}, part, true});
}
ModelReferences::Ticket ModelReferences::bind(std::uint64_t lease, std::uint32_t field,
                                              std::uint32_t first, std::uint32_t elements,
                                              std::uint32_t lsb, std::uint32_t width,
                                              bool readOnly) noexcept {
    const auto it = entries_.find(lease);
    if (it == entries_.end() || it->second.view) return {{}, Error::InvalidBinding};
    const auto binding = store_.bind(it->second.lease, field, readOnly);
    if (!binding) return {{}, binding.error};
    return publishView({binding.value, first, elements, lsb, width});
}
ActivationResult<ActivationPart> ModelReferences::part(std::uint64_t view) const noexcept {
    const auto it = entries_.find(view);
    if (it == entries_.end() || !it->second.view) return {{}, Error::InvalidBinding};
    const auto& part = it->second.part;
    const auto region = store_.region(part);
    if (!region) return {{}, region.error};
    return {part};
}
ModelReferences::Ticket ModelReferences::select(std::uint64_t view, std::uint32_t first,
                                                std::uint32_t elements, std::uint32_t lsb,
                                                std::uint32_t width, bool readOnly) noexcept {
    const auto parent = part(view);
    if (!parent) return {{}, parent.error};
    auto narrowed = parent.value;
    if (!elements || first >= narrowed.numElems || elements > narrowed.numElems - first ||
        lsb >= narrowed.width || width > narrowed.width - lsb)
        return {{}, Error::OutOfRange};
    if (readOnly) {
        const auto binding = store_.readOnly(narrowed.binding);
        if (!binding) return {{}, binding.error};
        narrowed.binding = binding.value;
    }
    narrowed.firstElem += first;
    narrowed.numElems = elements;
    narrowed.lsb += lsb;
    narrowed.width = width == 0 ? narrowed.width - lsb : width;
    return publishView(narrowed);
}
ModelReferences::Ticket ModelReferences::retain(std::uint64_t ticket) noexcept {
    const auto it = entries_.find(ticket);
    if (it == entries_.end()) return {{}, Error::InvalidBinding};
    const Entry& entry = it->second;
    auto lease = entry.view ? store_.retain(entry.part.binding) : store_.retain(entry.lease);
    if (!lease) return {{}, lease.error};
    return publish({std::move(lease.value), entry.part, entry.view});
}
ActivationError ModelReferences::release(std::uint64_t ticket) noexcept {
    return entries_.erase(ticket) ? Error::None : Error::InvalidBinding;
}
}  // namespace kirimasim::kernel
