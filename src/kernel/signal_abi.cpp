// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/signal_abi.h"
#include "abi_boundary.h"
#include "abi_process_view.h"
#include "abi_slot_view.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
U32 write(kirima_context* c, kirima_slot_bits b, kirima_domain_value v, U32* changed) {
    if (!changed || !abi::valid(c, b, v) || fromC(c).strengths().registered(SlotId{b.slot}))
        return KIRIMA_VALUE_INVALID;
    *changed = fromC(c).scheduler().write(abi::bits(b), v.words);
    return KIRIMA_VALUE_OK;
}
U32 mark(kirima_context* c, U32 id, U32 net, U32 driven, U32 strength) {
    if (!abi::integral(c, id) || net > 1 || driven > 1 || strength > net ||
        (fromC(c).strengths().registered(SlotId{id}) && !strength))
        return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().markHeld(SlotId{id}, net, driven, strength);
    return KIRIMA_VALUE_OK;
}
U32 note(kirima_context* c, U32 id) {
    if (!abi::slot(c, id)) return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().noteDriven(SlotId{id});
    return KIRIMA_VALUE_OK;
}
bool holdTarget(kirima_context* c, U32 level, U32 slot) {
    return level <= KIRIMA_HOLD_FORCE && c && fromC(c).scheduler().hasHoldStorage(SlotId{slot});
}
U32 hold(kirima_context* c, U32 level, kirima_process p, kirima_slot_part range) {
    if (!abi::valid(c, p) || !abi::valid(c, range) || !holdTarget(c, level, range.slot))
        return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().hold(static_cast<Hold>(level), ProcessId{p.index}, abi::part(range));
    return KIRIMA_VALUE_OK;
}
U32 heldWrite(kirima_context* c, U32 level, kirima_process p, kirima_slot_bits b,
              kirima_domain_value v) {
    if (!abi::valid(c, p) || !abi::valid(c, b, v) || !holdTarget(c, level, b.slot))
        return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().writeHeld(static_cast<Hold>(level), ProcessId{p.index}, abi::bits(b),
                                   v.words);
    return KIRIMA_VALUE_OK;
}
U32 release(kirima_context* c, U32 level, kirima_slot_part range) {
    if (!abi::valid(c, range) || !holdTarget(c, level, range.slot)) return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().release(static_cast<Hold>(level), abi::part(range));
    return KIRIMA_VALUE_OK;
}
U32 holdsAny(kirima_context* c, U32 level, kirima_process p, U32* out) {
    if (!abi::valid(c, p) || !out || level > KIRIMA_HOLD_FORCE) return KIRIMA_VALUE_INVALID;
    *out = fromC(c).scheduler().holdsAny(static_cast<Hold>(level), ProcessId{p.index});
    return KIRIMA_VALUE_OK;
}
U32 driver(kirima_context* c, kirima_signal_driver* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    *out = {c, fromC(c).scheduler().addDelayedDriver(), 0};
    return KIRIMA_VALUE_OK;
}
U32 drive(kirima_context* c, kirima_signal_driver d, kirima_slot_bits b, kirima_domain_value v,
          std::uint64_t rise, std::uint64_t fall, std::uint64_t off,
          const kirima_side_update* inputs, U32 n) {
    if (!abi::valid(c, b, v) || !abi::integral(c, b.slot) || d.owner != c || d.reserved ||
        !fromC(c).scheduler().hasDelayedDriver(d.row) || (n && !inputs) ||
        fromC(c).strengths().registered(SlotId{b.slot}))
        return KIRIMA_VALUE_INVALID;
    std::vector<DelayedSideUpdate> sides;
    sides.reserve(n);
    for (auto side : std::span(inputs, n)) {
        const auto at = side.at;
        if (!abi::valid(c, at) || side.value > 1) return KIRIMA_VALUE_INVALID;
        const auto& l = fromC(c).store().layout(SlotId{at.slot});
        if (l.domain != Domain::Two || (at.width ? at.width : l.width) != 1 ||
            fromC(c).strengths().registered(SlotId{at.slot}))
            return KIRIMA_VALUE_INVALID;
        sides.push_back({abi::bits(at), side.value != 0});
    }
    fromC(c).scheduler().driveDelayed(d.row, abi::bits(b), v.words, rise, fall, off, sides);
    return KIRIMA_VALUE_OK;
}
U32 nba(kirima_context* c, kirima_process p, kirima_slot_bits b, kirima_domain_value v,
        std::uint64_t ticks) {
    if (!abi::valid(c, p) || !abi::valid(c, b, v) ||
        fromC(c).strengths().registered(SlotId{b.slot}))
        return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().scheduleNba(ProcessId{p.index}, abi::bits(b), v.words, ticks);
    return KIRIMA_VALUE_OK;
}
U32 stringNba(kirima_context* c, kirima_process p, kirima_slot_bits b, kirima_index i, U32 byte,
              std::uint64_t ticks) {
    if (!abi::valid(c, p) || !abi::valid(c, b) || b.width || b.lsb || byte > 255 || i.known > 1 ||
        i.reserved || fromC(c).store().layout(SlotId{b.slot}).domain != Domain::Handle)
        return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().scheduleNbaStringChar(ProcessId{p.index}, SlotId{b.slot}, b.element,
                                               i.known ? std::optional{i.value} : std::nullopt,
                                               static_cast<std::uint8_t>(byte), ticks);
    return KIRIMA_VALUE_OK;
}
U32 clockStatus(ClockingError e) {
    if (e == ClockingError::None) return KIRIMA_VALUE_OK;
    return e == ClockingError::ResourceLimit ? KIRIMA_VALUE_RESOURCE : KIRIMA_VALUE_INVALID;
}
U32 sampled(kirima_context* c, kirima_slot_bits source, U32 current, kirima_domain_value initial,
            const kirima_sample_history* previous) {
    if (!abi::slot(c, current) || !abi::valid(c, source) || (initial.count && !initial.words))
        return KIRIMA_VALUE_INVALID;
    std::optional<SampleHistory> history;
    if (previous) {
        if (!abi::valid(c, previous->clock) || !abi::slot(c, previous->previous))
            return KIRIMA_VALUE_INVALID;
        history = SampleHistory{abi::waitItem(previous->clock), SlotId{previous->previous}};
    }
    return clockStatus(fromC(c).scheduler().addSampledInput(
        abi::bits(source), SlotId{current}, {initial.words, initial.count}, history));
}
U32 clocking(kirima_context* c, kirima_wait_item clock, kirima_event_value e,
             const kirima_clocking_input* in, U32 n) {
    if (!abi::valid(c, clock) || e.owner != c || !e.bits || (e.bits >> 32) || (n && !in))
        return KIRIMA_VALUE_INVALID;
    auto& s = fromC(c).scheduler();
    const auto event = s.resolveEventValue({e.bits});
    if (!event) return KIRIMA_VALUE_INVALID;
    std::vector<ClockingInput> inputs;
    inputs.reserve(n);
    for (const auto i : std::span(in, n))
        inputs.push_back({abi::bits(i.source), abi::bits(i.target)});
    return clockStatus(s.addClockingBlock(abi::waitItem(clock), event.value, inputs));
}
U32 pending(kirima_context* c, kirima_pending_visitor fn, void* user, U32* completed) {
    if (!c || !fn || !completed) return KIRIMA_VALUE_INVALID;
    struct Visitor {
        kirima_pending_visitor fn;
        void* user;
    } cookie{fn, user};
    *completed = fromC(c).scheduler().visitPendingValues(
        [](void* p, SlotId slot, std::span<const std::uint64_t> words) noexcept {
            const auto& v = *static_cast<Visitor*>(p);
            return v.fn(v.user, slot.index(), words.data(), words.size()) != 0;
        },
        &cookie);
    return KIRIMA_VALUE_OK;
}
U32 prepareCounted(kirima_context* c, U32 process, U32 slot, kirima_counted_slot* token,
                   kirima_w4* initial) {
    if (!c || !token || !initial) return KIRIMA_COUNTED_SLOT_INVALID;
    auto& scheduler = fromC(c).scheduler();
    const ProcessId processId{process};
    W4 value{};
    const auto serial = scheduler.prepareCountedSlot(processId, SlotId{slot}, value);
    if (!serial)
        return abi::slot(c, slot) && scheduler.processSerial(processId)
                   ? KIRIMA_COUNTED_SLOT_DECLINED
                   : KIRIMA_COUNTED_SLOT_INVALID;
    *initial = {value.a, value.b};
    *token = {c, serial};
    return KIRIMA_COUNTED_SLOT_OK;
}
U32 commitCounted(kirima_context* c, kirima_counted_slot token, kirima_w4 value,
                  std::uint64_t writes, std::uint64_t changes) {
    if (!c || token.owner != c ||
        !fromC(c).scheduler().commitCountedSlot(token.serial, {value.a, value.b}, writes, changes))
        return KIRIMA_COUNTED_SLOT_INVALID;
    return KIRIMA_COUNTED_SLOT_OK;
}
}  // namespace
const void* signalService(std::uint32_t version) noexcept {
    static constexpr kirima_signal_api api{Boundary<write>::call,
                                           Boundary<mark>::call,
                                           Boundary<note>::call,
                                           Boundary<hold>::call,
                                           Boundary<heldWrite>::call,
                                           Boundary<release>::call,
                                           holdsAny,
                                           Boundary<driver>::call,
                                           Boundary<drive>::call,
                                           Boundary<nba>::call,
                                           Boundary<stringNba>::call,
                                           sampled,
                                           Boundary<clocking>::call,
                                           pending};
    static constexpr kirima_signal_api_v2 counted{api, prepareCounted,
                                                  Boundary<commitCounted>::call, nullptr};
    if (version == KIRIMA_SIGNAL_VERSION) return &api;
    return version == KIRIMA_SIGNAL_COUNTED_VERSION ? &counted : nullptr;
}
}  // namespace kirimasim::kernel
