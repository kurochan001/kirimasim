// The storage of slots: one word array per domain (SIR_SPEC.md 3.2).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <vector>

#include "kirimasim/kernel/id.h"

namespace kirimasim::kernel {

struct SlotTag;
using SlotId = Id<SlotTag>;

/// Storage domain of a slot (SIR_SPEC.md 3.2).
/// Bits [lsb, lsb + width) of element `elem` of a slot (width 0: the
/// whole element). The writers take one of these rather than three plain
/// numbers in a row, which could change places unseen; callers name the
/// fields.
struct SlotBits {
    SlotId slot;
    std::uint32_t elem = 0;
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;
};

enum class Domain : std::uint8_t { Two, Four, Real, Handle };

/// The value every bit of a slot starts with: 0 (two-state, real, handle;
/// four-state too when asked), 1, x or z (four-state only).
enum class Init : std::uint8_t { Zero, One, X, Z };

/// Where a slot lives. An element of a four-state slot takes
/// 2 * wordsFor(width) words; of a two-state slot wordsFor(width); a real
/// or handle element takes one word.
struct SlotLayout {
    Domain domain = Domain::Four;
    std::uint32_t width = 1;      ///< Bits of one element (64 for real and handle).
    std::uint32_t elements = 1;   ///< 1 unless the slot is an unpacked array.
    std::uint32_t elemWords = 2;  ///< Words of one element.
    std::uint64_t offset = 0;     ///< First word in the domain's array.
};

/// The values of all slots, owned by a Context.
class Store {
public:
    /// Adds a slot and sets every element to its initial value.
    SlotId add(Domain domain, std::uint32_t width, std::uint32_t elements, Init init);

    [[nodiscard]] const SlotLayout& layout(SlotId s) const { return slots_[s.index()]; }
    [[nodiscard]] std::uint32_t size() const { return static_cast<std::uint32_t>(slots_.size()); }

    /// The words of element e of slot s.
    std::uint64_t* element(SlotId s, std::uint32_t e) {
        const SlotLayout& l = slots_[s.index()];
        return words(l.domain).data() + l.offset + std::uint64_t{e} * l.elemWords;
    }
    [[nodiscard]] const std::uint64_t* element(SlotId s, std::uint32_t e) const {
        const SlotLayout& l = slots_[s.index()];
        return words(l.domain).data() + l.offset + std::uint64_t{e} * l.elemWords;
    }

private:
    std::vector<SlotLayout> slots_;
    std::vector<std::uint64_t> domains_[4];

    std::vector<std::uint64_t>& words(Domain d) { return domains_[static_cast<int>(d)]; }
    [[nodiscard]] const std::vector<std::uint64_t>& words(Domain d) const {
        return domains_[static_cast<int>(d)];
    }
};

}  // namespace kirimasim::kernel
