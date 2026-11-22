// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "island_candidates.h"
#include "kirimasim/analysis/islands.h"
#include "kirimasim/sir/constant_index.h"
#include "kirimasim/sir/monitor_reads.h"

#include <algorithm>
#include <map>

namespace kirimasim::analysis {
namespace {
using Reason = ExclusionReason;
using sir::Op;
using sir::Operand;
using sir::ProcessKind;

bool heapValue(sir::Type type) {
    return sir::isContainer(type) || sir::isClass(type) || type.kind == sir::TypeKind::String;
}

void reason(IslandCandidate& row, Reason code) {
    if (std::ranges::find(row.reasons, code) == row.reasons.end()) row.reasons.push_back(code);
}

bool indexed(Op op) {
    return op == Op::LoadElem || op == Op::StoreElem || op == Op::NbaElem || op == Op::StoreBits ||
           op == Op::NbaBits;
}

bool delayed(Op op) {
    switch (op) {
        case Op::WaitDelay:
        case Op::DriveDelay:
        case Op::NbaDelay:
        case Op::NbaElemDelay:
        case Op::NbaBitsDelay:
        case Op::NbaRefDelay:
        case Op::NbaRefElemDelay:
        case Op::NbaRefBitsDelay:
        case Op::NbaStrCharDelay:
        case Op::StrengthDriveDelay:
        case Op::StrengthEdgeDelay: return true;
        default: return false;
    }
}

void slotAccess(const sir::Design& d, const sir::Instr& in, const std::vector<sir::Type>& temps,
                IslandCandidate& row, bool dynamicRanges) {
    const auto& ops = in.operands;
    const auto& target = ops.front();
    if (target.kind != Operand::Kind::Slot) return;
    const auto& slot = d.slot(target.asSlot());
    auto range = sir::physicalRange(
        d, {target.asSlot(), target.firstElem, target.numElems, target.lsb, target.width});
    if (indexed(in.op)) {
        // Only a read on the sensitivity list contributes a dependency. Reading
        // data at a dynamic address on an ff's clock edge is not a trigger.
        if (in.op == Op::LoadElem && !std::ranges::any_of(row.inputs, [&](const auto& input) {
                return input.slot == range.slot && sir::overlaps(slot, input, range);
            }))
            return;
        const bool bit = in.op == Op::StoreBits || in.op == Op::NbaBits;
        const auto end = 1 + slot.dims.size() + static_cast<std::size_t>(bit);
        if (std::any_of(ops.begin() + 1, ops.begin() + static_cast<std::ptrdiff_t>(end),
                        [](const Operand& o) { return o.kind != Operand::Kind::Const; })) {
            if (!dynamicRanges)
                reason(row, Reason::RuntimeIndex);
            else if (in.op == Op::StoreElem || in.op == Op::StoreBits)
                row.immediate.push_back(sir::physicalRange(d, {target.asSlot(), 0, 0, 0, 0}));
            return;
        }
        const auto element =
            sir::constantElement(d, slot.dims, std::span{ops}.subspan(1, slot.dims.size()));
        if (!element) return;
        range.firstElem = *element;
        range.numElems = 1;
        if (bit) {
            const auto pos = sir::constantIndex(d.constant(ops[end - 1].asConst()));
            const auto& value = ops[end];
            const auto width = value.kind == Operand::Kind::Const
                                   ? d.constant(value.asConst()).type.width
                                   : temps[value.id].width;
            const auto window = range.width ? range.width : slot.type.width;
            if (!pos || *pos >= window || *pos <= -static_cast<std::int64_t>(width)) return;
            const auto lo = std::max<std::int64_t>(0, *pos);
            range.lsb += static_cast<std::uint32_t>(lo);
            range.width =
                static_cast<std::uint32_t>(std::min<std::int64_t>(window, *pos + width) - lo);
        }
    }
    if (in.op == Op::Store || in.op == Op::StoreElem || in.op == Op::StoreBits)
        row.immediate.push_back(range);
}

void inspectBody(const sir::Design& d, const auto& body, bool callee, IslandCandidate& row,
                 std::vector<sir::SubroutineId>& calls, bool dynamicRanges) {
    std::vector<sir::Type> temps(body.numTemps);
    for (const auto& block : body.blocks) {
        for (const auto& p : block.params)
            temps[p.temp.index()] = p.type;
        for (const auto& in : block.instrs)
            if (in.result != sir::kNone) temps[in.result.index()] = in.type;
    }
    for (const auto& block : body.blocks) {
        for (const auto& in : block.instrs) {
            const auto op = in.op;
            if (delayed(op))
                reason(row, Reason::Delay);
            else if (sir::isWait(in) || op == Op::NbaEvent)
                reason(row, Reason::MidEventControl);
            if (heapValue(in.type) || sir::isObjectOp(op) || sir::isContainerOp(op))
                reason(row, Reason::HeapValue);
            for (const auto& operand : in.operands) {
                if ((operand.kind == Operand::Kind::Slot &&
                     heapValue(d.slot(operand.asSlot()).type)) ||
                    (operand.kind == Operand::Kind::Const &&
                     heapValue(d.constant(operand.asConst()).type)) ||
                    (operand.kind == Operand::Kind::Temp && heapValue(temps[operand.id])))
                    reason(row, Reason::HeapValue);
            }
            if (op == Op::Syscall || op == Op::Monitor || op == Op::Strobe)
                reason(row, Reason::SystemTask);
            if (op == Op::ReportCall || op == Op::ReportCallNamed || op == Op::ReportDefer ||
                op == Op::ReportCancel)
                reason(row, Reason::ReportQueue);
            if (op == Op::Call || op == Op::CallTask)
                calls.push_back(in.operands[0].asSubroutine());
            if (op == Op::CallTask || op == Op::Fork || op == Op::Disable ||
                op == Op::DisableFork || op == Op::DisableTask || op == Op::Trigger ||
                op == Op::TriggerNb || op == Op::OverrideStart || op == Op::OverrideStop ||
                op == Op::StoreRef || op == Op::StoreRefElem || op == Op::StoreRefBits ||
                (callee &&
                 (op == Op::Syscall || sir::postponedAccess(d, in) != sir::PostponedAccess::Local)))
                reason(row, Reason::SideEffectCall);
            if (op == Op::Store || indexed(op)) slotAccess(d, in, temps, row, dynamicRanges);
        }
    }
}
}  // namespace

std::vector<sir::Trigger> islandSensitivity(const sir::Design& d, const sir::Process& p) {
    if ((p.kind != ProcessKind::Cont && p.kind != ProcessKind::Comb &&
         p.kind != ProcessKind::Latch) ||
        p.triggers.empty())
        return p.triggers;
    // SIR may load an entire vector followed by a constant select. Track
    // every SSA use, including branch arguments. Any whole-value use keeps
    // that load's original range, independent of block visitation order.
    struct Use {
        bool whole = false;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> parts;
    };
    std::vector<Use> uses(p.numTemps);
    std::map<sir::SlotId, std::vector<sir::SlotRange>> reads;
    std::vector<sir::SlotId> opaque;
    for (const auto& block : p.blocks)
        for (const auto& in : block.instrs) {
            if (in.op == Op::Call || in.op == Op::CallTask) return p.triggers;
            for (std::size_t i = 0; i < in.operands.size(); ++i) {
                const auto& arg = in.operands[i];
                if (arg.kind == Operand::Kind::Slot && in.op != Op::Load &&
                    !(i == 0 && sir::writesSlot(in.op)))
                    opaque.push_back(sir::physicalSlot(d, arg.asSlot()));
                if (arg.kind != Operand::Kind::Temp) continue;
                auto& use = uses[arg.id];
                if (i == 0 &&
                    (in.op == Op::Bitsel || in.op == Op::Partsel || in.op == Op::IdxPartsel) &&
                    in.operands[1].kind == Operand::Kind::Const) {
                    const auto pos = sir::constantIndex(d.constant(in.operands[1].asConst()));
                    if (pos && *pos >= 0 && *pos <= UINT32_MAX) {
                        use.parts.emplace_back(static_cast<std::uint32_t>(*pos), in.type.width);
                        continue;
                    }
                }
                use.whole = true;
            }
        }
    for (const auto& block : p.blocks)
        for (const auto& in : block.instrs) {
            if (in.op != Op::Load) continue;
            const auto& arg = in.operands.front();
            auto r = sir::physicalRange(
                d, {arg.asSlot(), arg.firstElem, arg.numElems, arg.lsb, arg.width});
            const auto& use = uses[in.result.index()];
            auto& parts = reads[r.slot];
            const auto width = r.width ? r.width : d.slot(r.slot).type.width;
            if (use.whole || use.parts.empty())
                parts.push_back(r);
            else
                for (auto [offset, n] : use.parts) {
                    if (offset >= width) continue;
                    auto part = r;
                    part.lsb += offset;
                    part.width = std::min(width - offset, n);
                    parts.push_back(part);
                }
        }
    std::vector<sir::Trigger> result;
    for (const auto& trigger : p.triggers) {
        if (trigger.edge != sir::Edge::Any || trigger.guard != sir::kNone ||
            trigger.watch != sir::kNone)
            return p.triggers;
        const auto range = sir::physicalRange(d, trigger.range);
        auto it = reads.find(range.slot);
        if (it == reads.end() || it->second.empty() ||
            std::ranges::find(opaque, range.slot) != opaque.end() ||
            !std::ranges::all_of(
                it->second, [&](auto r) { return sir::covers(d.slot(range.slot), range, r); })) {
            result.push_back(trigger);
            continue;
        }
        for (auto r : it->second)
            if (std::ranges::none_of(result, [&](const auto& t) { return t.range == r; }))
                result.push_back({r, sir::Edge::Any});
    }
    return result;
}

std::vector<IslandCandidate> islandCandidates(const sir::Design& d, bool dynamicRanges) {
    const auto patterns = sir::validatePatterns(d);
    std::vector<bool> overrideTargets(d.slots.size());
    for (std::uint32_t i = 0; i < d.slots.size(); ++i)
        if ((d.slots[i].flags & sir::kForceTarget) != 0)
            overrideTargets[sir::physicalSlot(d, sir::SlotId{i}).index()] = true;
    for (const auto& p : d.processes)
        if (p.kind == ProcessKind::Override)
            for (const auto r : p.writes)
                overrideTargets[sir::physicalSlot(d, r.slot).index()] = true;
    std::vector<IslandCandidate> rows(d.processes.size());
    std::vector<std::size_t> visited(d.subroutines.size(), d.processes.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& p = d.processes[i];
        auto& row = rows[i];
        const auto k = p.kind;
        if (k != ProcessKind::Cont && k != ProcessKind::Comb && k != ProcessKind::Latch &&
            k != ProcessKind::Ff && k != ProcessKind::FfPlain)
            reason(row, Reason::Kind);
        if (p.netResolver) reason(row, Reason::NetResolver);
        if (d.instance(p.instance).kind == sir::InstanceKind::Program) reason(row, Reason::Program);
        if (!p.implicitContainers.empty()) reason(row, Reason::HeapValue);
        for (auto r : p.writes)
            if (overrideTargets[sir::physicalSlot(d, r.slot).index()])
                reason(row, Reason::OverrideTarget);
        std::vector<sir::SubroutineId> calls;
        for (const auto& trigger : islandSensitivity(d, p)) {
            if (trigger.watch != sir::kNone) {
                const auto reads =
                    sir::monitorReads(d, sir::Owner::of(trigger.watch), patterns.shapes);
                for (const auto r : reads.reads)
                    row.inputs.push_back(sir::physicalRange(d, r));
                if (reads.heap) reason(row, Reason::HeapValue);
                if (!reads.refs.empty()) reason(row, Reason::RuntimeIndex);
                calls.push_back(trigger.watch);
            } else
                row.inputs.push_back(sir::physicalRange(d, trigger.range));
            if (trigger.guard != sir::kNone) calls.push_back(trigger.guard);
        }
        for (auto r : p.strengthTriggers)
            row.inputs.push_back(sir::physicalRange(d, r));
        inspectBody(d, p, false, row, calls, dynamicRanges);
        while (!calls.empty()) {
            const auto id = calls.back();
            calls.pop_back();
            if (visited[id.index()] == i) continue;
            visited[id.index()] = i;
            const auto& sub = d.subroutine(id);
            if (sub.mayWait ||
                (!sub.automatic && sub.kind == sir::SubroutineKind::Function &&
                 !sub.params.empty()) ||
                std::ranges::any_of(sub.params, [](const auto& param) {
                    return param.dir != sir::Direction::Input &&
                           param.dir != sir::Direction::ConstRef;
                }))
                reason(row, Reason::SideEffectCall);
            inspectBody(d, sub, true, row, calls, dynamicRanges);
        }
        std::ranges::sort(row.reasons);
    }
    return rows;
}
}  // namespace kirimasim::analysis
