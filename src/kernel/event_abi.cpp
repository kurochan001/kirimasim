// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/event_abi.h"
#include "abi_boundary.h"
#include "abi_process_view.h"
#include "service_tables.h"

namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
using Value = kirima_event_value;
using Error = ManagedEventError;
U32 code(Error e) { return static_cast<U32>(e); }
static_assert(static_cast<U32>(Error::ResourceLimit) == KIRIMA_EVENT_RESOURCE);
bool valid(kirima_context* c, Value v) { return c && (v.bits ? v.owner == c : !v.owner); }
U32 permanent(kirima_context* c, Value* out) {
    if (!c || !out) return KIRIMA_EVENT_ABI;
    *out = {c, EventValue::legacy(fromC(c).scheduler().addEvent()).bits};
    return KIRIMA_EVENT_OK;
}
template <class Acquire> U32 acquire(kirima_context* c, kirima_pinned_event* out, Acquire fn) {
    auto& resources = fromC(c).modelResources();
    auto slot = resources.reserve<ManagedEventPin>();
    auto r = fn();
    if (!r || !r.value) {
        resources.release<ManagedEventPin>(slot.id);
        if (r) *out = {};
    } else {
        slot.entry = std::move(r.value);
        *out = {{c, slot.entry.token().bits}, {c, slot.id}};
    }
    return code(r.error);
}
U32 create(kirima_context* c, kirima_pinned_event* out) {
    if (!c || !out) return KIRIMA_EVENT_ABI;
    return acquire(c, out, [&] { return fromC(c).managedEvents().create(); });
}
U32 retain(kirima_context* c, Value v, kirima_pinned_event* out) {
    if (!valid(c, v) || !out) return KIRIMA_EVENT_ABI;
    return acquire(c, out,
                   [&] { return fromC(c).managedEvents().retain(ManagedEventToken{v.bits}); });
}
U32 release(kirima_context* c, kirima_resource pin) {
    if (!c || pin.owner != c || !fromC(c).modelResources().release<ManagedEventPin>(pin.serial))
        return KIRIMA_EVENT_ABI;
    return KIRIMA_EVENT_OK;
}
U32 resolve(kirima_context* c, Value v, U32* out) {
    if (!valid(c, v) || !out) return KIRIMA_EVENT_ABI;
    const auto r = fromC(c).scheduler().resolveEventValue({v.bits});
    if (r) *out = r.value.index();
    return code(r.error);
}
U32 trigger(kirima_context* c, Value v) {
    if (!valid(c, v)) return KIRIMA_EVENT_ABI;
    return code(fromC(c).scheduler().triggerValue({v.bits}));
}
U32 triggerNb(kirima_context* c, kirima_process p, Value v, std::uint64_t ticks, U32 overflow) {
    if (!valid(c, v) || !abi::valid(c, p) || overflow > 1) return KIRIMA_EVENT_ABI;
    return code(fromC(c).scheduler().triggerNbValue(
        ProcessId{p.index}, {v.bits}, overflow ? std::nullopt : std::optional{ticks}));
}
U32 triggered(kirima_context* c, Value v, U32* out) {
    if (!valid(c, v) || !out) return KIRIMA_EVENT_ABI;
    const auto r = fromC(c).scheduler().triggeredValue({v.bits});
    if (r) *out = r.value;
    return code(r.error);
}
U32 waitNamed(kirima_context* c, kirima_process p, Value v, U32 flush) {
    if (!abi::valid(c, p) || !valid(c, v) || flush > 1) return KIRIMA_EVENT_ABI;
    return code(fromC(c).scheduler().waitNamedValue(ProcessId{p.index}, {v.bits}, flush != 0));
}
U32 waitMany(kirima_context* c, kirima_process p, const kirima_event_wait* input, U32 n) {
    if (!abi::valid(c, p) || (n && !input)) return KIRIMA_EVENT_ABI;
    std::vector<NamedValueWaitItem> items;
    items.reserve(n);
    for (const auto& w : std::span(input, n)) {
        if (!valid(c, w.value) || w.flush_point > 1 || w.reserved) return KIRIMA_EVENT_ABI;
        items.push_back({{w.value.bits}, w.guard, w.frame, w.flush_point != 0});
    }
    return code(fromC(c).scheduler().waitAnyValues(ProcessId{p.index}, {}, {}, items));
}
U32 collect(kirima_context* c, const Value* input, U32 n) {
    if (!c || (n && !input)) return KIRIMA_EVENT_ABI;
    auto& ctx = fromC(c);
    if (!ctx.scheduler().permitsEventCollection()) return KIRIMA_EVENT_BUSY;
    const std::span model(input, n);
    for (const auto v : model) {
        if (!valid(c, v)) return KIRIMA_EVENT_ABI;
        if (v.bits && !(v.bits >> 32)) {
            const auto r = ctx.scheduler().resolveEventValue({v.bits});
            if (!r) return code(r.error);
        }
    }
    const ManagedEventRootProvider all[] = {
        {&model,
         [](const void* raw, ManagedEventRoots& roots) noexcept {
             for (const auto v : *static_cast<const std::span<const Value>*>(raw)) {
                 if (!(v.bits >> 32)) continue;
                 const auto e = roots.add({v.bits});
                 if (e != Error::None) return e;
             }
             return Error::None;
         }},
        ctx.activations().eventRootProvider()};
    auto& events = ctx.managedEvents();
    events.rootsChanged();
    const auto e = events.collect(all);
    if (e == Error::None) ctx.activations().acknowledgeEventRoots();
    return code(e);
}
U32 stats(kirima_context* c, kirima_event_stats* out) {
    if (!c || !out) return KIRIMA_EVENT_ABI;
    const auto& e = fromC(c).managedEvents();
    *out = {e.liveObjects(), e.allocatedObjects(), e.retiredObjects(), e.hasReusableObject(),
            e.collectionEligible()};
    return KIRIMA_EVENT_OK;
}
template <auto F> constexpr auto guard = Boundary<F, KIRIMA_EVENT_RESOURCE>::call;
}  // namespace
const void* eventService() noexcept {
    static constexpr kirima_event_api table{
        guard<permanent>, guard<create>, guard<retain>, release,         resolve, trigger,
        triggerNb,        triggered,     waitNamed,     guard<waitMany>, collect, stats};
    return &table;
}
}  // namespace kirimasim::kernel
