// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/observation_abi.h"
#include "abi_boundary.h"
#include "abi_file_targets.h"
#include "abi_process_view.h"
#include "abi_slot_view.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
constexpr U32 ok = KIRIMA_OBSERVE_OK, invalid = KIRIMA_OBSERVE_INVALID;
constexpr U32 token = KIRIMA_OBSERVE_TOKEN, resource = KIRIMA_OBSERVE_RESOURCE;
U32 status(NbaError e) {
    switch (e) {
        case NbaError::None: return ok;
        case NbaError::InvalidTicket: return token;
        case NbaError::InvalidPhase: return KIRIMA_OBSERVE_PHASE;
        case NbaError::ResourceLimit: return resource;
        default: return invalid;
    }
}
bool validFilter(kirima_context* c, const kirima_heap_filter* filter) {
    return !filter || (filter->owner == c && filter->test);
}
HeapResult<bool> filterRead(const void* ptr, HeapRoot root) noexcept {
    const auto& f = *static_cast<const kirima_heap_filter*>(ptr);
    U32 keep = 0;
    const U32 err = f.test(f.frame, {f.owner, root.bits, static_cast<U32>(root.kind), 0}, &keep);
    if (err > KIRIMA_HEAP_RESOURCE || keep > 1) return {{}, {HeapError::InvalidHandle}};
    return {keep != 0, {static_cast<HeapError>(err)}};
}
bool validEvent(kirima_context* c, kirima_event_value v) {
    return v.bits ? v.owner == c : !v.owner;
}
bool watch(kirima_context* c, const kirima_watch& w, MonitorSpec& out) {
    if ((w.slot_count && !w.slots) || (w.event_count && !w.events) ||
        (w.extent_count && !w.capture_extents) || (w.part_count && !w.static_parts) ||
        (w.flags & ~31U) || w.edge > KIRIMA_EDGE_BOTH || !validFilter(c, w.filter))
        return false;
    if (w.implicit_container) {
        const auto b = *w.implicit_container;
        if (w.flags || w.eval || w.guard || w.filter || w.edge || w.slot_count || w.event_count ||
            w.extent_count || w.part_count || !abi::slot(c, b.slot) ||
            !fromC(c).heap().describeType(HeapTypeId{b.type}))
            return false;
        const auto& l = fromC(c).store().layout(SlotId{b.slot});
        if (l.domain != Domain::Handle || l.width != 64 || l.elements != 1) return false;
        out.implicitContainer = ImplicitContainerBinding{SlotId{b.slot}, HeapTypeId{b.type}};
        return true;
    }
    if (!w.eval || !w.width || w.domain > KIRIMA_DOMAIN_HANDLE ||
        (w.domain == KIRIMA_DOMAIN_REAL && w.width != 32 && w.width != 64) ||
        (w.domain == KIRIMA_DOMAIN_HANDLE && w.width != 64) ||
        ((w.flags & KIRIMA_WATCH_STRING) && w.domain != KIRIMA_DOMAIN_HANDLE) ||
        (w.filter && !(w.flags & KIRIMA_WATCH_HEAP)))
        return false;
    out.domain = static_cast<Domain>(w.domain);
    out.width = w.width;
    out.edge = static_cast<Edge>(w.edge);
    out.untilTrue = w.flags & KIRIMA_WATCH_UNTIL;
    out.heap = w.flags & KIRIMA_WATCH_HEAP;
    out.captureEvents = w.flags & KIRIMA_WATCH_EVENTS;
    out.activation.capture = w.flags & KIRIMA_WATCH_AUTOMATIC;
    out.stringValue = w.flags & KIRIMA_WATCH_STRING;
    out.eval = w.eval;
    out.frame = w.frame;
    out.guard = w.guard;
    out.guardFrame = w.guard_frame;
    if (w.filter) {
        out.heapReadFilter = filterRead;
        out.heapReadFrame = w.filter;
    }
    for (auto p : std::span(w.slots, w.slot_count)) {
        if (!abi::valid(c, p)) return false;
        out.slots.push_back(abi::part(p));
    }
    for (auto e : std::span(w.events, w.event_count)) {
        if (!validEvent(c, e)) return false;
        out.eventValues.push_back({e.bits});
    }
    auto& refs = fromC(c).references();
    for (auto r : std::span(w.capture_extents, w.extent_count)) {
        if (r.owner != c) return false;
        auto part = refs.part(r.ticket);
        if (!part) return false;
        out.activation.bindings.push_back(part.value.binding);
    }
    for (auto r : std::span(w.static_parts, w.part_count)) {
        if (r.owner != c) return false;
        auto part = refs.part(r.ticket);
        if (!part) return false;
        out.activation.parts.push_back(part.value);
    }
    return true;
}
bool observer(kirima_context* c, const kirima_observer& src, NbaEventItem& out) {
    if (src.reserved) return false;
    switch (src.kind) {
        case KIRIMA_OBSERVE_SLOT:
            if (!abi::valid(c, src.item.slot)) return false;
            out = abi::waitItem(src.item.slot);
            return true;
        case KIRIMA_OBSERVE_EVENT: {
            const auto& n = src.item.event;
            if (!validEvent(c, n.value) || n.reserved || n.flush_point > 1) return false;
            out = NamedValueWaitItem{{n.value.bits}, n.guard, n.frame, n.flush_point != 0};
            return true;
        }
        case KIRIMA_OBSERVE_EXPRESSION:
            return src.item.watch && watch(c, *src.item.watch, out.emplace<MonitorSpec>());
        default: return false;
    }
}
U32 prepare(kirima_context* c, const kirima_wait_item* src, U32 count, kirima_resource* out) {
    if (!c || !out || (count && !src)) return invalid;
    std::vector<WaitItem> items;
    items.reserve(count);
    for (auto w : std::span(src, count)) {
        if (!abi::valid(c, w) || w.guard) return invalid;
        items.push_back(abi::waitItem(w));
    }
    auto& resources = fromC(c).modelResources();
    auto t = resources.reserve<ModelPreparedWait>();
    // A failed preparation must not publish or leave an inaccessible ticket.
    struct Rollback {
        ModelResources& resources;
        std::uint64_t id;
        bool committed = false;
        ~Rollback() {
            if (!committed) resources.release<ModelPreparedWait>(id);
        }
    } rollback{resources, t.id};
    auto w = fromC(c).scheduler().prepareWait(items);
    if (w.first == kNone) return resource;
    t.entry = {w.first, w.count};
    *out = {c, t.id};
    rollback.committed = true;
    return ok;
}
U32 waitPrepared(kirima_context* c, kirima_process p, kirima_resource r) {
    if (!abi::valid(c, p) || r.owner != c) return token;
    auto* w = fromC(c).modelResources().get<ModelPreparedWait>(r.serial);
    if (!w) return token;
    fromC(c).scheduler().waitPrepared(ProcessId{p.index}, {w->first, w->count});
    return ok;
}
U32 releasePrepared(kirima_context* c, kirima_resource r) {
    return c && r.owner == c && fromC(c).modelResources().release<ModelPreparedWait>(r.serial)
               ? ok
               : token;
}
U32 mixed(kirima_context* c, kirima_process p, const kirima_observer* src, U32 count) {
    if (!abi::valid(c, p) || (count && !src)) return invalid;
    std::vector<WaitItem> slots;
    std::vector<NamedValueWaitItem> names;
    std::vector<MonitorSpec> watches;
    for (const auto& item : std::span(src, count)) {
        NbaEventItem x;
        if (!observer(c, item, x)) return invalid;
        if (auto* w = std::get_if<WaitItem>(&x))
            slots.push_back(*w);
        else if (auto* n = std::get_if<NamedValueWaitItem>(&x))
            names.push_back(*n);
        else
            watches.push_back(std::move(*std::get_if<MonitorSpec>(&x)));
    }
    const auto e = fromC(c).scheduler().waitAnyValues(ProcessId{p.index}, slots, watches, names);
    return e == ManagedEventError::None            ? ok
           : e == ManagedEventError::ResourceLimit ? resource
                                                   : token;
}
constexpr U32 payloadKind = 1, controlKind = 2;
bool validTicket(kirima_context* c, kirima_nba_ticket t, U32 kind) {
    return c && t.owner == c && t.kind == kind && t.serial;
}
U32 capture(kirima_context* c, kirima_process p, const kirima_nba_write* src, U32 n,
            kirima_nba_ticket* out) {
    if (!abi::valid(c, p) || !out || (n && !src)) return invalid;
    std::vector<NbaWrite> writes;
    writes.reserve(n);
    for (const auto& w : std::span(src, n)) {
        if (w.character > 1 || (w.value.count && !w.value.words)) return invalid;
        NbaWrite value{abi::bits(w.target), {w.value.words, w.value.count}};
        if (w.character) {
            if (w.byte > 255 || w.index.reserved || w.index.known > 1) return invalid;
            value.character =
                NbaStringChar{w.index.known ? std::optional{w.index.value} : std::nullopt,
                              static_cast<std::uint8_t>(w.byte)};
        } else if (!abi::valid(c, w.target, w.value))
            return invalid;
        writes.push_back(value);
    }
    auto r = fromC(c).scheduler().captureNba(ProcessId{p.index}, writes);
    if (r) *out = {c, r.ticket.serial, r.ticket.index, payloadKind};
    return status(r.error);
}
NbaRepeat repeat(kirima_nba_repeat r) {
    if ((r.value.count && !r.value.words) || r.is_signed > 1 || r.four_state > 1)
        return {0, false, false, {}};
    return {r.value.width, r.is_signed != 0, r.four_state != 0, {r.value.words, r.value.count}};
}
U32 classify(kirima_nba_repeat r, U32* out) {
    if (!out) return invalid;
    *out = static_cast<U32>(classifyNbaRepeat(repeat(r)));
    return ok;
}
U32 ready(kirima_context* c, kirima_nba_ticket t) {
    return validTicket(c, t, payloadKind) ? status(fromC(c).scheduler().readyNba({t.row, t.serial}))
                                          : token;
}
U32 discard(kirima_context* c, kirima_nba_ticket t) {
    if (validTicket(c, t, payloadKind))
        return status(fromC(c).scheduler().discardNba({t.row, t.serial}));
    if (validTicket(c, t, controlKind))
        return status(fromC(c).scheduler().discardNbaEvent({t.row, t.serial}));
    return token;
}
U32 bind(kirima_context* c, kirima_process p, kirima_nba_ticket t, kirima_nba_repeat r,
         const kirima_observer* src, U32 n, kirima_cleanup* cleanup, kirima_nba_ticket* out) {
    if (!abi::valid(c, p) || !out || !validTicket(c, t, payloadKind) || (cleanup && !cleanup->call))
        return invalid;
    auto count = repeat(r);
    const auto action = classifyNbaRepeat(count);
    if (action == NbaCountAction::Invalid) return invalid;
    std::vector<NbaEventItem> items;
    if (action == NbaCountAction::Bind) {
        if (n && !src) return invalid;
        items.reserve(n);
        for (const auto& i : std::span(src, n))
            if (!observer(c, i, items.emplace_back())) return invalid;
    }
    SafePointHook release;
    if (cleanup)
        release = {[](void* raw) noexcept {
                       auto* h = static_cast<kirima_cleanup*>(raw);
                       h->call(h->frame);
                   },
                   cleanup};
    const auto reg = fromC(c).scheduler().registerNbaEvent(ProcessId{p.index}, {t.row, t.serial},
                                                           count, items, release);
    if (reg)
        *out = reg.ticket.serial
                   ? kirima_nba_ticket{c, reg.ticket.serial, reg.ticket.index, controlKind}
                   : kirima_nba_ticket{};
    return status(reg.error);
}
U32 cancel(kirima_context* c) {
    if (!c) return invalid;
    fromC(c).scheduler().cancelNbaEvents();
    return ok;
}
U32 display(kirima_context* c, kirima_process p, const kirima_slot_part* src, U32 n,
            const kirima_file_targets* files, U32 heap, U32 strength,
            const kirima_heap_filter* filter) {
    if (!abi::valid(c, p) || (n && !src) || heap > 1 || strength > 1 || (filter && !heap) ||
        !validFilter(c, filter))
        return invalid;
    std::vector<SlotPart> parts;
    parts.reserve(n);
    for (auto x : std::span(src, n)) {
        if (!abi::valid(c, x)) return invalid;
        parts.push_back(abi::part(x));
    }
    DisplayReadSpec reads{heap != 0, filter ? filterRead : nullptr, filter, strength != 0};
    auto& s = fromC(c).scheduler();
    if (files) {
        FileTargets targets;
        if (!abi::restore(c, files, targets)) return invalid;
        s.addFileMonitor(ProcessId{p.index}, parts, std::move(targets), reads);
    } else
        s.setMonitor(ProcessId{p.index}, parts, reads);
    return ok;
}
U32 fileStrobe(kirima_context* c, kirima_process p, const kirima_file_targets* f) {
    FileTargets targets;
    if (!abi::valid(c, p) || !abi::restore(c, f, targets)) return invalid;
    fromC(c).scheduler().strobe(ProcessId{p.index}, std::move(targets));
    return ok;
}
U32 detach(kirima_context* c, kirima_process p) {
    if (!abi::valid(c, p)) return token;
    fromC(c).scheduler().detachDisplayMonitor(ProcessId{p.index});
    return ok;
}
U32 monitoring(kirima_context* c, U32 on) {
    if (!c || on > 1) return invalid;
    fromC(c).scheduler().setMonitoring(on != 0);
    return ok;
}
U32 flush(kirima_context* c) {
    if (!c) return invalid;
    fromC(c).scheduler().flushHeapChanges();
    return ok;
}
U32 limit(kirima_context* c, U32* limited, kirima_process* out) {
    if (!c || !limited || !out) return invalid;
    auto& s = fromC(c).scheduler();
    auto id = s.failedHeapMonitorProcess();
    auto serial = s.processSerial(id);
    *limited = s.heapMonitorLimited();
    *out = serial ? kirima_process{c, *serial, id.index(), 0} : kirima_process{};
    return ok;
}
U32 visit(kirima_context* c, kirima_string_mark mark, void* user, U32* completed) {
    if (!c || !mark || !completed) return invalid;
    struct Call {
        kirima_string_mark fn;
        void* frame;
    } call{mark, user};
    const auto cb = [](void* raw, std::uint64_t h) noexcept {
        auto* v = static_cast<Call*>(raw);
        return v->fn(v->frame, h) != 0;
    };
    auto& ctx = fromC(c);
    *completed = ctx.activations().visitStringRoots(cb, &call) &&
                 ctx.scheduler().visitMonitorStrings(cb, &call);
    return ok;
}
template <auto F> constexpr auto guard = Boundary<F, resource>::call;
}  // namespace
const void* observationService() noexcept {
    static constexpr kirima_observation_api table{guard<prepare>,
                                                  guard<waitPrepared>,
                                                  releasePrepared,
                                                  guard<mixed>,
                                                  guard<capture>,
                                                  classify,
                                                  ready,
                                                  discard,
                                                  guard<bind>,
                                                  cancel,
                                                  guard<display>,
                                                  guard<fileStrobe>,
                                                  detach,
                                                  guard<monitoring>,
                                                  guard<flush>,
                                                  limit,
                                                  visit};
    return &table;
}
}  // namespace kirimasim::kernel
