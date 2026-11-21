// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/strings.h"
#include <algorithm>
#include <limits>

namespace kirimasim::kernel {
HeapResult<KeyPosition> Heap::locateKey(HeapTypeId type, ContainerToken value,
                                        const Strings& strings, std::span<const KeyPart> parts,
                                        std::span<const W4> key,
                                        std::span<W4> cell) const noexcept {
    const auto bad = HeapResult<KeyPosition>{{}, {HeapError::InvalidType}};
    if (parts.empty() || key.size() >= cell.size()) return bad;
    std::size_t offset = 0;
    for (const auto& part : parts) {
        const auto count = part.string ? 1U : wordsFor(part.width);
        if ((!part.string && part.width == 0) || count > key.size() - offset) return bad;
        offset += count;
    }
    if (offset != key.size()) return bad;
    if (std::ranges::any_of(key, [](auto w) { return w.b != 0; })) return {};
    const auto length = size(type, value);
    if (!length) return {{}, length.status};
    if (length.value > std::numeric_limits<std::int64_t>::max())
        return {{}, {HeapError::ResourceLimit}};
    const auto compare = [&] {
        std::size_t start = 0;
        for (const auto& part : parts) {
            if (part.string) {
                const auto cmp = strings.get(cell[start].a).compare(strings.get(key[start].a));
                if (cmp != 0) return cmp;
                ++start;
            } else {
                const auto count = wordsFor(part.width);
                // Flipping the sign bit maps signed numerical order onto unsigned order.
                const auto sign =
                    part.signedIntegral ? std::uint64_t{1} << ((part.width - 1) % 64) : 0;
                for (std::size_t j = count; j > 0; --j) {
                    const auto flip = j == count ? sign : 0;
                    const auto a = cell[start + j - 1].a ^ flip, b = key[start + j - 1].a ^ flip;
                    if (a != b) return a < b ? -1 : 1;
                }
                start += count;
            }
        }
        return 0;
    };
    std::int64_t low = 0, high = static_cast<std::int64_t>(length.value);
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        const auto status = read(type, value, mid, cell);
        if (!status) return {{}, status};
        const auto cmp = compare();
        if (cmp == 0) return {{mid, true, true}, {}};
        if (cmp < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return {{low, false, true}, {}};
}
}  // namespace kirimasim::kernel
