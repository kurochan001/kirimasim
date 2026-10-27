// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>

#include "kirimasim/sir/design.h"

namespace kirimasim::vm::detail {

/// Ordinary storage plus the private logical address space used by refs.
/// Promoted/captured values occupy no ordinary words or ordinary root rows.
struct FrameLayout {
    std::size_t size = 0, logicalSize = 0;
    std::pmr::vector<std::size_t> offset, logical;
    std::pmr::vector<std::uint32_t> chunks;
    std::pmr::vector<std::size_t> roots, eventRoots, stringRoots;
    explicit FrameLayout(std::span<const sir::FrameVar> frame = {},
                         std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
        : offset(memory), logical(memory), chunks(memory), roots(memory), eventRoots(memory),
          stringRoots(memory) {
        offset.resize(frame.size());
        logical.resize(frame.size());
        chunks.resize(frame.size());
        for (std::size_t i = 0; i < frame.size(); ++i) {
            const auto& row = frame[i];
            const bool ref = row.type.kind == sir::TypeKind::Ref;
            chunks[i] = sir::isIntegral(row.type)
                            ? row.type.width / 64 + (row.type.width % 64 != 0 ? 1U : 0U)
                            : 1;
            const auto words = row.dims.empty() || ref ? chunks[i] : 1;
            logical[i] = logicalSize;
            logicalSize += words;
            if (row.home != sir::FrameHome::Ordinary) continue;
            offset[i] = size;
            size += words;
            if (ref || sir::isContainer(row.type)) roots.push_back(i);
            if (row.type.kind == sir::TypeKind::Event) eventRoots.push_back(i);
            if (row.type.kind == sir::TypeKind::String) stringRoots.push_back(i);
        }
        for (std::size_t i = 0; i < frame.size(); ++i) {
            const auto& row = frame[i];
            if (row.dims.empty() || row.type.kind == sir::TypeKind::Ref) continue;
            const auto words = sir::elementCount(row) * chunks[i];
            logical[i] = logicalSize;
            logicalSize += words;
            if (row.home != sir::FrameHome::Ordinary) continue;
            offset[i] = size;
            size += words;
        }
    }
    [[nodiscard]] std::size_t wordOf(sir::FrameVarId field) const { return offset[field.index()]; }
    [[nodiscard]] std::uint32_t chunksOfVar(sir::FrameVarId field) const {
        return chunks[field.index()];
    }
};

}  // namespace kirimasim::vm::detail
