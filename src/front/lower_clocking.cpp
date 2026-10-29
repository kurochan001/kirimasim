// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "lowerer.h"
#include "slang/ast/expressions/MiscExpressions.h"

namespace kirimasim::front {

void Lowerer::declareClocking(const ast::ClockingBlockSymbol& block) {
    if (block.isGlobal) unsupported(block.location, "global clocking block");
    for (const auto& member : block.members()) {
        if (member.kind != ast::SymbolKind::ClockVar)
            unsupported(member.location, "this clocking block member");
        const auto& var = member.as<ast::ClockVarSymbol>();
        if (var.direction != ast::ArgumentDirection::In)
            unsupported(var.location, "output or inout clocking variable");
        if (!var.getType().isIntegral() || var.getType().isUnpackedArray())
            unsupported(var.location, "nonintegral clocking input");
        clockvars_.insert(
            slotOf(var, var.location));  // The source is sampled, not initialized here.
    }
    sir::Slot event;
    event.name = block.getHierarchicalPath() + ".$event";
    event.type = {sir::TypeKind::Event, 0};
    event.instance = instance_;
    const auto id = sir::SlotId{static_cast<std::uint32_t>(design_.slots.size())};
    design_.names.push_back({event.name, id});
    design_.slots.push_back(std::move(event));
    clockingEvents_.emplace(&block, id);
}

void Lowerer::lowerClocking(const ast::ClockingBlockSymbol& block) {
    const auto event = clockingEvents_.find(&block);
    if (event == clockingEvents_.end()) return;  // Declaration already diagnosed.
    const auto range = [&](const ast::Expression& expr) {
        auto bits = staticNetBits(expr, false);
        if (!bits || bits->empty())
            unsupported(expr.sourceRange.start(), "clocking source without static packed storage");
        auto part = bits->front();
        for (std::size_t i = 0; i < bits->size(); ++i)
            if ((*bits)[i].slot != part.slot || (*bits)[i].lsb != part.lsb + i)
                unsupported(expr.sourceRange.start(), "noncontiguous clocking source");
        part.width = static_cast<std::uint32_t>(bits->size());
        if (expr.kind == ast::ExpressionKind::NamedValue ||
            expr.kind == ast::ExpressionKind::HierarchicalValue)
            part.width = 0;  // Whole reads retain signedness; packed selections are unsigned.
        part.numElems = 0;   // Scalar SIR ranges use no unpacked-element selector.
        return part;
    };
    const auto oneStep = [&](ast::ClockingSkew skew, slang::SourceLocation at) {
        if (skew.edge != ast::EdgeKind::None ||
            (skew.delay != nullptr && skew.delay->kind != ast::TimingControlKind::OneStepDelay))
            unsupported(at, "clocking input skew other than #1step");
    };
    const ast::TimingControl* timing = &block.getEvent();
    if (timing->kind == ast::TimingControlKind::EventList) {
        const auto events = timing->as<ast::EventListControl>().events;
        if (events.size() != 1) unsupported(block.location, "compound clocking event");
        timing = events.front();
    }
    if (timing->kind != ast::TimingControlKind::SignalEvent)
        unsupported(block.location, "this clocking event control");
    const auto& trigger = timing->as<ast::SignalEventControl>();
    if (trigger.iffCondition != nullptr) unsupported(block.location, "guarded clocking event");
    sir::ClockingBlock result;
    result.clock = range(trigger.expr);
    result.event = event->second;
    result.instance = instance_;
    result.loc = loc(block.location);
    switch (trigger.edge) {
        case ast::EdgeKind::None: result.edge = sir::Edge::Any; break;
        case ast::EdgeKind::PosEdge: result.edge = sir::Edge::Pos; break;
        case ast::EdgeKind::NegEdge: result.edge = sir::Edge::Neg; break;
        default: unsupported(block.location, "this clocking edge qualifier");
    }
    for (const auto& member : block.members()) {
        const auto& var = member.as<ast::ClockVarSymbol>();
        oneStep(var.inputSkew.hasValue() ? var.inputSkew : block.getDefaultInputSkew(),
                var.location);
        const auto* source = var.getInitializer();
        if (source == nullptr) unsupported(var.location, "clocking input without a source");
        result.inputs.push_back({range(*source), slots_.at(&var)});
    }
    design_.clockingBlocks.push_back(std::move(result));
}

std::optional<sir::SlotId> Lowerer::clockingEvent(const ast::Expression& expr) const {
    if (expr.kind != ast::ExpressionKind::ArbitrarySymbol) return std::nullopt;
    const auto* symbol = expr.as<ast::ArbitrarySymbolExpression>().symbol.get();
    const auto found = clockingEvents_.find(symbol);
    return found == clockingEvents_.end() ? std::nullopt : std::optional(found->second);
}

}  // namespace kirimasim::front
