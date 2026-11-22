// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/analysis/race.h"
#include "island_candidates.h"

#include <map>
#include <set>
#include <tuple>

namespace kirimasim::analysis {
namespace {
using sir::Op;
using sir::Operand;
struct Clock {
    sir::SlotId slot;
    std::uint32_t element, bit;
    sir::Edge edge;
    auto operator<=>(const Clock&) const = default;
};
struct Access {
    sir::ProcessId process;
    sir::SlotRange range;
    sir::SourceLoc loc;
};
struct Accesses {
    std::vector<Access> reads, writes;
};
using Sites = std::map<std::pair<Clock, sir::SlotId>, Accesses>;

void addEdges(std::vector<Clock>& clocks, const sir::Design& d, sir::SlotRange range,
              sir::Edge edge) {
    if (edge == sir::Edge::Any || range.slot == sir::kNone) return;
    const auto physical = sir::physicalRange(d, range);
    if (edge != sir::Edge::Neg)
        clocks.push_back({physical.slot, physical.firstElem, physical.lsb, sir::Edge::Pos});
    if (edge != sir::Edge::Pos)
        clocks.push_back({physical.slot, physical.firstElem, physical.lsb, sir::Edge::Neg});
}

std::vector<sir::Type> tempTypes(const auto& body) {
    std::vector<sir::Type> types(body.numTemps);
    for (const auto& block : body.blocks) {
        for (const auto& param : block.params)
            types[param.temp.index()] = param.type;
        for (const auto& in : block.instrs)
            if (in.result != sir::kNone) types[in.result.index()] = in.type;
    }
    return types;
}

// A wait ends the current clock context. A subsequent explicit edge starts a
// new one, also in an initial block reached through delays or loop back edges.
void collect(const sir::Design& d, sir::ProcessId pid, Sites& sites) {
    const auto& proc = d.process(pid);
    if (d.instance(proc.instance).kind == sir::InstanceKind::Program || proc.blocks.empty()) return;
    using State = std::tuple<std::uint32_t, std::uint32_t, std::optional<Clock>>;
    std::vector<State> pending;
    std::set<State> visited;
    const auto push = [&](std::uint32_t body, std::uint32_t block, std::optional<Clock> clock) {
        State state{body, block, clock};
        if (visited.insert(state).second) pending.push_back(state);
    };
    std::vector<Clock> initial;
    for (const auto& trigger : proc.triggers)
        if (trigger.watch == sir::kNone) addEdges(initial, d, trigger.range, trigger.edge);
    if (initial.empty()) push(0, 0, {});
    for (auto clock : initial)
        push(0, 0, clock);
    std::map<std::uint32_t, std::vector<sir::Type>> types;
    while (!pending.empty()) {
        auto [body, b, clock] = pending.back();
        pending.pop_back();
        const auto& block = body ? d.subroutines[body - 1].blocks[b] : proc.blocks[b];
        for (const auto& in : block.instrs) {
            const bool write =
                in.op == Op::Store || in.op == Op::StoreElem || in.op == Op::StoreBits;
            if (clock && (write || in.op == Op::Load || in.op == Op::LoadElem)) {
                auto [it, inserted] = types.try_emplace(body);
                if (inserted)
                    it->second = body ? tempTypes(d.subroutines[body - 1]) : tempTypes(proc);
                bool dynamic = false;
                if (auto range = storageAccessRange(d, in, it->second, dynamic)) {
                    auto& found = sites[{*clock, range->slot}];
                    (write ? found.writes : found.reads).push_back({pid, *range, in.loc});
                }
            }
            if (in.op == Op::Call || in.op == Op::CallTask) {
                const auto sub = in.operands.front().asSubroutine();
                const auto& target = d.subroutine(sub);
                if (!target.blocks.empty()) push(sub.index() + 1, 0, clock);
                if (target.mayWait) clock.reset();
            }
        }
        const auto& last = block.instrs.back();
        std::vector<Clock> resumed;
        if (sir::isWait(last)) clock.reset();
        if (last.op == Op::WaitEvent)
            for (const auto& item : last.operands)
                if (item.kind == Operand::Kind::Slot)
                    addEdges(resumed, d,
                             {item.asSlot(), item.firstElem, item.numElems, item.lsb, item.width},
                             item.edge);
        for (const auto& target : last.operands) {
            if (target.kind != Operand::Kind::Block) continue;
            if (resumed.empty())
                push(body, target.id, clock);
            else
                for (auto edge : resumed)
                    push(body, target.id, edge);
        }
    }
}
}  // namespace

std::vector<BlockingRace> blockingRaces(const sir::VerifiedDesign& verified) {
    const auto& d = verified.design();
    Sites sites;
    for (std::uint32_t p = 0; p < d.processes.size(); ++p)
        collect(d, sir::ProcessId{p}, sites);
    std::vector<BlockingRace> result;
    for (const auto& [key, accesses] : sites) {
        std::set<std::pair<sir::ProcessId, sir::ProcessId>> reported;
        const auto& [clock, slot] = key;
        for (const auto& write : accesses.writes)
            for (const auto& read : accesses.reads) {
                if (write.process == read.process ||
                    !sir::overlaps(d.slot(slot), write.range, read.range) ||
                    !reported.emplace(write.process, read.process).second)
                    continue;
                result.push_back({slot, clock.slot, clock.edge, write.process, read.process,
                                  write.loc, read.loc});
            }
    }
    return result;
}
}  // namespace kirimasim::analysis
