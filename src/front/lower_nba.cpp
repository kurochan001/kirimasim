// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Approved event-NBA contract: RHS, typed count, target selectors, registration.
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "kirimasim/sir/nba.h"
#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"

namespace kirimasim::front {
namespace {
using sir::Op;
using sir::Operand;
using sir::Type;
}  // namespace

Type Lowerer::operandType(const Operand& operand) {
    if (operand.kind == Operand::Kind::Const) return design_.constant(operand.asConst()).type;
    if (operand.kind == Operand::Kind::Temp)
        for (const auto& bb : blocks()) {
            for (const auto& param : bb.params)
                if (param.temp.index() == operand.id) return param.type;
            for (const auto& in : bb.instrs)
                if (in.result.index() == operand.id) return in.type;
        }
    throw std::logic_error("missing captured selector type");
}

sir::NbaValueId Lowerer::captureNbaValue(EventNbaCapture& event, const Val& value,
                                         slang::SourceLocation sl) {
    Val frozen = value;
    if (frozen.op.kind == Operand::Kind::Slot)
        frozen = emit(Op::Load, value.type, {value.op}, sl);
    else if (frozen.op.kind == Operand::Kind::Frame)
        frozen = emit(Op::Fload, value.type, {value.op}, sl);
    if (frozen.op.kind != Operand::Kind::Temp && frozen.op.kind != Operand::Kind::Const)
        throw std::logic_error("event-NBA capture is not a value");
    const auto id = sir::NbaValueId{static_cast<std::uint32_t>(event.values.size())};
    event.layout.values.push_back(frozen.type);
    event.values.push_back(frozen.op);
    return id;
}

void Lowerer::captureNbaWrite(EventNbaCapture& event, const Place& p, const Val& value,
                              slang::SourceLocation sl) {
    if (p.kind == Place::Kind::Frame || p.home == Place::Home::Frame)
        unsupported(sl, "nonblocking assignment to an automatic variable");
    const auto v = captureNbaValue(event, coerce(value, p.type, sl), sl);
    const auto indices = [&] {
        std::vector<sir::NbaValueId> result;
        result.reserve(p.indices.size());
        for (const auto& index : p.indices)
            result.push_back(captureNbaValue(event, {index, operandType(index)}, sl));
        return result;
    };
    if (p.home == Place::Home::Ref || p.kind == Place::Kind::Ref) {
        Place parent = p;
        parent.kind = Place::Kind::Ref;
        parent.indices.clear();
        if (p.kind == Place::Kind::Bits) parent.type = homeType(p);
        const auto reference = p.indices.empty() ? referencePlace(parent, false, sl)
                                                 : emit(Op::Fload, frameVar(p.frame).type,
                                                        {Operand::frame(p.frame)}, sl);
        const auto ref = captureNbaValue(event, reference, sl);
        const auto dims = p.indices.empty() ? std::vector<sir::ArrayDim>{} : frameVar(p.frame).dims;
        const auto selected = indices();
        if (p.kind == Place::Kind::Bits) {
            const auto pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            event.layout.targets.push_back(
                sir::NbaRefBitsTarget{ref, p.leaf, parent.type, captureNbaValue(event, pos, sl), v,
                                      dims, selected, p.windowLsb, p.windowWidth});
        } else {
            event.layout.targets.push_back(
                sir::NbaRefTarget{ref, p.leaf, p.type, v, dims, selected});
        }
        return;
    }
    switch (p.kind) {
        case Place::Kind::Slot:
            event.layout.targets.push_back(sir::NbaSlotTarget{{p.slot}, v});
            break;
        case Place::Kind::Element:
            event.layout.targets.push_back(sir::NbaElementTarget{p.slot, indices(), v});
            break;
        case Place::Kind::Bits: {
            const auto pos =
                p.pos ? *p.pos : constant(sir::s2(64), static_cast<std::uint64_t>(p.lsb));
            const auto selected = indices();
            event.layout.targets.push_back(
                sir::NbaBitsTarget{{p.slot, 0, 0, p.windowLsb, p.windowWidth},
                                   selected,
                                   captureNbaValue(event, pos, sl),
                                   v});
            break;
        }
        default: throw std::logic_error("unflattened event-NBA destination");
    }
}

Val Lowerer::eventNbaCount(const ast::TimingControl& control, slang::SourceLocation sl) {
    if (control.kind != ast::TimingControlKind::RepeatedEvent) return constant(sir::u2(32), 1);
    const auto count = lowerExpr(control.as<ast::RepeatedEventControl>().expr);
    if (!sir::isIntegral(count.type)) unsupported(sl, "repeat event count of this type");
    return count;
}

void Lowerer::finishEventNba(EventNbaCapture& capture, const ast::TimingControl& timing,
                             const Val& count, slang::SourceLocation sl) {
    const auto& control = timing.kind == ast::TimingControlKind::RepeatedEvent
                              ? timing.as<ast::RepeatedEventControl>().event
                              : timing;
    std::vector<const ast::TimingControl*> items;
    if (control.kind == ast::TimingControlKind::SignalEvent)
        items.push_back(&control);
    else if (control.kind == ast::TimingControlKind::EventList)
        for (const auto* item : control.as<ast::EventListControl>().events)
            items.push_back(item);
    else
        unsupported(sl, "this nonblocking event control");
    bool named = false;
    for (const auto* item : items) {
        if (item->kind != ast::TimingControlKind::SignalEvent)
            unsupported(sl, "this nonblocking event control");
        named = named || item->as<ast::SignalEventControl>().expr.type->isEvent();
    }
    const auto beforeEvents = capture.values.size();
    sir::BlockId skip = sir::kNone, done = sir::kNone;
    if (named) {
        // Relational comparison returns X for any X/Z operand; br_if takes
        // only known true. This is the existing ordinary repeat-event rule.
        const auto truth = sir::isFourState(count.type) ? sir::u4(1) : sir::u2(1);
        const auto positive = emit(Op::Gt, truth, {count.op, constant(count.type, 0).op}, sl);
        const auto bind = newBlock();
        skip = newBlock();
        done = newBlock();
        terminate(Op::BrIf, {positive.op, Operand::block(bind), Operand::block(skip)}, sl);
        setBlock(bind);
    }
    for (const auto* item : items) {
        const auto& subject = item->as<ast::SignalEventControl>();
        const auto at = subject.expr.sourceRange.start();
        if (readsAutomatic(subject.expr) ||
            (subject.iffCondition != nullptr && readsAutomatic(*subject.iffCondition)))
            error(at, "automatic variable in a nonblocking event control");
        sir::SubroutineId guard = sir::kNone;
        if (subject.iffCondition != nullptr)
            guard = waiterOf(sir::SubroutineKind::Guard, *subject.iffCondition, "iff", at, true);
        sir::Edge edge = sir::Edge::Any;
        switch (subject.edge) {
            case ast::EdgeKind::PosEdge: edge = sir::Edge::Pos; break;
            case ast::EdgeKind::NegEdge: edge = sir::Edge::Neg; break;
            case ast::EdgeKind::BothEdges: edge = sir::Edge::Both; break;
            case ast::EdgeKind::None: break;
        }
        if (subject.expr.type->isEvent()) {
            const auto value = captureNbaValue(
                capture, {eventOperand(subject.expr, at), {sir::TypeKind::Event, 0}}, at);
            capture.layout.events.push_back(sir::NbaNamedEvent{value, guard});
        } else if (const auto range = eventSlot(subject.expr)) {
            capture.layout.events.push_back(sir::NbaSlotEvent{*range, edge, guard});
        } else {
            if (!subject.expr.type->isIntegral())
                unsupported(at, "event control on an expression of this type");
            const auto watch =
                waiterOf(sir::SubroutineKind::Watch, subject.expr, "event", at, true);
            capture.layout.events.push_back(sir::NbaWatchEvent{watch, edge, guard});
        }
    }
    for (const auto& range : sir::nbaWrites(capture.layout))
        noteWrite(range);
    const auto id = sir::NbaLayoutId{static_cast<std::uint32_t>(design_.nbaLayouts.size())};
    design_.nbaLayouts.push_back(std::move(capture.layout));
    const auto publish = [&] {
        std::vector<Operand> ops{Operand::nbaLayout(id), count.op};
        ops.insert(ops.end(), capture.values.begin(), capture.values.end());
        emitVoid(Op::NbaEvent, std::move(ops), sl);
    };
    publish();
    if (named) {
        jump(done, sl);
        setBlock(skip);
        const auto null = constant({sir::TypeKind::Event, 0}, 0);
        for (auto i = beforeEvents; i < capture.values.size(); ++i)
            capture.values[i] = null.op;
        publish();
        jump(done, sl);
        setBlock(done);
    }
}

Val Lowerer::lowerEventNba(const ast::AssignmentExpression& assignment) {
    const auto sl = assignment.sourceRange.start();
    const auto& control = *assignment.timingControl;
    if (assignment.isCompound()) unsupported(sl, "timing control on a compound assignment");
    if (assignment.left().kind == ast::ExpressionKind::Streaming) {
        lowerUnpack(assignment.left().as<ast::StreamingConcatenationExpression>(),
                    assignment.right(), true, sl, nullptr, &control);
        return {Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    if (isAggregate(*assignment.left().type)) {
        const ast::Expression* rhs = &assignment.right();
        if (rhs->kind == ast::ExpressionKind::Conversion &&
            rhs->as<ast::ConversionExpression>().conversionKind ==
                ast::ConversionKind::StreamingConcat)
            rhs = &rhs->as<ast::ConversionExpression>().operand();
        if (rhs->kind == ast::ExpressionKind::Streaming)
            return lowerStreamAssignment(
                assignment, rhs->as<ast::StreamingConcatenationExpression>(), &control);
        return lowerAggregateAssignment(assignment, nullptr, &control);
    }
    const auto type = typeOf(*assignment.left().type, sl);
    const auto value = sir::isContainer(type) ? snapshotValue(lowerExpr(assignment.right()), sl)
                                              : coerce(lowerExpr(assignment.right()), type, sl);
    const auto count = eventNbaCount(control, sl);
    const auto target = placeOf(assignment.left(), sl);
    EventNbaCapture capture;
    storePlace(target, value, true, sl, std::nullopt, &capture);
    finishEventNba(capture, control, count, sl);
    return value;
}
}  // namespace kirimasim::front
