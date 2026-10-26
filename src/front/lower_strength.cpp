// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Import ordinary owned contributions, never the published resolved value.
// Approved MOS/switch contract 1, 6, 7; SIR_SPEC 3.5, 5.3, 7.1, 10.
#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "lowerer.h"

namespace kirimasim::front {
namespace {
using Range = sir::StrengthRange;
bool coversBit(const sir::Slot& slot, const sir::SlotRange& range, std::uint32_t element,
               std::uint32_t bit) {
    const auto count = range.numElems == 0 ? sir::elementCount(slot) : range.numElems;
    const auto width = range.width == 0 ? slot.type.width : range.width;
    return element >= range.firstElem && element - range.firstElem < count && bit >= range.lsb &&
           bit - range.lsb < width;
}
Range initialPoint(sir::SlotKind kind) {
    switch (sir::initialValue(kind)) {
        case sir::InitValue::Zero:
            return kind == sir::SlotKind::Supply0 ? Range{-7, -7} : Range{-5, -5};
        case sir::InitValue::One: return kind == sir::SlotKind::Supply1 ? Range{7, 7} : Range{5, 5};
        case sir::InitValue::X: return {-6, 6};
        case sir::InitValue::Z: return {};
    }
    return {};
}
std::vector<Range> initialRanges(const sir::Design& design, sir::SlotId id,
                                 const std::vector<sir::SlotRange>& unknown) {
    const auto& slot = design.slot(id);
    std::vector<Range> result(sir::elementCount(slot) * slot.type.width, initialPoint(slot.kind));
    for (const auto& w : unknown) {
        if (sir::physicalSlot(design, w.slot) != id) continue;
        for (std::uint32_t e = 0; e < sir::elementCount(slot); ++e)
            for (std::uint32_t bit = 0; bit < slot.type.width; ++bit)
                if (coversBit(slot, w, e, bit)) result[e * slot.type.width + bit] = {-6, 6};
    }
    return result;
}
}  // namespace

void Lowerer::prepareStrengthNets() {
    if (strengthNets_.empty()) return;
    const auto unknown = sir::unknownAtStart(design_);
    for (const auto& [net, where] : strengthNets_) {
        const auto& slot = design_.slot(net);
        const auto reject = [&](const std::string& reason) {
            ++numErrors_;
            diags_ +=
                whereOf(where) + ": error: unsupported: " + reason + " (not implemented yet)\n";
        };
        if (!sir::isIntegral(slot.type) || !sir::isFourState(slot.type) ||
            slot.kind == sir::SlotKind::Var || slot.driverOf != sir::kNone) {
            reject("MOS strength metadata on this net domain");
            continue;
        }
        for (std::uint32_t s = 0; s < design_.slots.size(); ++s)
            if (sir::physicalSlot(design_, sir::SlotId{s}) == net &&
                (design_.slots[s].flags & sir::kForceTarget) != 0)
                reject("force/release of a MOS strength net");
        auto initial = initialRanges(design_, net, unknown);
        // Before forcing a previously unresolved driver into private storage,
        // retain its initial X at its own static ranks, including highz sides.
        for (const auto& p : design_.processes) {
            if (p.kind != sir::ProcessKind::Cont) continue;
            const auto id =
                sir::ProcessId{static_cast<std::uint32_t>(&p - design_.processes.data())};
            const auto it = strengths_.find(id);
            const auto one = it == strengths_.end() ? 6 : it->second.first;
            const auto zero = it == strengths_.end() ? 6 : it->second.second;
            for (const auto& w : p.writes) {
                if (sir::physicalSlot(design_, w.slot) != net) continue;
                for (std::uint32_t e = 0; e < sir::elementCount(slot); ++e)
                    for (std::uint32_t bit = 0; bit < slot.type.width; ++bit)
                        if (coversBit(slot, w, e, bit))
                            initial[e * slot.type.width + bit] = {static_cast<std::int8_t>(-zero),
                                                                  static_cast<std::int8_t>(one)};
            }
        }
        strengthInitial_[net] = std::move(initial);
    }
}

void Lowerer::addStrengthSideValidity(sir::ProcessId process, sir::SlotId driver) {
    const auto flag = sir::SlotId{static_cast<std::uint32_t>(design_.slots.size())};
    sir::Slot slot;
    slot.name = design_.slot(driver).name + ".$sideValid";
    slot.type = sir::u2(1);
    slot.instance = design_.slot(driver).instance;
    design_.slots.push_back(std::move(slot));
    initialSideValidity_[driver] = flag;
    auto& p = design_.process(process);
    const auto one = constant(sir::u2(1), 1).op;
    bool delayed = false;
    for (auto& block : p.blocks)
        for (auto& in : block.instrs)
            if (in.op == sir::Op::DriveDelay && in.operands[0].asSlot() == driver) {
                // The validity bit commits with the complete value/side tuple,
                // not when its delayed propagation is merely scheduled.
                in.operands.push_back(sir::Operand::slot(flag, 0, 1));
                in.operands.push_back(one);
                delayed = true;
            }
    if (!delayed)
        for (auto& block : p.blocks) {
            auto& code = block.instrs;
            if (code.empty() || code.back().op != sir::Op::Ret) continue;
            code.insert(code.end() - 1, {sir::Op::Store,
                                         sir::voidType(),
                                         sir::kNone,
                                         {sir::Operand::slot(flag), one},
                                         code.back().loc});
        }
    p.writes.push_back({flag});
}

void Lowerer::finishStrengthNets() {
    if (strengthNets_.empty() || numErrors_ != 0) return;
    const auto unknown = sir::unknownAtStart(design_);
    std::map<sir::SlotId, std::uint32_t> firstNode;
    std::uint32_t component = 0;
    for (const auto& [net, where] : strengthNets_) {
        auto& slot = design_.slot(net);
        slot.flags |= sir::kNeedsStrength;
        const auto& initial = newlyResolvedStrengthNets_.contains(net)
                                  ? strengthInitial_.at(net)
                                  : (strengthInitial_[net] = initialRanges(design_, net, unknown));
        firstNode[net] = static_cast<std::uint32_t>(design_.strengthNodes.size());
        const bool pull = slot.kind == sir::SlotKind::Tri0 || slot.kind == sir::SlotKind::Tri1;
        const bool supply =
            slot.kind == sir::SlotKind::Supply0 || slot.kind == sir::SlotKind::Supply1;
        for (std::uint32_t e = 0; e < sir::elementCount(slot); ++e)
            for (std::uint32_t bit = 0; bit < slot.type.width; ++bit) {
                const auto node = static_cast<std::uint32_t>(design_.strengthNodes.size());
                design_.strengthNodes.push_back({{net, e, 1, bit, 1},
                                                 pull || supply ? sir::SlotKind::Wire : slot.kind,
                                                 component,
                                                 initial[e * slot.type.width + bit],
                                                 where});
                if (pull || supply)
                    design_.strengthOrigins.push_back(
                        {node, sir::kNone, initialPoint(slot.kind),
                         pull ? sir::StrengthOriginKind::Pull : sir::StrengthOriginKind::Supply,
                         where});
            }
        ++component;
    }
    const auto nodeOf = [&](sir::SlotRange at) {
        return firstNode.at(at.slot) + at.firstElem * design_.slot(at.slot).type.width + at.lsb;
    };
    if (!passiveConnections_.empty()) {
        std::vector<std::uint32_t> parent(component);
        std::iota(parent.begin(), parent.end(), 0U);
        const auto find = [&](std::uint32_t id) {
            auto root = id;
            while (parent[root] != root)
                root = parent[root];
            while (parent[id] != id) {
                const auto next = parent[id];
                parent[id] = root;
                id = next;
            }
            return root;
        };
        for (const auto& edge : passiveConnections_) {
            const auto a = nodeOf(edge.first), b = nodeOf(edge.second);
            parent[find(design_.strengthNodes[a].component)] =
                find(design_.strengthNodes[b].component);
            const auto id =
                sir::StrengthEdgeId{static_cast<std::uint32_t>(design_.strengthEdges.size())};
            design_.strengthEdges.push_back({a, b, edge.kind, edge.owner, edge.delayed, edge.loc});
            if (edge.owner == sir::kNone) continue;
            auto& owner = design_.process(edge.owner);
            owner.strengthEdgeWrites.push_back(id);
            for (auto& block : owner.blocks)
                for (auto& in : block.instrs)
                    if (in.op == sir::Op::StrengthEdge || in.op == sir::Op::StrengthEdgeDelay)
                        in.operands[0] = sir::Operand::strengthEdge(id);
        }
        std::map<std::uint32_t, std::uint32_t> dense;
        for (auto& node : design_.strengthNodes) {
            const auto root = find(node.component);
            node.component =
                dense.try_emplace(root, static_cast<std::uint32_t>(dense.size())).first->second;
        }
    }
    const auto origin = [&](sir::ProcessId owner, sir::SlotRange at, Range initial,
                            sir::SourceLoc where) {
        const auto id =
            sir::StrengthOriginId{static_cast<std::uint32_t>(design_.strengthOrigins.size())};
        design_.strengthOrigins.push_back(
            {nodeOf(at), owner, initial, sir::StrengthOriginKind::Process, where});
        design_.process(owner).strengthWrites.push_back(id);
        return sir::Operand::strengthOrigin(id);
    };
    for (const auto& [id, output] : mosOutputs_) {
        const auto root = origin(id, output.target, {}, output.loc);
        for (auto& block : design_.process(id).blocks)
            for (auto& in : block.instrs)
                if (in.op == sir::Op::StrengthDrive || in.op == sir::Op::StrengthDriveDelay)
                    in.operands[0] = root;
    }
    for (const auto& [net, where] : strengthNets_) {
        const auto found = netResolvers_.find(net);
        if (found == netResolvers_.end()) continue;  // implicit roots or MOS alone.
        const auto id = found->second;
        auto& p = design_.process(id);
        // Resolve operands already express the owned contribution, static
        // ranks and optional side field. Import that typed boundary once.
        std::map<sir::TempId, sir::Operand> loads;
        std::map<sir::TempId, sir::Operand> outputs;
        const auto old = std::move(p.blocks[0].instrs);
        for (const auto& in : old) {
            if (in.op == sir::Op::Load) loads.emplace(in.result, in.operands[0]);
            if (in.op == sir::Op::Store && in.operands[1].kind == sir::Operand::Kind::Temp)
                outputs.emplace(sir::TempId{in.operands[1].id}, in.operands[0]);
        }
        p.numTemps = 0;
        p.writes.clear();
        p.drives.clear();
        auto& code = p.blocks[0].instrs;
        code.clear();
        const auto emit = [&](sir::Op op, sir::Type type, std::vector<sir::Operand> operands) {
            const auto temp = sir::TempId{p.numTemps++};
            code.push_back({op, type, temp, std::move(operands), where});
            return sir::Operand::temp(temp);
        };
        for (const auto& in : old) {
            if (in.op != sir::Op::Resolve) continue;
            const auto& target = outputs.at(in.result);
            const auto& args = in.operands;
            for (std::size_t k = 1; k < args.size();) {
                const auto source = loads.at(sir::TempId{args[k].id});
                const auto rankOne = args[k + 1], rankZero = args[k + 2];
                k += 3;
                std::optional<sir::Operand> side;
                if (k < args.size() && args[k].kind == sir::Operand::Kind::Temp &&
                    !sir::isFourState(
                        design_.slot(loads.at(sir::TempId{args[k].id}).asSlot()).type))
                    side = loads.at(sir::TempId{args[k++].id});
                const auto driver = source.asSlot();
                const auto& shape = design_.slot(driver);
                const auto one =
                    static_cast<std::int8_t>(design_.constant(rankOne.asConst()).aWords[0]);
                const auto zero =
                    static_cast<std::int8_t>(design_.constant(rankZero.asConst()).aWords[0]);
                const auto valid = initialSideValidity_.find(driver);
                for (std::uint32_t bit = 0; bit < shape.type.width; ++bit) {
                    const bool written = std::ranges::any_of(unknown, [&](const auto& w) {
                        return w.slot == driver && coversBit(shape, w, source.firstElem, bit);
                    });
                    if (!written) continue;  // no physical driver on uncovered bits.
                    auto slice = source;
                    slice.lsb = bit;
                    slice.width = 1;
                    const auto value = emit(sir::Op::Load, sir::u4(1), {slice});
                    std::vector<sir::Operand> make{value, rankOne, rankZero};
                    if (side) {
                        auto oneSide = *side, zeroSide = *side;
                        oneSide.lsb = bit;
                        zeroSide.lsb = shape.type.width + bit;
                        oneSide.width = zeroSide.width = 1;
                        const auto oneBit = emit(sir::Op::Load, sir::u2(1), {oneSide});
                        const auto zeroBit = emit(sir::Op::Load, sir::u2(1), {zeroSide});
                        auto mask = emit(sir::Op::Concat, sir::u2(2), {zeroBit, oneBit});
                        if (valid != initialSideValidity_.end()) {
                            const auto initialized = emit(sir::Op::Load, sir::u2(1),
                                                          {sir::Operand::slot(valid->second)});
                            mask = emit(sir::Op::Select, sir::u2(2),
                                        {initialized, mask, constant(sir::u2(2), 3).op});
                        }
                        make.push_back(mask);
                    }
                    const Range initial = side && valid == initialSideValidity_.end()
                                              ? Range{}
                                              : Range{static_cast<std::int8_t>(-zero), one};
                    const auto root =
                        origin(id, {net, target.firstElem, 1, bit, 1}, initial, where);
                    const auto strength =
                        emit(sir::Op::StrengthMake, sir::strengthType(1), std::move(make));
                    code.push_back({sir::Op::StrengthDrive,
                                    sir::voidType(),
                                    sir::kNone,
                                    {root, strength},
                                    where});
                }
                if (valid != initialSideValidity_.end())
                    p.triggers.push_back({{valid->second}, sir::Edge::Any});
            }
        }
        code.push_back({sir::Op::Ret, sir::voidType(), sir::kNone, {}, where});
    }
}
}  // namespace kirimasim::front
