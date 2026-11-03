// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Typed, scalar first bridge of the approved specify contract §§1–8.
#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>
#include <vector>

#include "kirimasim/sir/module_path.h"
#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/PortSymbols.h"

namespace kirimasim::front {
sir::SubroutineId Lowerer::modulePathCondition(const ast::Expression& expression,
                                               slang::SourceLocation sl) {
    if (!expression.type->isIntegral())
        unsupported(sl, "specify condition other than a pure static integral expression");
    auto check = ast::makeVisitor([&](auto& visitor, const auto& node) {
        if constexpr (std::is_base_of_v<ast::Expression, std::remove_cvref_t<decltype(node)>>) {
            const ast::Expression& e = node;
            switch (e.kind) {
                case ast::ExpressionKind::NamedValue: {
                    const auto& value = e.template as<ast::NamedValueExpression>().symbol;
                    if (value.kind == ast::SymbolKind::Parameter ||
                        value.kind == ast::SymbolKind::Specparam)
                        break;
                    if ((value.kind != ast::SymbolKind::Variable &&
                         value.kind != ast::SymbolKind::Net) ||
                        !inSlot(value) || declaringInstance(value) != instance_ ||
                        !value.getType().isIntegral() || !slots_.contains(&value))
                        unsupported(e.sourceRange.start(),
                                    "nonlocal or nonintegral specify condition binding");
                    const auto old = slots_.at(&value);
                    if (design_.slot(old).instance != instance_) {
                        // A collapsed input still has a local declaration. Preserve
                        // that read-only identity as a view of the same physical net.
                        const auto names = design_.names.size();
                        const auto view = newSlot(value, "", value.getType(), {}, value.location);
                        design_.slot(view).viewOf = sir::physicalSlot(design_, old);
                        design_.slot(view).instance = instance_;
                        design_.names.resize(names);
                        const auto name = value.getHierarchicalPath();
                        for (auto& entry : design_.names)
                            if (entry.path == name) entry.slot = view;
                        slots_[&value] = view;
                    }
                    break;
                }
                case ast::ExpressionKind::ElementSelect:
                    if (!constInt(e.template as<ast::ElementSelectExpression>().selector()))
                        unsupported(e.sourceRange.start(),
                                    "runtime selection in a specify condition");
                    break;
                case ast::ExpressionKind::RangeSelect: {
                    const auto& range = e.template as<ast::RangeSelectExpression>();
                    if (!constInt(range.left()) || !constInt(range.right()))
                        unsupported(e.sourceRange.start(),
                                    "runtime selection in a specify condition");
                    break;
                }
                case ast::ExpressionKind::IntegerLiteral:
                case ast::ExpressionKind::UnbasedUnsizedIntegerLiteral:
                case ast::ExpressionKind::Conversion:
                case ast::ExpressionKind::BinaryOp:
                case ast::ExpressionKind::ConditionalOp:
                case ast::ExpressionKind::Concatenation:
                case ast::ExpressionKind::Replication: break;
                case ast::ExpressionKind::UnaryOp: {
                    const auto op = e.template as<ast::UnaryExpression>().op;
                    if (op == ast::UnaryOperator::Preincrement ||
                        op == ast::UnaryOperator::Predecrement ||
                        op == ast::UnaryOperator::Postincrement ||
                        op == ast::UnaryOperator::Postdecrement)
                        unsupported(e.sourceRange.start(), "effectful specify condition");
                    break;
                }
                default:
                    unsupported(e.sourceRange.start(), "specify condition expression of this form");
            }
        }
        visitor.visitDefault(node);
    });
    expression.visit(check);
    sir::Subroutine condition;
    condition.name = design_.instance(instance_).scopeName + ".$path" +
                     std::to_string(design_.subroutines.size());
    condition.kind = sir::SubroutineKind::PathCondition;
    condition.instance = instance_;
    condition.returnType = sir::u4(1);
    condition.loc = loc(sl);
    condition.blocks.resize(1);
    const auto id = sir::SubroutineId{static_cast<std::uint32_t>(design_.subroutines.size())};
    design_.subroutines.push_back(std::move(condition));
    Body saved = std::move(b_);
    b_ = Body{};
    b_.isSub = true;
    b_.sub = id;
    b_.scale = saved.scale;
    try {
        const auto value = coerce(lowerExpr(expression), sir::u4(1), sl);
        terminate(sir::Op::Ret, {value.op}, sl);
    } catch (...) {
        b_ = std::move(saved);
        throw;
    }
    b_ = std::move(saved);
    return id;
}

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
                const auto* condition = path.getConditionExpr();
                const auto enable = condition != nullptr    ? sir::ModulePathEnable::Condition
                                    : path.isStateDependent ? sir::ModulePathEnable::IfNone
                                                            : sir::ModulePathEnable::Always;
                if (enable != sir::ModulePathEnable::Always &&
                    path.edgeIdentifier != ast::EdgeKind::None)
                    unsupported(path.location, "edge-sensitive state-dependent specify path");
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
                const auto evaluator = condition != nullptr
                                           ? modulePathCondition(*condition, path.location)
                                           : sir::SubroutineId{sir::kNone};
                for (const auto source : inputs)
                    for (const auto destination : outputs) {
                        sir::ModulePath row;
                        row.instance = instance;
                        row.source = source;
                        row.destination = destination;
                        row.edge = edge;
                        row.delays = delays.value_or(sir::ModulePathDelays{});
                        row.loc = loc(path.location);
                        row.enable = enable;
                        row.condition = evaluator;
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
    std::map<std::tuple<sir::InstanceId, sir::SlotId, sir::SlotId>, std::uint32_t> groups;
    for (auto& pending : modulePaths_) {
        auto& row = pending.row;
        if (row.enable == sir::ModulePathEnable::Always) continue;
        const auto key = std::tuple{row.instance, row.source.slot, row.destination.slot};
        const auto [at, inserted] =
            groups.try_emplace(key, static_cast<std::uint32_t>(groups.size()));
        (void)inserted;
        row.group = at->second;
    }
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
                           other.edge == row.edge && other.enable == row.enable &&
                           other.condition == row.condition;
                }))
                unsupported(at, "duplicate scalar specify path binding");
            if (std::ranges::any_of(modulePaths_, [&](const auto& other) {
                    return row.enable == sir::ModulePathEnable::IfNone &&
                           other.row.instance == row.instance && other.row.source == row.source &&
                           other.row.destination == row.destination &&
                           (other.row.enable == sir::ModulePathEnable::Always ||
                            other.row.edge != sir::Edge::Any);
                }))
                unsupported(at, "specify fallback with an unconditional or edge-sensitive path");
            design_.modulePaths.push_back(row);
        } catch (const Unsupported&) {
            continue;
        }
    }
}
}  // namespace kirimasim::front
