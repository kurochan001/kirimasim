// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Procedural sampled values, IEEE 1800-2023 16.5.1 and 16.9.3.
#include <algorithm>

#include "lowerer.h"
#include "slang/analysis/DataFlowAnalysis.h"
#include "slang/ast/ASTVisitor.h"

namespace kirimasim::front {

std::optional<sir::SampleClock> Lowerer::sampleClock(const ast::CallExpression& call) {
    if (call.getSubroutineName() == "$sampled") return std::nullopt;
    const auto sl = call.sourceRange.start();
    const ast::TimingControl* timing = nullptr;
    if (call.arguments().size() == 2) {
        const auto* event = call.arguments()[1]->as_if<ast::ClockingEventExpression>();
        if (event) timing = &event->timingControl;
    } else if (b_.procedure) {
        // Resolve the actual elaborated instance; canonical analysis of a
        // sibling can name a different clock and different port storage.
        auto [it, fresh] = inferredClocks_.try_emplace(b_.procedure, nullptr);
        if (fresh) {
            slang::analysis::AnalysisContext context(analysis_);
            slang::analysis::DefaultDFA dfa(context, *b_.procedure, false);
            dfa.run();
            ast::EvalContext eval(*b_.procedure);
            it->second = dfa.inferClock(context, *b_.procedure, eval, nullptr);
            for (auto* stmt : dfa.getTimedStatements())
                if (auto* timed = stmt->as_if<ast::TimedStatement>();
                    timed && timed->timing.kind == ast::TimingControlKind::EventList)
                    it->second = nullptr;
        }
        timing = it->second;
    }
    const auto* event = timing ? timing->as_if<ast::SignalEventControl>() : nullptr;
    if (!event || event->iffCondition ||
        (event->edge != ast::EdgeKind::PosEdge && event->edge != ast::EdgeKind::NegEdge))
        unsupported(sl, "sample history without a single unguarded posedge or negedge");
    const auto range = eventSlot(event->expr);
    if (!range || event->expr.type->getBitWidth() != 1 || !design_.slot(range->slot).dims.empty())
        unsupported(sl, "sample history without a static one-bit clock");
    return sir::SampleClock{sir::physicalRange(design_, *range),
                            event->edge == ast::EdgeKind::PosEdge ? sir::Edge::Pos
                                                                  : sir::Edge::Neg};
}

const sir::SampledInput& Lowerer::sampledInput(const ast::ValueSymbol& source,
                                               const std::optional<sir::SampleClock>& clock,
                                               slang::SourceLocation sl) {
    const auto key =
        std::tuple{instance_, &source, clock ? clock->range.slot : sir::SlotId{sir::kNone},
                   clock ? clock->range.lsb : 0U, clock ? clock->edge : sir::Edge::Any};
    if (const auto it = sampleBindings_.find(key); it != sampleBindings_.end())
        return design_.sampledInputs[it->second];
    const auto slot = slotOf(source, sl);
    const auto type = design_.slot(slot).type;
    const auto path = source.getHierarchicalPath();
    const auto physical = sir::physicalSlot(design_, slot);
    const auto declaration = [&](const std::string& name) {
        return std::ranges::find_if(design_.names, [&](const auto& entry) {
            return entry.path == name && sir::physicalSlot(design_, entry.slot) == physical;
        });
    };
    auto name = declaration(path);
    if (name == design_.names.end()) name = declaration(design_.slot(slot).name);
    if (name == design_.names.end()) unsupported(sl, "sampled input without a source declaration");
    sir::SampledInput row;
    row.instance = instance_;
    row.sourceDeclaration = static_cast<std::uint32_t>(name - design_.names.begin());
    row.source = {physical};
    row.clock = clock;
    const auto* init = source.kind == ast::SymbolKind::Net ? nullptr : declaredInitializer(source);
    if (init) {
        auto cv = constantValue(*init);
        if (!cv || !cv->isInteger()) unsupported(sl, "nonconstant sampled declaration initializer");
        row.initial = constantFrom(*cv, type, sl).op.asConst();
    } else
        row.initial =
            (sir::isFourState(type) ? allXConstant(type) : constant(type, 0)).op.asConst();

    const auto internal = [&] {
        const auto id = static_cast<sir::SlotId>(design_.slots.size());
        sir::Slot storage;
        storage.name = "$sample." + std::to_string(id.index());
        storage.type = type;
        storage.instance = instance_;
        design_.slots.push_back(std::move(storage));
        return id;
    };
    row.current = internal();
    if (row.clock) row.clock->previous = internal();
    sampleBindings_.emplace(key, design_.sampledInputs.size());
    design_.sampledInputs.push_back(row);
    return design_.sampledInputs.back();
}

Val Lowerer::lowerSampled(const ast::CallExpression& call) {
    const auto sl = call.sourceRange.start();
    const auto& expression = *call.arguments()[0];
    const auto clock = sampleClock(call);
    SampleReads reads;
    auto check = ast::makeVisitor([&](auto& v, const auto& node) {
        if constexpr (std::is_base_of_v<ast::Expression, std::remove_cvref_t<decltype(node)>>) {
            const ast::Expression& e = node;
            const auto at = e.sourceRange.start();
            checkStack(at);
            if (!e.type->isIntegral()) unsupported(at, "nonintegral sampled operand");
            switch (e.kind) {
                case ast::ExpressionKind::NamedValue:
                case ast::ExpressionKind::HierarchicalValue: {
                    const auto& sym = e.template as<ast::ValueExpressionBase>().symbol;
                    if (sym.kind == ast::SymbolKind::Parameter ||
                        sym.kind == ast::SymbolKind::Specparam ||
                        sym.kind == ast::SymbolKind::EnumValue)
                        break;
                    if ((sym.kind != ast::SymbolKind::Variable &&
                         sym.kind != ast::SymbolKind::Net &&
                         sym.kind != ast::SymbolKind::FormalArgument) ||
                        !inSlot(sym) || findFrameVar(sym))
                        unsupported(at, "sampled operand without static packed storage");
                    const auto& row = sampledInput(sym.template as<ast::ValueSymbol>(), clock, at);
                    reads.bindings.emplace(
                        &e, std::pair{row.current,
                                      row.clock ? row.clock->previous : sir::SlotId{sir::kNone}});
                    break;
                }
                case ast::ExpressionKind::ElementSelect:
                    if (!constInt(e.template as<ast::ElementSelectExpression>().selector()))
                        unsupported(at, "runtime selection in a sampled operand");
                    break;
                case ast::ExpressionKind::RangeSelect: {
                    const auto& range = e.template as<ast::RangeSelectExpression>();
                    if (!constInt(range.left()) || !constInt(range.right()))
                        unsupported(at, "runtime selection in a sampled operand");
                    break;
                }
                case ast::ExpressionKind::UnaryOp:
                    if (expressionHasSideEffects(e)) unsupported(at, "effectful sampled operand");
                    break;
                case ast::ExpressionKind::IntegerLiteral:
                case ast::ExpressionKind::UnbasedUnsizedIntegerLiteral:
                case ast::ExpressionKind::Conversion:
                case ast::ExpressionKind::MemberAccess:
                case ast::ExpressionKind::BinaryOp:
                case ast::ExpressionKind::ConditionalOp:
                case ast::ExpressionKind::Inside:
                case ast::ExpressionKind::ValueRange:
                case ast::ExpressionKind::Concatenation:
                case ast::ExpressionKind::Replication: break;
                default: unsupported(at, "sampled operand of this form");
            }
        }
        v.visitDefault(node);
    });
    expression.visit(check);
    struct RestoreReads {
        const SampleReads*& context;
        const SampleReads* saved;
        ~RestoreReads() { context = saved; }
    } restore{sampleReads_, std::exchange(sampleReads_, &reads)};
    auto current = lowerExpr(expression);
    if (!clock) return current;
    reads.previous = true;
    const auto previous = lowerExpr(expression);
    return emit(call.getSubroutineName() == "$stable" ? sir::Op::Ceq : sir::Op::Cne, sir::u2(1),
                {current.op, previous.op}, sl);
}

}  // namespace kirimasim::front
