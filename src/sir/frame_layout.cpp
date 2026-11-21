// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/frame_layout.h"

#include <limits>

namespace kirimasim::sir {
namespace {
struct SegmentSizes {
    std::uint64_t header = 0;
    std::uint64_t arrays = 0;
};
struct RowShape {
    std::uint64_t elements;
    std::uint32_t chunks;
    bool array;
};
RowShape shape(const FrameVar& field) {
    const bool array = !field.dims.empty() && field.type.kind != TypeKind::Ref;
    return {array ? elementCount(field) : 1,
            isIntegral(field.type) ? integralWords(field.type.width) : 1, array};
}

bool add(SegmentSizes& sizes, RowShape row) {
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max() / kFrameCellBytes;
    if (row.elements == 0 || row.chunks == 0 || row.elements > limit / row.chunks) return false;
    const auto header = row.array ? 1 : row.chunks;
    const auto arrays = row.array ? row.elements * row.chunks : 0;
    if (header > limit - sizes.header || arrays > limit - sizes.arrays) return false;
    sizes.header += header;
    sizes.arrays += arrays;
    return sizes.header <= limit - sizes.arrays;
}

std::uint64_t advance(SegmentSizes& next, RowShape row) {
    const auto first = row.array ? next.arrays : next.header;
    next.header += row.array ? 1 : row.chunks;
    if (row.array) next.arrays += row.elements * row.chunks;
    return first * kFrameCellBytes;
}
}  // namespace

std::optional<FramePayloadSize> framePayloadLayout(std::span<const FrameVar> frame,
                                                   FrameLayoutVisitor visit, void* state) {
    if (frame.size() >= kNone) return std::nullopt;
    SegmentSizes physical, logical;
    for (const auto& field : frame) {
        const auto row = shape(field);
        if (!add(logical, row) || (field.home == FrameHome::Ordinary && !add(physical, row)))
            return std::nullopt;
    }
    if (visit) {
        SegmentSizes ordinaryNext{0, physical.header}, logicalNext{0, logical.header};
        for (std::uint32_t i = 0; i < frame.size(); ++i) {
            const auto& field = frame[i];
            const auto row = shape(field);
            std::optional<std::uint64_t> offset;
            if (field.home == FrameHome::Ordinary) offset = advance(ordinaryNext, row);
            visit(state, field,
                  {FrameVarId{i}, offset, advance(logicalNext, row),
                   row.elements * row.chunks * kFrameCellBytes, row.elements, row.chunks});
        }
    }
    return FramePayloadSize{(physical.header + physical.arrays) * kFrameCellBytes,
                            (logical.header + logical.arrays) * kFrameCellBytes};
}
}  // namespace kirimasim::sir
