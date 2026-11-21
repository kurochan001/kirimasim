// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "kirimasim/sir/design.h"

namespace kirimasim::sir {

// Layout of the ordinary frame payload on the 64-bit tier-1 and tier-2 targets.
// Offsets are bytes. A scoped or captured row has an activation binding and no
// ordinary payload. Logical addresses, used to step refs, also cover such rows.
inline constexpr std::uint32_t kFrameCellBytes = 16;
inline constexpr std::uint32_t kFrameCellAlignment = 8;

struct FrameFieldLayout {
    FrameVarId id;
    std::optional<std::uint64_t> offset;
    std::uint64_t logicalOffset;
    std::uint64_t size;
    std::uint64_t elements;
    std::uint32_t chunksPerElement;
};

struct FramePayloadSize {
    std::uint64_t bytes;
    std::uint64_t logicalBytes;
};

using FrameLayoutVisitor = void (*)(void*, const FrameVar&, const FrameFieldLayout&);

/// Check all sizes before publishing any row. Row IDs pair each descriptor
/// with its type, value/root kind, home and activation IDs in the verified SIR.
std::optional<FramePayloadSize> framePayloadLayout(std::span<const FrameVar> frame,
                                                   FrameLayoutVisitor visit = nullptr,
                                                   void* state = nullptr);
}  // namespace kirimasim::sir
