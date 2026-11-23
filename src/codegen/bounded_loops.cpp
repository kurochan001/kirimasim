// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "bounded_loops.h"
#include "kirimasim/sir/constant_index.h"
#include <algorithm>
#include <array>

namespace kirimasim::codegen {
namespace {
using sir::Op;
using sir::Operand;
using Edges = std::vector<std::vector<std::size_t>>;

bool isResult(const Operand& operand, const sir::Instr& instruction) {
    return operand.kind == Operand::Kind::Temp && operand.asTemp() == instruction.result;
}

std::optional<std::int64_t> integer(const sir::Design& design, const Operand& operand,
                                    sir::Type type) {
    if (operand.kind != Operand::Kind::Const) return {};
    const auto& value = design.constant(operand.asConst());
    if (value.type != type) return {};
    return sir::constantIndex(value);
}

struct Loop {
    std::size_t field, preheader, header, body, latch;
    std::uint32_t trips;
};

std::optional<Loop> match(const sir::Design& design, const sir::Process& process,
                          const Edges& incoming, const Edges& outgoing, std::size_t header,
                          bool slot) {
    const auto load = slot ? Op::Load : Op::Fload;
    const auto storeOp = slot ? Op::Store : Op::Fstore;
    const auto& test = process.blocks[header].instrs;
    if (header == 0 || test.size() != 3 || test[0].op != load || test[1].op != Op::Lt ||
        test[2].op != Op::BrIf || !isResult(test[1].operands[0], test[0]) ||
        !isResult(test[2].operands[0], test[1]) || incoming[header].size() != 2)
        return {};
    const auto field = test[0].operands[0].id;
    const auto type = test[0].type;
    const auto wholeCounter = [&](const Operand& op) {
        return op.id == field && (!slot || (op.kind == Operand::Kind::Slot && op.lsb == 0 &&
                                            op.width == 0 && !op.firstElem && !op.numElems));
    };
    if (!wholeCounter(test[0].operands[0])) return {};
    if (slot && (type != design.slots[field].type || (type != sir::s4(32) && type != sir::u4(32))))
        return {};
    const auto stop = integer(design, test[1].operands[1], type);
    if (!stop || *stop < 0) return {};

    const auto body = outgoing[header][0];
    if (!body || incoming[body].size() != 1 || process.blocks[body].instrs.back().op != Op::Br)
        return {};
    const auto latch = outgoing[body][0];
    if (!latch || incoming[latch].size() != 1 || outgoing[latch].size() != 1 ||
        outgoing[latch][0] != header)
        return {};
    const auto preheader = incoming[header][incoming[header][0] == latch ? 1 : 0];
    std::array nodes{header, body, latch, preheader, outgoing[header][1]};
    std::ranges::sort(nodes);
    if (std::ranges::adjacent_find(nodes) != nodes.end()) return {};

    const auto& init = process.blocks[preheader].instrs;
    const auto& inc = process.blocks[latch].instrs;
    if (init.size() < 2 || init.back().op != Op::Br || inc.size() != 4 || inc[0].op != load ||
        !wholeCounter(inc[0].operands[0]) || inc[0].type != type || inc[1].type != type ||
        inc[1].op != Op::Add || !isResult(inc[1].operands[0], inc[0]) ||
        integer(design, inc[1].operands[1], type) != 1 || inc[2].op != storeOp ||
        !wholeCounter(inc[2].operands[0]) || !isResult(inc[2].operands[1], inc[1]) ||
        inc[3].op != Op::Br)
        return {};
    const auto& store = init[init.size() - 2];
    if (store.op != storeOp || !wholeCounter(store.operands[0])) return {};
    const auto start = integer(design, store.operands[1], type);
    if (!start || *start < 0) return {};

    // Both constants fit the counter type. With a strict comparison the
    // last increment reaches at most the upper limit. Bound compilation eligibility
    // and omit any loop whose count depends on a runtime value.
    const auto trips = static_cast<std::uint64_t>(std::max<std::int64_t>(0, *stop - *start));
    if (trips > 256) return {};
    return Loop{field, preheader, header, body, latch, static_cast<std::uint32_t>(trips)};
}

bool edges(const sir::Process& process, Edges& incoming, Edges& outgoing) {
    for (std::size_t b = 0; b < process.blocks.size(); ++b) {
        const auto& block = process.blocks[b];
        if (!block.params.empty() || block.instrs.empty()) return false;
        for (const auto& arg : block.instrs.back().operands)
            if (arg.kind == Operand::Kind::Block) {
                outgoing[b].push_back(arg.id);
                incoming[arg.id].push_back(b);
            }
    }
    return true;
}
}  // namespace

std::optional<BoundedLoops> frameLoops(const sir::Design& design, sir::ProcessId id) {
    const auto& process = design.process(id);
    if (process.kind != sir::ProcessKind::Ff || process.frame.empty() ||
        process.frame.size() > 16 || !process.activation.scopes.empty() ||
        !process.activation.extents.empty() || process.activation.inheritCaptures ||
        process.parent.kind != sir::Owner::Kind::None || design.foreignReflection ||
        !design.foreignObjects.empty())
        return {};
    for (const auto& trigger : process.triggers)
        if (trigger.guard != sir::kNone || trigger.watch != sir::kNone) return {};
    for (const auto& field : process.frame)
        if (field.home != sir::FrameHome::Ordinary || field.type != sir::s2(32) ||
            !field.dims.empty() || field.readonly || field.captureSource != sir::kNone ||
            field.activationScope != sir::kNone)
            return {};

    const auto count = process.blocks.size();
    Edges incoming(count), outgoing(count);
    if (!edges(process, incoming, outgoing)) return {};
    for (const auto& block : process.blocks)
        for (const auto& in : block.instrs)
            if (in.op == Op::Call || in.op == Op::Store || in.op == Op::StoreElem ||
                in.op == Op::StoreBits)
                return {};

    std::vector<std::optional<Loop>> byField(process.frame.size());
    std::vector<bool> inside(count);
    BoundedLoops plan(count);
    for (std::size_t header = 0; header < count; ++header) {
        const auto loop = match(design, process, incoming, outgoing, header, false);
        if (!loop) continue;
        if (byField[loop->field]) return {};
        for (auto block : {header, loop->body, loop->latch}) {
            if (inside[block]) return {};
            inside[block] = true;
            plan.extraVisits[block] = loop->trips;
        }
        byField[loop->field] = loop;
        plan.latch[loop->latch] = true;
    }
    if (std::ranges::any_of(byField, [](const auto& loop) { return !loop; })) return {};

    // A counter cannot escape, or be modified along any other path. This
    // also excludes reference-taking, captured frames and competing stores.
    for (std::size_t b = 0; b < count; ++b) {
        const auto& ins = process.blocks[b].instrs;
        for (const auto& in : ins)
            for (const auto& arg : in.operands) {
                if (arg.kind != Operand::Kind::Frame) continue;
                const auto& binding = byField[arg.id];
                if (!binding) return {};
                const auto& loop = *binding;
                if (in.op == Op::Fload && (b == loop.header || b == loop.body || b == loop.latch))
                    continue;
                if ((b == loop.preheader && &in == &ins[ins.size() - 2]) ||
                    (b == loop.latch && &in == &ins[2]))
                    continue;
                return {};
            }
    }
    const auto ownsFrame = [&](sir::Owner owner) {
        return owner.kind == sir::Owner::Kind::Process && owner.asProcess() == id;
    };
    if (std::ranges::any_of(design.subroutines,
                            [&](const auto& sub) { return ownsFrame(sub.frameOf); }) ||
        std::ranges::any_of(design.processes,
                            [&](const auto& child) { return ownsFrame(child.parent); }))
        return {};
    return plan;
}

std::vector<sir::ProcessId> exclusiveSlotOwners(const sir::Design& d) {
    // These tables can bind storage without an instruction's direct slot
    // operand (sampled inputs, late NBA lvalues and interface/reflection).
    if (d.foreignReflection || !d.foreignObjects.empty() || !d.foreignArguments.empty() ||
        !d.nbaLayouts.empty() || !d.clockingBlocks.empty() || !d.sampledInputs.empty() ||
        !d.interfaces.empty() || !d.classes.empty() || !d.strengthNodes.empty() ||
        !d.strengthOrigins.empty() || !d.strengthEdges.empty() || !d.modulePaths.empty())
        return {};
    std::vector<sir::ProcessId> owners(d.slots.size(), sir::kNone);
    std::vector<bool> shared(d.slots.size());
    const auto use = [&](sir::SlotId slot, sir::ProcessId process) {
        if (slot == sir::kNone) return;
        auto& owner = owners[slot.index()];
        if (process == sir::kNone || (owner != sir::kNone && owner != process))
            shared[slot.index()] = true;
        else
            owner = process;
    };
    const auto operands = [&](const auto& blocks, sir::ProcessId pid) {
        for (const auto& block : blocks)
            for (const auto& in : block.instrs)
                for (const auto& arg : in.operands)
                    if (arg.kind == Operand::Kind::Slot) use(arg.asSlot(), pid);
    };
    for (std::uint32_t p = 0; p < d.processes.size(); ++p) {
        const auto& proc = d.processes[p];
        const sir::ProcessId id{p};
        operands(proc.blocks, id);
        for (const auto& range : proc.writes)
            use(range.slot, id);
        for (const auto& trigger : proc.triggers)
            use(trigger.range.slot, id);
    }
    for (const auto& sub : d.subroutines) {
        operands(sub.blocks, sir::kNone);
        for (const auto& param : sub.params)
            use(param.slot, sir::kNone);
    }
    for (const auto& slot : d.slots) {
        use(slot.viewOf, sir::kNone);
        use(slot.driverOf, sir::kNone);
    }
    std::vector<bool> named(d.slots.size());
    for (const auto& name : d.names) {
        if (named[name.slot.index()]) shared[name.slot.index()] = true;
        named[name.slot.index()] = true;
    }
    for (std::size_t i = 0; i < owners.size(); ++i) {
        const auto& slot = d.slots[i];
        if (shared[i] || slot.kind != sir::SlotKind::Var || slot.flags || !slot.dims.empty() ||
            slot.viewOf != sir::kNone || slot.driverOf != sir::kNone ||
            slot.subroutine != sir::kNone)
            owners[i] = sir::kNone;
    }
    return owners;
}

std::optional<BoundedLoops> slotLoops(const sir::Design& d, sir::ProcessId id,
                                      std::span<const sir::ProcessId> owners) {
    const auto& p = d.process(id);
    if (owners.empty() || p.kind != sir::ProcessKind::CombAt || !p.frame.empty() ||
        p.parent.kind != sir::Owner::Kind::None || !p.activation.scopes.empty() ||
        !p.activation.extents.empty() || p.activation.inheritCaptures)
        return {};
    const auto count = p.blocks.size();
    Edges incoming(count), outgoing(count);
    if (!edges(p, incoming, outgoing)) return {};
    // Start with a single non-nested loop. The body emitter checks that
    // deleting its back edge also removes every cycle in the process.
    std::optional<Loop> loop;
    for (std::size_t b = 0; b < count; ++b)
        if (auto candidate = match(d, p, incoming, outgoing, b, true)) {
            if (loop || owners[candidate->field] != id) return {};
            loop = candidate;
        }
    if (!loop) return {};
    BoundedLoops plan(count);
    plan.latch[loop->latch] = true;
    for (std::size_t b = 0; b < count; ++b) {
        const auto& ins = p.blocks[b].instrs;
        if (b == loop->header || b == loop->body || b == loop->latch)
            plan.extraVisits[b] = loop->trips;
        for (const auto& in : ins) {
            // Other slot accesses, including taking a reference to the
            // counter or forcing a part of it, invalidate its trip bound.
            for (const auto& op : in.operands) {
                if (op.kind != Operand::Kind::Slot || op.id != loop->field) continue;
                if (in.op == Op::Load || (b == loop->preheader && &in == &ins[ins.size() - 2]) ||
                    (b == loop->latch && &in == &ins[2]))
                    continue;
                return {};
            }
        }
    }
    return plan;
}
}  // namespace kirimasim::codegen
