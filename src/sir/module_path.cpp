// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/module_path.h"

#include <algorithm>
#include <set>
#include <tuple>

namespace kirimasim::sir {
namespace {
bool pathConditionOp(Op op) {
    switch (op) {
        case Op::Load:
        case Op::LoadElem:
        case Op::Neg:
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Mod:
        case Op::Pow:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::Eq:
        case Op::Ne:
        case Op::Ceq:
        case Op::Cne:
        case Op::Weq:
        case Op::Wne:
        case Op::Lnot:
        case Op::Land:
        case Op::Lor:
        case Op::Limp:
        case Op::Leqv:
        case Op::Truth:
        case Op::And:
        case Op::Or:
        case Op::Xor:
        case Op::Xnor:
        case Op::Not:
        case Op::Rand:
        case Op::Rnand:
        case Op::Ror:
        case Op::Rnor:
        case Op::Rxor:
        case Op::Rxnor:
        case Op::Shl:
        case Op::Shr:
        case Op::Ashr:
        case Op::Select:
        case Op::Bitsel:
        case Op::Partsel:
        case Op::IdxPartsel:
        case Op::Concat:
        case Op::Repl:
        case Op::Zext:
        case Op::Sext:
        case Op::Trunc:
        case Op::To2:
        case Op::To4:
        case Op::CondX:
        case Op::Br:
        case Op::BrIf:
        case Op::Ret: return true;
        default: return false;
    }
}
}  // namespace
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
    std::set<std::tuple<StrengthOriginId, SlotId, std::uint32_t, std::uint32_t, Edge,
                        ModulePathEnable, SubroutineId>>
        seen;
    const auto pairKey = [](const ModulePath& p) {
        return std::tuple{p.instance, p.origin, p.source.slot, p.source.firstElem, p.source.lsb};
    };
    for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
        const auto& condition = d.subroutines[i];
        if (condition.kind != SubroutineKind::PathCondition) continue;
        const auto bad = [&](const std::string& message) {
            errors.push_back(
                {"module-path condition @" + std::to_string(i), message, condition.loc});
        };
        if (condition.returnType != u4(1) || condition.automatic || condition.mayWait ||
            !condition.params.empty() || !condition.frame.empty() ||
            condition.frameOf.kind != Owner::Kind::None || condition.classRole != ClassRole::None ||
            condition.classOwner != kNone || !condition.activation.scopes.empty() ||
            condition.activation.root != kNone || !condition.activation.extents.empty() ||
            condition.activation.inheritCaptures)
            bad("path condition needs a static frame-free four-state one-bit evaluator");
        std::vector<std::size_t> incoming(condition.blocks.size());
        for (const auto& block : condition.blocks) {
            for (const auto& param : block.params)
                if (!isIntegral(param.type)) bad("path condition needs integral block values");
            for (const auto& in : block.instrs) {
                if (!pathConditionOp(in.op) ||
                    (in.type.kind != TypeKind::Void && !isIntegral(in.type)))
                    bad("effectful or nonintegral path-condition instruction");
                for (std::size_t n = 0; n < in.operands.size(); ++n) {
                    const auto& operand = in.operands[n];
                    if (operand.kind == Operand::Kind::Slot) {
                        if (operand.id >= d.slots.size()) continue;
                        const auto& slot = d.slots[operand.id];
                        if ((in.op != Op::Load && in.op != Op::LoadElem) || n != 0 ||
                            slot.instance != condition.instance || slot.subroutine != kNone ||
                            slot.driverOf != kNone || !isIntegral(slot.type))
                            bad("path condition needs a local public integral read binding");
                        auto root = operand.id;
                        std::size_t depth = 0;
                        while (root < d.slots.size() && d.slots[root].viewOf != kNone &&
                               depth++ < d.slots.size())
                            root = d.slots[root].viewOf.index();
                        if (root >= d.slots.size() || depth > d.slots.size() ||
                            d.slots[root].driverOf != kNone || d.slots[root].subroutine != kNone ||
                            !isIntegral(d.slots[root].type))
                            bad("path condition read binding has no public integral root");
                    } else if (operand.kind == Operand::Kind::Const) {
                        if (operand.id < d.constants.size() &&
                            !isIntegral(d.constants[operand.id].type))
                            bad("nonintegral path-condition constant");
                    } else if (operand.kind == Operand::Kind::Block) {
                        if ((in.op != Op::Br && in.op != Op::BrIf) ||
                            operand.id >= condition.blocks.size())
                            bad("invalid path-condition control flow");
                        else
                            ++incoming[operand.id];
                    } else if (operand.kind != Operand::Kind::Temp &&
                               !(in.op == Op::CondX && operand.kind == Operand::Kind::Text)) {
                        bad("path condition may not use frames, calls or foreign references");
                    }
                    if ((in.op == Op::LoadElem && n > 0) ||
                        ((in.op == Op::Bitsel || in.op == Op::Partsel || in.op == Op::IdxPartsel) &&
                         n == 1)) {
                        if (operand.kind != Operand::Kind::Const)
                            bad("path condition needs constant selections");
                    }
                }
            }
        }
        std::vector<std::uint32_t> ready;
        ready.reserve(condition.blocks.size());
        for (std::size_t b = 0; b < incoming.size(); ++b)
            if (incoming[b] == 0) ready.push_back(static_cast<std::uint32_t>(b));
        for (std::size_t n = 0; n < ready.size(); ++n)
            for (const auto& in : condition.blocks[ready[n]].instrs)
                for (const auto& operand : in.operands)
                    if (operand.kind == Operand::Kind::Block && operand.id < incoming.size() &&
                        incoming[operand.id] > 0 && --incoming[operand.id] == 0)
                        ready.push_back(operand.id);
        if (ready.size() != condition.blocks.size()) bad("cyclic path-condition control flow");
    }
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
                          path.edge, path.enable, path.condition)
                 .second)
            bad("duplicate module-path binding");
        if (path.enable != ModulePathEnable::Always && path.enable != ModulePathEnable::Condition &&
            path.enable != ModulePathEnable::IfNone)
            bad("invalid module-path enable kind");
        if (path.enable == ModulePathEnable::Always) {
            if (path.condition != kNone || path.group != UINT32_MAX)
                bad("unconditional path has condition/group metadata");
        } else {
            if (path.edge != Edge::Any || path.group == UINT32_MAX)
                bad("conditional/fallback path needs a simple path and group");
            if (path.enable == ModulePathEnable::Condition) {
                if (path.condition.index() >= d.subroutines.size() ||
                    d.subroutine(path.condition).kind != SubroutineKind::PathCondition ||
                    d.subroutine(path.condition).instance != path.instance)
                    bad("invalid or foreign module-path condition");
            } else if (path.condition != kNone) {
                bad("fallback path has an evaluator");
            }
            for (const auto& other : d.modulePaths) {
                const bool samePair = pairKey(path) == pairKey(other);
                if (other.enable != ModulePathEnable::Always &&
                    (other.group == path.group) != samePair)
                    bad("module-path group disagrees with owner/source/destination");
                if (samePair && path.enable == ModulePathEnable::IfNone &&
                    (other.enable == ModulePathEnable::Always || other.edge != Edge::Any))
                    bad("fallback overlaps an unconditional or edge-sensitive path");
            }
        }
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
