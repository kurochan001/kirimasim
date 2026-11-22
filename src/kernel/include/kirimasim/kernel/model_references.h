// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstdint>
#include <map>
#include <memory_resource>

#include "kirimasim/kernel/activation.h"

namespace kirimasim::kernel {

/// Tickets held by a model across native callback boundaries. A lexical view
/// borrows its binding; only create/retain owns an ActivationLease. The store
/// outlives every row, including during Context destruction.
class ModelReferences {
public:
    ModelReferences(ActivationStore& store, std::pmr::memory_resource* memory)
        : store_(store), entries_(memory) {}
    ModelReferences(const ModelReferences&) = delete;
    ModelReferences& operator=(const ModelReferences&) = delete;
    using Ticket = ActivationResult<std::uint64_t>;
    Ticket create(ActivationLayoutId layout) noexcept;
    Ticket bind(std::uint64_t lease, std::uint32_t field, std::uint32_t first,
                std::uint32_t elements, std::uint32_t lsb, std::uint32_t width,
                bool readOnly) noexcept;
    Ticket select(std::uint64_t view, std::uint32_t first, std::uint32_t elements,
                  std::uint32_t lsb, std::uint32_t width, bool readOnly) noexcept;
    Ticket retain(std::uint64_t ticket) noexcept;
    ActivationError release(std::uint64_t ticket) noexcept;
    ActivationResult<ActivationPart> part(std::uint64_t view) const noexcept;

private:
    struct Entry {
        ActivationLease lease;
        ActivationPart part;
        bool view = false;
    };
    Ticket publish(Entry entry) noexcept;
    Ticket publishView(ActivationPart part) noexcept;
    ActivationStore& store_;
    std::pmr::map<std::uint64_t, Entry> entries_;
    std::uint64_t issued_ = 0;
};
}  // namespace kirimasim::kernel
