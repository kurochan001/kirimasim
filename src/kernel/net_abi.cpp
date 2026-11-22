// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/net_abi.h"
#include "abi_boundary.h"
#include "abi_process_view.h"
#include "abi_slot_view.h"
#include "abi_views.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
U32 code(StrengthError e) { return static_cast<U32>(e); }
static_assert(static_cast<U32>(StrengthError::Interrupted) == KIRIMA_NET_INTERRUPTED);
static_assert(static_cast<U32>(NetKind::Uwire) == KIRIMA_UWIRE);
StrengthRange range(kirima_strength r) { return {r.low, r.high}; }
U32 enable(kirima_context* c, U32 slot, const kirima_strength* src, U32 n) {
    if (!c || (n && !src)) return KIRIMA_NET_SHAPE;
    std::vector<StrengthRange> v;
    v.reserve(n);
    for (auto s : std::span(src, n))
        v.push_back(range(s));
    return code(fromC(c).scheduler().enableStrengths(SlotId{slot}, v));
}
U32 configure(kirima_context* c, const kirima_net_node* ns, U32 nn, const kirima_net_origin* os,
              U32 no, const kirima_net_edge* es, U32 ne) {
    if (!c || (nn && !ns) || (no && !os) || (ne && !es)) return KIRIMA_NET_SHAPE;
    std::vector<StrengthNodeSpec> nodes;
    std::vector<StrengthOriginSpec> origins;
    std::vector<StrengthEdgeSpec> edges;
    nodes.reserve(nn);
    origins.reserve(no);
    edges.reserve(ne);
    for (const auto& n : std::span(ns, nn)) {
        if (n.kind > KIRIMA_UWIRE) return KIRIMA_NET_VALUE;
        nodes.push_back({abi::bits(n.target), static_cast<NetKind>(n.kind), n.component});
    }
    for (auto o : std::span(os, no))
        origins.push_back({o.node, range(o.initial)});
    for (auto e : std::span(es, ne)) {
        if (e.transfer > KIRIMA_RESISTIVE || e.initial > KIRIMA_SWITCH_UNCERTAIN)
            return KIRIMA_NET_VALUE;
        edges.push_back({e.first, e.second, static_cast<StrengthTransfer>(e.transfer),
                         static_cast<StrengthConduction>(e.initial)});
    }
    return code(fromC(c).scheduler().configureStrengthDrivers(nodes, origins, edges));
}
U32 start(kirima_context* c) {
    return c ? code(fromC(c).scheduler().startStrengthDrivers()) : KIRIMA_NET_TARGET;
}
U32 drive(kirima_context* c, U32 origin, kirima_strength r) {
    return c ? code(fromC(c).scheduler().driveStrength(origin, range(r))) : KIRIMA_NET_TARGET;
}
U32 driveDelayed(kirima_context* c, U32 origin, kirima_strength r, std::uint64_t rise,
                 std::uint64_t fall, std::uint64_t off) {
    return c ? code(fromC(c).scheduler().driveStrengthDelayed(origin, range(r), rise, fall, off))
             : KIRIMA_NET_TARGET;
}
U32 edge(kirima_context* c, U32 id, U32 value) {
    if (!c || value > KIRIMA_SWITCH_UNCERTAIN) return KIRIMA_NET_VALUE;
    return code(fromC(c).scheduler().setStrengthEdge(id, static_cast<StrengthConduction>(value)));
}
U32 edgeDelayed(kirima_context* c, U32 id, U32 v, std::uint64_t on, std::uint64_t off) {
    if (!c || v > KIRIMA_SWITCH_UNCERTAIN) return KIRIMA_NET_VALUE;
    return code(fromC(c).scheduler().setStrengthEdgeDelayed(id, static_cast<StrengthConduction>(v),
                                                            on, off));
}
U32 publication(StrengthPublication r, kirima_publication* out) {
    *out = {r.committed, r.changed};
    return code(r.error);
}
U32 forces(kirima_context* c, const kirima_net_force* src, U32 n, kirima_publication* out) {
    if (!c || !out || (n && !src)) return KIRIMA_NET_SHAPE;
    std::vector<StrengthForceUpdate> v;
    v.reserve(n);
    for (auto f : std::span(src, n)) {
        if (f.force > 1) return KIRIMA_NET_VALUE;
        v.push_back({f.node, f.force ? std::optional{f.value} : std::nullopt});
    }
    return publication(fromC(c).scheduler().setStrengthForces(v), out);
}
U32 write(kirima_context* c, kirima_slot_bits b, kirima_words_in v, U32 kind,
          kirima_publication* out) {
    if (!c || !out || (v.count && !v.data) || kind > KIRIMA_NET_RELEASE) return KIRIMA_NET_SHAPE;
    return publication(fromC(c).scheduler().writeNet(abi::bits(b), {v.data, v.count},
                                                     static_cast<NetWriteKind>(kind)),
                       out);
}
U32 read(kirima_context* c, kirima_slot_bits window, kirima_index elem, kirima_index pos, U32 width,
         kirima_words_out out) {
    if (!c || !out.data || elem.reserved || pos.reserved || elem.known > 1 || pos.known > 1)
        return KIRIMA_NET_SHAPE;
    std::optional<U32> element;
    if (elem.known && elem.value >= 0 && std::uint64_t(elem.value) <= UINT32_MAX)
        element = static_cast<U32>(elem.value);
    return code(fromC(c).strengths().readSelectedPacked(
        abi::bits(window), element, pos.known ? std::optional{pos.value} : std::nullopt, width,
        {out.data, out.count}));
}
U32 wait(kirima_context* c, kirima_process p, const kirima_wait_item* ws, U32 nw,
         const kirima_slot_part* ss, U32 ns) {
    if (!abi::valid(c, p) || (nw && !ws) || (ns && !ss)) return KIRIMA_NET_TARGET;
    std::vector<WaitItem> values;
    std::vector<SlotPart> ranges;
    values.reserve(nw);
    ranges.reserve(ns);
    for (auto w : std::span(ws, nw)) {
        if (!abi::valid(c, w)) return KIRIMA_NET_TARGET;
        values.push_back(abi::waitItem(w));
    }
    for (auto s : std::span(ss, ns)) {
        if (!abi::valid(c, s)) return KIRIMA_NET_TARGET;
        ranges.push_back(abi::part(s));
    }
    return code(fromC(c).scheduler().waitEventStrength(ProcessId{p.index}, values, ranges));
}
U32 track(kirima_context* c, kirima_slot_bits b, U32* out) {
    if (!out || !abi::valid(c, b)) return KIRIMA_NET_TARGET;
    *out = fromC(c).scheduler().trackDisplayStrength(abi::bits(b));
    return KIRIMA_NET_OK;
}
U32 paths(kirima_context* c, const kirima_path* src, U32 n, kirima_path_evaluator* eval) {
    if (!c || (n && !src) || (eval && !eval->read)) return KIRIMA_NET_SHAPE;
    std::vector<ModulePathSpec> values;
    values.reserve(n);
    for (const auto& p : std::span(src, n)) {
        if (p.edge > KIRIMA_EDGE_NEG || p.enable > KIRIMA_PATH_IFNONE || (p.overflow_mask >> 12))
            return KIRIMA_NET_VALUE;
        ModulePathSpec s;
        s.source = abi::bits(p.source);
        s.origin = p.origin;
        s.edge = static_cast<ModulePathEdge>(p.edge);
        for (U32 i = 0; i < s.delays.size(); ++i)
            if (!(p.overflow_mask & (1U << i))) s.delays[i] = p.ticks[i];
        s.enable = static_cast<ModulePathEnable>(p.enable);
        s.condition = p.condition;
        s.group = p.group;
        s.sourceIdentity = p.source_identity;
        s.destinationIdentity = p.destination_identity;
        values.push_back(s);
    }
    ModulePathEvaluator native;
    if (eval)
        native = {[](void* raw, U32 id, W4& out) noexcept {
                      auto& e = *static_cast<kirima_path_evaluator*>(raw);
                      return e.read(e.frame, id, &out) != 0;
                  },
                  eval, eval->count};
    return code(fromC(c).scheduler().configureModulePaths(values, native));
}
U32 detach(kirima_context* c, kirima_path_evaluator* eval) {
    if (!c || !eval) return KIRIMA_NET_TARGET;
    fromC(c).scheduler().detachModulePathEvaluator(eval);
    return KIRIMA_NET_OK;
}
bool validUdpText(std::string_view text, bool sequential) {
    constexpr std::string_view levels = "01x?b";
    while (!text.empty()) {
        if (text.front() == '(') {
            if (!sequential || text.size() < 4 || text[3] != ')' ||
                levels.find(text[1]) == levels.npos || levels.find(text[2]) == levels.npos)
                return false;
            text.remove_prefix(4);
        } else {
            if (levels.find(text.front()) == levels.npos &&
                (!sequential || std::string_view("rfpn*").find(text.front()) == levels.npos))
                return false;
            text.remove_prefix(1);
        }
    }
    return true;
}
U32 compile(kirima_context* c, U32 inputs, const kirima_udp_row* src, U32 n, kirima_resource* out) {
    if (!c || !out || !inputs || (n && !src)) return KIRIMA_NET_SHAPE;
    UdpTable table;
    table.inputs = inputs;
    table.rows.reserve(n);
    for (const auto& r : std::span(src, n)) {
        if (!abi::valid(r.inputs) || r.state > 127 || r.output > 127) return KIRIMA_NET_VALUE;
        const bool sequential = src[0].state != 0;
        if ((r.state != 0) != sequential ||
            (sequential && std::string_view("01x?b").find(static_cast<char>(r.state)) ==
                               std::string_view::npos) ||
            (std::string_view("01x").find(static_cast<char>(r.output)) == std::string_view::npos &&
             !(sequential && r.output == '-')) ||
            !validUdpText(abi::view(r.inputs), sequential))
            return KIRIMA_NET_VALUE;
        auto row = compileUdpRow(abi::view(r.inputs), static_cast<char>(r.state),
                                 static_cast<char>(r.output));
        if (row.masks.size() != inputs || row.edge < -1) return KIRIMA_NET_SHAPE;
        table.rows.push_back(std::move(row));
    }
    auto ticket = fromC(c).modelResources().reserve<UdpTable>();
    ticket.entry = std::move(table);
    *out = {c, ticket.id};
    return KIRIMA_NET_OK;
}
const UdpTable* udp(kirima_context* c, kirima_resource h) {
    return c && h.owner == c ? fromC(c).modelResources().get<UdpTable>(h.serial) : nullptr;
}
auto reader(kirima_udp_source s) {
    return [s](unsigned port) {
        return static_cast<std::uint8_t>(std::min<U32>(kUdpX, s.read(s.frame, port)));
    };
}
U32 output(kirima_context* c, kirima_resource h, kirima_udp_source s, U32* out) {
    const auto* table = udp(c, h);
    if (!table || !out || !s.read) return KIRIMA_NET_TARGET;
    *out = udpOutputRead(*table, reader(s));
    return KIRIMA_NET_OK;
}
U32 step(kirima_context* c, kirima_resource h, U32 state, kirima_udp_source previous,
         kirima_udp_source current, U32* out) {
    const auto* table = udp(c, h);
    if (!table || !out || !previous.read || !current.read || state > 3) return KIRIMA_NET_TARGET;
    *out = udpStepRead(*table, static_cast<std::uint8_t>(state), reader(previous), reader(current));
    return KIRIMA_NET_OK;
}
U32 release(kirima_context* c, kirima_resource h) {
    return c && h.owner == c && fromC(c).modelResources().release<UdpTable>(h.serial)
               ? KIRIMA_NET_OK
               : KIRIMA_NET_TARGET;
}
template <auto F> constexpr auto guard = Boundary<F, KIRIMA_NET_RESOURCE>::call;
}  // namespace
const void* netService() noexcept {
    static constexpr kirima_net_api table{
        guard<enable>, guard<configure>, start,          drive,  driveDelayed, edge,
        edgeDelayed,   guard<forces>,    write,          read,   guard<wait>,  track,
        guard<paths>,  detach,           guard<compile>, output, step,         release};
    return &table;
}
}  // namespace kirimasim::kernel
