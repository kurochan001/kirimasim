// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Typed, scalar first bridge of the approved specify contract §§1–8.
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "kirimasim/sir/module_path.h"
#include "lowerer.h"
#include "slang/ast/symbols/PortSymbols.h"

namespace kirimasim::front {
void Lowerer::prescanModulePaths(const ast::InstanceSymbol& instance, sir::InstanceId id) {
    // Only public nets can collapse through ports. The owned functional
    // contribution is introduced later by resolveNets and never collapses.
    for (const auto& member : instance.body.members())
        if (member.kind == ast::SymbolKind::SpecifyBlock)
            specifyBlocks_.emplace_back(&member.as<ast::SpecifyBlockSymbol>(), id);
}

void Lowerer::prepareModulePaths() {
    const auto savedInstance = instance_;
    const auto savedScale = b_.scale;
    const auto savedEval = evalSymbol_;
    for (const auto& [block, instance] : specifyBlocks_) {
        instance_ = instance;
        const auto& cell = instanceSyms_[instance.index()]->as<ast::InstanceSymbol>();
        b_.scale = timeScaleOf(cell.body);
        evalSymbol_ = block;
        for (const auto& member : block->members()) {
            if (member.kind == ast::SymbolKind::Specparam &&
                !member.as<ast::SpecparamSymbol>().isPathPulse &&
                !member.name.starts_with("PATHPULSE$"))
                continue;
            if (member.kind != ast::SymbolKind::TimingPath) {
                report(member.location, "specify pulse control or timing check");
                continue;
            }
            const auto& path = member.as<ast::TimingPathSymbol>();
            try {
                if (cell.getDefinition().definitionKind != ast::DefinitionKind::Module)
                    unsupported(path.location, "specify paths outside a module");
                if (path.isStateDependent || path.getConditionExpr() != nullptr)
                    unsupported(path.location, "state-dependent specify path");
                const auto terminal = [&](const ast::Expression& expr, bool output) {
                    const auto* symbol = namedSymbol(expr);
                    if (expr.kind != ast::ExpressionKind::NamedValue || symbol == nullptr ||
                        symbol->kind != ast::SymbolKind::Net)
                        unsupported(expr.sourceRange.start(),
                                    "specify terminal other than a scalar port net");
                    const auto& net = symbol->as<ast::NetSymbol>();
                    if (!net.getType().isScalar() || !net.getType().isFourState())
                        unsupported(expr.sourceRange.start(),
                                    "vector or non-four-state specify terminal");
                    const auto direction =
                        output ? ast::ArgumentDirection::Out : ast::ArgumentDirection::In;
                    bool port = false;
                    for (const auto* p : cell.body.getPortList())
                        if (p->kind == ast::SymbolKind::Port) {
                            const auto& ps = p->as<ast::PortSymbol>();
                            port =
                                port || (ps.internalSymbol == symbol && ps.direction == direction);
                        }
                    if (!port)
                        unsupported(
                            expr.sourceRange.start(),
                            "specify terminal without the required input/output net direction");
                    const auto slot = sir::physicalSlot(design_, slots_.at(symbol));
                    const auto& physical = design_.slot(slot);
                    if (output && physical.kind != sir::SlotKind::Wire &&
                        physical.kind != sir::SlotKind::Tri)
                        unsupported(expr.sourceRange.start(),
                                    "specify destination other than wire or tri");
                    return sir::SlotRange{slot, 0, 1, 0, 1};
                };
                std::vector<sir::SlotRange> inputs, outputs;
                for (const auto* expr : path.getInputs())
                    inputs.push_back(terminal(*expr, false));
                for (const auto* expr : path.getOutputs())
                    outputs.push_back(terminal(*expr, true));
                if (inputs.empty() || outputs.empty())
                    unsupported(path.location, "empty specify path");
                std::vector<std::optional<std::uint64_t>> selected;
                for (const auto* expr : path.getDelays()) {
                    const auto cv = constantValue(*expr);
                    if (!cv)
                        unsupported(expr->sourceRange.start(),
                                    "specify delay that is not a constant");
                    const bool negative =
                        (cv->isInteger() && cv->integer().isSigned() &&
                         !cv->integer().hasUnknown() && cv->integer().isNegative()) ||
                        (cv->isReal() && double(cv->real()) < 0) ||
                        (cv->isShortReal() && double(cv->shortReal()) < 0);
                    selected.push_back(negative ? std::optional<std::uint64_t>{0}
                                                : delayTicks(*expr, path.location));
                }
                const auto delays = sir::normalizeModulePathDelays(selected);
                if (!delays) unsupported(path.location, "specify path delay list size");
                auto edge = sir::Edge::Any;
                if (path.edgeIdentifier == ast::EdgeKind::PosEdge)
                    edge = sir::Edge::Pos;
                else if (path.edgeIdentifier == ast::EdgeKind::NegEdge)
                    edge = sir::Edge::Neg;
                else if (path.edgeIdentifier != ast::EdgeKind::None)
                    unsupported(path.location, "specify path edge qualifier");
                // getEdgeSourceExpr and polarity are annotations. Do not
                // lower/evaluate them as the module's functional computation.
                for (const auto source : inputs)
                    for (const auto destination : outputs) {
                        sir::ModulePath row;
                        row.instance = instance;
                        row.source = source;
                        row.destination = destination;
                        row.edge = edge;
                        row.delays = delays.value_or(sir::ModulePathDelays{});
                        row.loc = loc(path.location);
                        modulePaths_.push_back({row, &path});
                        strengthNets_.try_emplace(destination.slot, row.loc);
                    }
            } catch (const Unsupported&) {
                continue;
            }
        }
    }
    instance_ = savedInstance;
    b_.scale = savedScale;
    evalSymbol_ = savedEval;
}

void Lowerer::checkModulePathDrivers() {
    for (auto& pending : modulePaths_) {
        auto& path = pending.row;
        const auto at = pending.symbol->location;
        try {
            std::vector<sir::ProcessId> writers;
            for (std::uint32_t i = 0; i < design_.processes.size(); ++i) {
                const auto& p = design_.processes[i];
                if (p.kind == sir::ProcessKind::Override) continue;
                if (std::ranges::any_of(p.writes, [&](const auto& w) {
                        return sir::physicalSlot(design_, w.slot) == path.destination.slot;
                    }))
                    writers.push_back(sir::ProcessId{i});
            }
            if (writers.size() != 1)
                unsupported(at,
                            "specify destination without exactly one ordinary functional driver");
            const auto writer = writers[0];
            const auto& process = design_.process(writer);
            if (process.kind != sir::ProcessKind::Cont || process.instance != path.instance ||
                process.parent.kind != sir::Owner::Kind::None || netPorts_.contains(writer) ||
                sides_.contains(writer) || mosOutputs_.contains(writer))
                unsupported(at,
                            "specify destination with a nonordinary or external functional driver");
            for (const auto& block : process.blocks)
                for (const auto& in : block.instrs)
                    if (in.op == sir::Op::Udp || in.op == sir::Op::UdpSeq)
                        unsupported(at, "specify destination with a UDP functional driver");
            if (std::ranges::any_of(passiveConnections_, [&](const auto& edge) {
                    return edge.first.slot == path.destination.slot ||
                           edge.second.slot == path.destination.slot;
                }))
                unsupported(at, "specify destination with passive connectivity");
            path.functionalOwner = writer;
            strengthNets_.try_emplace(path.destination.slot, path.loc);
        } catch (const Unsupported&) {
            continue;
        }
    }
}

void Lowerer::finishModulePaths() {
    if (numErrors_ != 0) return;
    for (auto& pending : modulePaths_) {
        auto row = pending.row;
        const auto at = pending.symbol->location;
        try {
            const auto& writer = design_.process(row.functionalOwner);
            bool foundFunctional = false;
            for (const auto& w : writer.writes)
                if (design_.slot(w.slot).driverOf == row.destination.slot) {
                    row.functional = {w.slot, 0, 1, 0, 1};
                    foundFunctional = true;
                }
            if (!foundFunctional)
                unsupported(at, "specify functional contribution without private storage");
            for (std::uint32_t i = 0; i < design_.strengthOrigins.size(); ++i) {
                const auto& origin = design_.strengthOrigins[i];
                if (design_.strengthNodes[origin.node].target == row.destination) {
                    if (row.origin != sir::kNone)
                        unsupported(at, "specify destination with multiple physical origins");
                    row.origin = sir::StrengthOriginId{i};
                }
            }
            if (row.origin == sir::kNone)
                unsupported(at, "specify destination without its original physical origin");
            // Duplicate paths are diagnosed before the typed verifier. A
            // source error never turns into an internal malformed-IR report.
            if (std::ranges::any_of(design_.modulePaths, [&](const auto& other) {
                    return other.origin == row.origin && other.source == row.source &&
                           other.edge == row.edge;
                }))
                unsupported(at, "duplicate scalar specify path binding");
            design_.modulePaths.push_back(row);
        } catch (const Unsupported&) {
            continue;
        }
    }
}
}  // namespace kirimasim::front
