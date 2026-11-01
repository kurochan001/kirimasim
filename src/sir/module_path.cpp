// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/module_path.h"

#include <algorithm>
#include <set>
#include <tuple>

namespace kirimasim::sir {
std::optional<ModulePathDelays>
normalizeModulePathDelays(std::span<const std::optional<std::uint64_t>> values) {
    ModulePathDelays d{};
    switch (values.size()) {
        case 1: d.fill(values[0]); return d;
        case 2: d = {values[0], values[1], values[0], values[0], values[1], values[1]}; break;
        case 3: d = {values[0], values[1], values[2], values[0], values[2], values[1]}; break;
        case 6: std::copy(values.begin(), values.end(), d.begin()); break;
        case 12: std::copy(values.begin(), values.end(), d.begin()); return d;
        default: return std::nullopt;
    }
    // Overflow is positive infinity, not optional's ordinary ordering.
    const auto earliest = [](auto a, auto b) { return !a ? b : !b ? a : std::min(a, b); };
    const auto latest = [](auto a, auto b) { return !a || !b ? std::nullopt : std::max(a, b); };
    d[6] = earliest(d[0], d[2]);
    d[7] = latest(d[0], d[3]);
    d[8] = earliest(d[1], d[4]);
    d[9] = latest(d[1], d[5]);
    d[10] = latest(d[2], d[4]);
    d[11] = earliest(d[3], d[5]);
    return d;
}

std::vector<VerifyError> verifyModulePaths(const Design& d) {
    std::vector<VerifyError> errors;
    std::set<std::tuple<StrengthOriginId, SlotId, std::uint32_t, std::uint32_t, Edge>> seen;
    const auto scalar = [&](SlotRange at) {
        if (at.slot.index() >= d.slots.size()) return false;
        const auto& slot = d.slot(at.slot);
        return at.numElems == 1 && at.width == 1 && at.firstElem < elementCount(slot) &&
               at.lsb < slot.type.width && isIntegral(slot.type) && isFourState(slot.type) &&
               slot.viewOf == kNone;
    };
    for (std::size_t i = 0; i < d.modulePaths.size(); ++i) {
        const auto& path = d.modulePaths[i];
        const auto bad = [&](const std::string& message) {
            errors.push_back({"module path !p" + std::to_string(i), message, path.loc});
        };
        if (path.loc.line == 0 || path.loc.column == 0 || path.loc.file >= d.sources.size())
            bad("invalid source location");
        if (path.instance.index() >= d.instances.size() ||
            d.instance(path.instance).kind != InstanceKind::Module)
            bad("module path needs a module instance");
        if (path.edge != Edge::Any && path.edge != Edge::Pos && path.edge != Edge::Neg)
            bad("invalid module-path edge");
        if (!seen.emplace(path.origin, path.source.slot, path.source.firstElem, path.source.lsb,
                          path.edge)
                 .second)
            bad("duplicate module-path binding");
        if (!scalar(path.source) || !scalar(path.destination) || !scalar(path.functional)) {
            bad("module path needs normalized scalar four-state terminals and functional result");
            continue;
        }
        const auto& source = d.slot(path.source.slot);
        const auto& destination = d.slot(path.destination.slot);
        const auto& functional = d.slot(path.functional.slot);
        if (source.kind == SlotKind::Var || source.driverOf != kNone)
            bad("module-path source must be a public net");
        if ((destination.kind != SlotKind::Wire && destination.kind != SlotKind::Tri) ||
            destination.driverOf != kNone)
            bad("module-path destination must be a public wire or tri");
        if (functional.driverOf != path.destination.slot || functional.type != destination.type ||
            path.functional.firstElem != path.destination.firstElem ||
            path.functional.lsb != path.destination.lsb ||
            elementCount(functional) != elementCount(destination))
            bad("module path needs its private original functional contribution");
        if (path.origin.index() >= d.strengthOrigins.size()) {
            bad("module-path origin out of range");
            continue;
        }
        const auto& origin = d.strengthOrigins[path.origin.index()];
        if (origin.kind != StrengthOriginKind::Process ||
            origin.owner.index() >= d.processes.size() || origin.node >= d.strengthNodes.size()) {
            bad("module path needs an original process origin");
            continue;
        }
        const auto& node = d.strengthNodes[origin.node];
        if (node.target != path.destination) bad("module-path destination disagrees with origin");
        const auto nodeCount = std::ranges::count_if(
            d.strengthNodes, [&](const auto& n) { return n.component == node.component; });
        const auto rootCount = std::ranges::count_if(
            d.strengthOrigins, [&](const auto& r) { return r.node == origin.node; });
        if (nodeCount != 1 || rootCount != 1 ||
            std::ranges::any_of(d.strengthEdges, [&](const auto& e) {
                return e.first == origin.node || e.second == origin.node;
            }))
            bad("module-path destination requires one physical origin without passive "
                "connectivity");
        const auto& publisher = d.process(origin.owner);
        bool readsFunctional = false;
        unsigned publications = 0;
        for (const auto& block : publisher.blocks)
            for (const auto& in : block.instrs) {
                if (in.op == Op::Load && !in.operands.empty() &&
                    in.operands[0].kind == Operand::Kind::Slot &&
                    in.operands[0].asSlot() == path.functional.slot)
                    readsFunctional = true;
                if ((in.op == Op::StrengthDrive || in.op == Op::StrengthDriveDelay) &&
                    !in.operands.empty() && in.operands[0].kind == Operand::Kind::StrengthOrigin &&
                    in.operands[0].id == path.origin.index()) {
                    ++publications;
                    if (in.op != Op::StrengthDrive)
                        bad("module-path distributed delay must precede the physical publication");
                }
            }
        if (!publisher.netResolver || !readsFunctional || publications != 1)
            bad("module path needs its single original functional publisher");
        if (path.functionalOwner.index() >= d.processes.size()) {
            bad("module-path functional owner out of range");
            continue;
        }
        const auto& writer = d.process(path.functionalOwner);
        if (writer.kind != ProcessKind::Cont || writer.netResolver ||
            writer.parent.kind != Owner::Kind::None || writer.instance != path.instance)
            bad("module path needs a static functional driver inside its module");
        unsigned writers = 0;
        for (std::size_t p = 0; p < d.processes.size(); ++p)
            for (const auto& w : d.processes[p].writes)
                if (w.slot == path.functional.slot && overlaps(functional, w, path.functional)) {
                    ++writers;
                    if (p != path.functionalOwner.index())
                        bad("foreign module-path functional writer");
                    break;
                }
        if (writers != 1) bad("module path needs one functional writer");
    }
    return errors;
}
}  // namespace kirimasim::sir
