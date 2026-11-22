// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/signal_abi.h"
namespace kirimasim::kernel::abi {
inline SlotBits bits(kirima_slot_bits b) { return {SlotId{b.slot}, b.element, b.lsb, b.width}; }
inline SlotPart part(kirima_slot_part p) {
    return {SlotId{p.slot}, p.first_elem, p.num_elems, p.lsb, p.width};
}
inline bool slot(kirima_context* c, std::uint32_t id) { return c && id < fromC(c).store().size(); }
inline bool valid(kirima_context* c, kirima_slot_part p) {
    if (!slot(c, p.slot)) return false;
    const auto& l = fromC(c).store().layout(SlotId{p.slot});
    if ((!p.num_elems && p.first_elem) || p.first_elem >= l.elements ||
        p.num_elems > l.elements - p.first_elem)
        return false;
    if (!p.width) return !p.lsb;
    if (l.domain == Domain::Handle || l.domain == Domain::Real) return !p.lsb && p.width == l.width;
    return p.lsb < l.width && p.width <= l.width - p.lsb;
}
inline bool valid(kirima_context* c, kirima_slot_bits b) {
    return valid(c, kirima_slot_part{b.slot, b.element, 1, b.lsb, b.width});
}
inline bool integral(kirima_context* c, std::uint32_t id) {
    if (!slot(c, id)) return false;
    const auto d = fromC(c).store().layout(SlotId{id}).domain;
    return d == Domain::Two || d == Domain::Four;
}
inline bool valid(kirima_context* c, kirima_wait_item w) {
    return valid(c, w.part) && w.edge <= static_cast<std::uint32_t>(Edge::Both);
}
inline WaitItem waitItem(kirima_wait_item w) {
    return {part(w.part), static_cast<Edge>(w.edge), w.guard, w.guard_frame};
}
inline bool valid(kirima_context* c, kirima_slot_bits b, kirima_domain_value v) {
    if (!v.words || !valid(c, b)) return false;
    const auto& l = fromC(c).store().layout(SlotId{b.slot});
    if (l.domain == Domain::Real || l.domain == Domain::Handle) return v.count == 1;
    const auto width = b.width ? b.width : l.width;
    return v.count == wordsFor(width) * (l.domain == Domain::Four ? 2U : 1U);
}
}  // namespace kirimasim::kernel::abi
