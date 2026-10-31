// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <memory_resource>
#include <span>
#include <vector>

#include "kirimasim/kernel/heap.h"
#include "kirimasim/sir/design.h"

namespace kirimasim::vm::detail {

// The VM's typed holders carry Context-local token bits with b=0. Only the
// heap boundary uses an owner validation tag; it is never a source value.
kernel::ObjectResult<kernel::ObjectToken> localObject(kernel::Heap& heap, sir::Type type,
                                                      kernel::W4 value) noexcept;
kernel::ObjectResult<kernel::W4> objectValue(kernel::Heap& heap, sir::Type type,
                                             kernel::ObjectToken value) noexcept;

/// Pins exact class values during input/return/copyback transport. Ref tokens
/// retain their typed storage separately and are never guessed to be objects.
class ObjectTransfers {
public:
    explicit ObjectTransfers(kernel::Heap& heap) : heap_(heap), pins_(heap.memoryResource()) {}
    kernel::ObjectError retain(sir::Type type, std::span<const kernel::W4> values) noexcept;
    kernel::ObjectError retainArguments(std::span<const sir::Param> parameters,
                                        std::span<const std::vector<kernel::W4>> values) noexcept;
    void clear() noexcept { pins_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return pins_.size(); }

private:
    void rewind(std::size_t size) noexcept {
        while (pins_.size() > size)
            pins_.pop_back();
    }
    kernel::Heap& heap_;
    std::pmr::vector<kernel::ObjectPin> pins_;
};
}  // namespace kirimasim::vm::detail
