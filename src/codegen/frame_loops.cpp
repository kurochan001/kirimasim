// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "frame_loops.h"
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

std::optional<std::int64_t> integer(const sir::Design& design, const Operand& operand) {
    if (operand.kind != Operand::Kind::Const) return {};
    const auto& value = design.constant(operand.asConst());
    if (value.type != sir::s2(32)) return {};
    return sir::constantIndex(value);
}

struct Loop {
    std::size_t field, preheader, header, body, latch;
    std::uint64_t trips;
};

std::optional<Loop> match(const sir::Design& design, const sir::Process& process,
                          const Edges& incoming, const Edges& outgoing, std::size_t header) {
    const auto& test = process.blocks[header].instrs;
    if (header == 0 || test.size() != 3 || test[0].op != Op::Fload || test[1].op != Op::Lt ||
        test[2].op != Op::BrIf || !isResult(test[1].operands[0], test[0]) ||
        !isResult(test[2].operands[0], test[1]) || incoming[header].size() != 2)
        return {};
    const auto field = test[0].operands[0].id;
    const auto stop = integer(design, test[1].operands[1]);
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
    if (init.size() < 2 || init.back().op != Op::Br || inc.size() != 4 || inc[0].op != Op::Fload ||
        inc[0].operands[0].id != field || inc[1].op != Op::Add ||
        !isResult(inc[1].operands[0], inc[0]) || integer(design, inc[1].operands[1]) != 1 ||
        inc[2].op != Op::Fstore || inc[2].operands[0].id != field ||
        !isResult(inc[2].operands[1], inc[1]) || inc[3].op != Op::Br)
        return {};
    const auto& store = init[init.size() - 2];
    if (store.op != Op::Fstore || store.operands[0].id != field) return {};
    const auto start = integer(design, store.operands[1]);
    if (!start || *start < 0) return {};

    // Both limits fit a signed 32-bit cell. With a strict upper bound the
    // last increment reaches at most INT32_MAX. Bound compilation eligibility
    // and omit any loop whose count depends on a runtime value.
    const auto trips = static_cast<std::uint64_t>(std::max<std::int64_t>(0, *stop - *start));
    if (trips > 256) return {};
    return Loop{field, preheader, header, body, latch, trips};
}
}  // namespace

std::optional<FrameLoops> frameLoops(const sir::Design& design, sir::ProcessId id) {
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
    for (std::size_t b = 0; b < count; ++b) {
        const auto& block = process.blocks[b];
        if (!block.params.empty() || block.instrs.empty()) return {};
        for (const auto& in : block.instrs)
            if (in.op == Op::Call || in.op == Op::Store || in.op == Op::StoreElem ||
                in.op == Op::StoreBits)
                return {};
        for (const auto& arg : block.instrs.back().operands)
            if (arg.kind == Operand::Kind::Block) {
                outgoing[b].push_back(arg.id);
                incoming[arg.id].push_back(b);
            }
    }

    std::vector<std::optional<Loop>> byField(process.frame.size());
    std::vector<bool> inside(count);
    FrameLoops plan{std::vector<bool>(count)};
    for (std::size_t header = 0; header < count; ++header) {
        const auto loop = match(design, process, incoming, outgoing, header);
        if (!loop) continue;
        if (byField[loop->field]) return {};
        for (auto block : {header, loop->body, loop->latch}) {
            if (inside[block]) return {};
            inside[block] = true;
            plan.repeatedSteps += loop->trips * process.blocks[block].instrs.size();
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
}  // namespace kirimasim::codegen
