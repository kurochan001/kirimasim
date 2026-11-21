// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <stdexcept>

#include "kirimasim/kernel/w4.h"
#include "kirimasim/sir/frame_layout.h"

namespace kirimasim::vm::detail {

/// Ordinary storage plus the private logical address space used by refs.
/// Promoted/captured values occupy no ordinary words or ordinary root rows.
struct FrameLayout {
    std::size_t size = 0, logicalSize = 0;
    std::pmr::vector<std::size_t> offset, logical;
    std::pmr::vector<std::uint32_t> chunks;
    std::pmr::vector<std::size_t> roots, eventRoots, stringRoots, objectRoots;
    explicit FrameLayout(std::span<const sir::FrameVar> frame = {},
                         std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
        : offset(memory), logical(memory), chunks(memory), roots(memory), eventRoots(memory),
          stringRoots(memory), objectRoots(memory) {
        offset.resize(frame.size());
        logical.resize(frame.size());
        chunks.resize(frame.size());
        static_assert(sizeof(kernel::W4) == sir::kFrameCellBytes);
        static_assert(alignof(kernel::W4) == sir::kFrameCellAlignment);
        const auto payload = sir::framePayloadLayout(frame, remember, this);
        if (!payload) throw std::length_error("frame layout exceeds the address space");
        size = payload->bytes / sir::kFrameCellBytes;
        logicalSize = payload->logicalBytes / sir::kFrameCellBytes;
    }
    [[nodiscard]] std::size_t wordOf(sir::FrameVarId field) const { return offset[field.index()]; }
    [[nodiscard]] std::uint32_t chunksOfVar(sir::FrameVarId field) const {
        return chunks[field.index()];
    }

private:
    static void remember(void* state, const sir::FrameVar& row,
                         const sir::FrameFieldLayout& field) {
        auto& layout = *static_cast<FrameLayout*>(state);
        const auto i = field.id.index();
        layout.logical[i] = field.logicalOffset / sir::kFrameCellBytes;
        layout.chunks[i] = field.chunksPerElement;
        if (!field.offset) return;
        layout.offset[i] = *field.offset / sir::kFrameCellBytes;
        if (row.type.kind == sir::TypeKind::Ref || sir::isContainer(row.type))
            layout.roots.push_back(i);
        if (row.type.kind == sir::TypeKind::Event) layout.eventRoots.push_back(i);
        if (row.type.kind == sir::TypeKind::String) layout.stringRoots.push_back(i);
        if (sir::isClass(row.type)) layout.objectRoots.push_back(i);
    }
};

}  // namespace kirimasim::vm::detail
