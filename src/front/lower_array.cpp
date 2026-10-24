// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Array methods share typed traversal and value operations (VALUE_SEMANTICS 7.9).
// Source evaluation, traversal and stable ordering are determined here; the
// execution tier only transports ordinary SIR and the cell-value swap API.
#include "lowerer.h"

#include "slang/ast/expressions/AssignmentExpressions.h"
#include "slang/ast/types/AllTypes.h"

namespace kirimasim::front {

std::optional<Val> Lowerer::lowerArrayMethod(const ast::CallExpression& call,
                                             slang::SourceLocation sl) {
    if (!call.isSystemCall() || call.arguments().empty()) return std::nullopt;
    const std::string_view name = call.getSubroutineName();
    const bool reduction =
        name == "sum" || name == "product" || name == "and" || name == "or" || name == "xor";
    const bool extrema = name == "min" || name == "max";
    const bool ordering = name == "sort" || name == "rsort" || name == "reverse";
    if (!reduction && !extrema && !ordering) return std::nullopt;
    const auto* self = call.arguments()[0];
    if (self->kind == ast::ExpressionKind::Assignment &&
        self->as<ast::AssignmentExpression>().isLValueArg())
        self = &self->as<ast::AssignmentExpression>().left();
    const auto& canonical = self->type->getCanonicalType();
    if (!canonical.isUnpackedArray()) return std::nullopt;
    const auto& builtin = std::get<ast::CallExpression::SystemCallInfo>(call.subroutine);
    if (builtin.subroutine.get() !=
        builtin.scope->getCompilation().getSystemMethod(canonical.kind, name))
        return std::nullopt;

    const auto [with, item] = builtin.getIteratorInfo();
    if (ordering && with && expressionHasSideEffects(*with))
        unsupported(sl, "ordering with an effectful or unverified-pure key");
    const auto element = typeOf(*canonical.getArrayElementType(), sl);
    const auto result = ordering ? sir::voidType() : typeOf(*call.type, sl);
    const auto keyType = with ? typeOf(*with->type, sl) : element;
    const bool realResult =
        result.kind == sir::TypeKind::Real || result.kind == sir::TypeKind::ShortReal;
    const bool realKey =
        keyType.kind == sir::TypeKind::Real || keyType.kind == sir::TypeKind::ShortReal;
    if (reduction && !sir::isIntegral(result) &&
        !(realResult && (name == "sum" || name == "product")))
        unsupported(sl, "array reduction result of this type");
    if (!reduction && name != "reverse" && !sir::isIntegral(keyType) && !realKey &&
        keyType.kind != sir::TypeKind::String)
        unsupported(sl, "array method comparison key of this type");

    const auto indexType = sir::u2(64);
    const auto zero = constant(indexType, 0), one = constant(indexType, 1);
    const bool dynamic = isDynamicContainer(canonical);
    bool heapSource = dynamic;
    Val source{}, count{}, receiver{};
    std::optional<Storage> fixed;
    std::optional<slang::ConstantRange> range;
    if (dynamic) {
        const auto type = typeOf(canonical, sl);
        if (ordering) {
            receiver = containerReference(*self, false, sl);
            source = emit(sir::Op::LoadRef, type, {receiver.op}, sl);
        } else {
            source = lowerExpr(*self);
            if (extrema) source = snapshotValue(source, sl);
        }
        count = emit(sir::Op::ContainerSize, indexType, {source.op}, sl);
    } else {
        if (canonical.kind != ast::SymbolKind::FixedSizeUnpackedArrayType)
            unsupported(sl, "array method on this source representation");
        range = canonical.as<ast::FixedSizeUnpackedArrayType>().range;
        fixed = storageOf(*self, sl);
        if (!fixed) unsupported(sl, "array method on this source representation");
        count = constant(indexType, range->width());
    }
    const auto fixedPlace = [&](const Val& index) {
        auto p = storagePlace(*fixed);
        const auto home = storageBase(*fixed);
        p.kind = Place::Kind::Element;
        p.home = home.home;
        p.slot = home.slot;
        p.frame = home.frame;
        p.type = element;
        p.indices = fixed->elem;
        const auto base = constant(sir::s2(64), static_cast<std::uint64_t>(range->left));
        const auto offset = coerce(index, sir::s2(64), sl);
        const auto actual = emit(range->left <= range->right ? sir::Op::Add : sir::Op::Sub,
                                 sir::s2(64), {base.op, offset.op}, sl);
        p.indices.push_back(actual.op);
        p.elem.reset();
        return p;
    };
    const auto read = [&](const Val& index) {
        return heapSource ? emit(sir::Op::ContainerRead, element, {source.op, index.op}, sl)
                          : loadPlace(fixedPlace(index), sl);
    };
    const auto forRange = [&](const Val& first, const Val& limit, const auto& action) {
        const auto counter = newFrameVar(indexType);
        emitVoid(sir::Op::Fstore, {sir::Operand::frame(counter), first.op}, sl);
        const auto head = newBlock(), body = newBlock(), done = newBlock();
        jump(head, sl);
        setBlock(head);
        const auto current = emit(sir::Op::Fload, indexType, {sir::Operand::frame(counter)}, sl);
        const auto more = emit(sir::Op::Lt, sir::u2(1), {current.op, limit.op}, sl);
        terminate(sir::Op::BrIf, {more.op, sir::Operand::block(body), sir::Operand::block(done)},
                  sl);
        setBlock(body);
        action(current);
        const auto next = emit(sir::Op::Add, indexType, {current.op, one.op}, sl);
        emitVoid(sir::Op::Fstore, {sir::Operand::frame(counter), next.op}, sl);
        jump(head, sl);
        setBlock(done);
    };
    const auto buffer = [&](sir::Type valueType) {
        const auto type = sequenceBufferType(valueType);
        const auto frame = newFrameVar(type);
        emitVoid(sir::Op::Fstore, {sir::Operand::frame(frame), constant(type, 0).op}, sl);
        return emit(sir::Op::RefMake, sir::refType(type.detail), {sir::Operand::frame(frame)}, sl);
    };
    const auto bufferType = [&](const Val& ref) { return sir::queueType(ref.type.detail); };
    const auto bufferValue = [&](const Val& ref) {
        return emit(sir::Op::LoadRef, bufferType(ref), {ref.op}, sl);
    };
    const auto push = [&](const Val& ref, const Val& value) {
        emitVoid(sir::Op::QueuePushBack, {sir::Operand::typeRow(ref.type.detail), ref.op, value.op},
                 sl);
    };
    const auto keyOf = [&](const Val& value) {
        if (!with) return value;
        const auto [binding, inserted] = iterators_.try_emplace(item, value);
        struct Restore {
            decltype(iterators_)& values;
            decltype(iterators_)::iterator binding;
            std::optional<Val> old;
            ~Restore() {
                if (old)
                    binding->second = std::move(*old);
                else
                    values.erase(binding);
            }
        } restore{iterators_, binding,
                  inserted ? std::nullopt : std::optional<Val>{std::move(binding->second)}};
        if (!inserted) binding->second = value;
        return lowerExpr(*with);
    };

    if (reduction) {
        const auto accumulator = newFrameVar(result);
        auto identity = realResult ? realConstant(result, name == "product" ? 1 : 0)
                                   : constant(result, name == "product" ? 1 : 0);
        if (name == "and") identity = emit(sir::Op::Not, result, {identity.op}, sl);
        emitVoid(sir::Op::Fstore, {sir::Operand::frame(accumulator), identity.op}, sl);
        const auto op = name == "sum"       ? sir::Op::Add
                        : name == "product" ? sir::Op::Mul
                        : name == "and"     ? sir::Op::And
                        : name == "or"      ? sir::Op::Or
                                            : sir::Op::Xor;
        forRange(zero, count, [&](const Val& index) {
            const auto value = coerce(keyOf(read(index)), result, sl);
            const auto first = newBlock(), next = newBlock(), join = newBlock();
            const auto isFirst = emit(sir::Op::Eq, sir::u2(1), {index.op, zero.op}, sl);
            terminate(sir::Op::BrIf,
                      {isFirst.op, sir::Operand::block(first), sir::Operand::block(next)}, sl);
            setBlock(first);
            emitVoid(sir::Op::Fstore, {sir::Operand::frame(accumulator), value.op}, sl);
            jump(join, sl);
            setBlock(next);
            const auto old = emit(sir::Op::Fload, result, {sir::Operand::frame(accumulator)}, sl);
            const auto combined = emit(op, result, {old.op, value.op}, sl);
            emitVoid(sir::Op::Fstore, {sir::Operand::frame(accumulator), combined.op}, sl);
            jump(join, sl);
            setBlock(join);
        });
        return emit(sir::Op::Fload, result, {sir::Operand::frame(accumulator)}, sl);
    }

    if (extrema) {
        if (!dynamic) {
            const auto captured = buffer(element);
            forRange(zero, count, [&](const Val& index) { push(captured, read(index)); });
            source = bufferValue(captured);
            heapSource = true;
        }
        const auto chosen = newFrameVar(element), best = newFrameVar(keyType);
        forRange(zero, count, [&](const Val& index) {
            const auto value = read(index), key = keyOf(value);
            const auto first = emit(sir::Op::Eq, sir::u2(1), {index.op, zero.op}, sl);
            const auto take = newBlock(), compare = newBlock(), next = newBlock();
            terminate(sir::Op::BrIf,
                      {first.op, sir::Operand::block(take), sir::Operand::block(compare)}, sl);
            setBlock(compare);
            const auto previous = emit(sir::Op::Fload, keyType, {sir::Operand::frame(best)}, sl);
            const auto better =
                compareValues(name == "min" ? sir::Op::Lt : sir::Op::Gt, key, previous, sl);
            terminate(sir::Op::BrIf,
                      {better.op, sir::Operand::block(take), sir::Operand::block(next)}, sl);
            setBlock(take);
            emitVoid(sir::Op::Fstore, {sir::Operand::frame(chosen), value.op}, sl);
            emitVoid(sir::Op::Fstore, {sir::Operand::frame(best), key.op}, sl);
            jump(next, sl);
            setBlock(next);
        });
        const auto output = buffer(element);
        const auto present = emit(sir::Op::Ne, sir::u2(1), {count.op, zero.op}, sl);
        const auto take = newBlock(), done = newBlock();
        terminate(sir::Op::BrIf, {present.op, sir::Operand::block(take), sir::Operand::block(done)},
                  sl);
        setBlock(take);
        push(output, emit(sir::Op::Fload, element, {sir::Operand::frame(chosen)}, sl));
        jump(done, sl);
        setBlock(done);
        return coerce(bufferValue(output), result, sl);
    }

    const auto swap = [&](const Val& first, const Val& second) {
        if (dynamic) {
            emitVoid(sir::Op::ContainerSwapValues,
                     {sir::Operand::typeRow(source.type.detail), receiver.op, first.op, second.op},
                     sl);
        } else {
            const auto a = fixedPlace(first), b = fixedPlace(second);
            const auto av = loadPlace(a, sl), bv = loadPlace(b, sl);
            const auto move = [&](const Place& place, const Val& value) {
                // Move owned handles as values, without language assignment's deep copy.
                std::vector<sir::Operand> args{place.home == Place::Home::Slot
                                                   ? sir::Operand::slot(place.slot)
                                                   : sir::Operand::frame(place.frame)};
                args.insert(args.end(), place.indices.begin(), place.indices.end());
                args.push_back(value.op);
                emitVoid(place.home == Place::Home::Ref ? sir::Op::StoreRefElem
                                                        : sir::Op::StoreElem,
                         std::move(args), sl);
                if (place.home == Place::Home::Slot) noteWrite(rangeOf(place));
            };
            move(a, bv);
            move(b, av);
        }
    };
    if (name == "reverse") {
        const auto half = emit(sir::Op::Div, indexType, {count.op, constant(indexType, 2).op}, sl);
        forRange(zero, half, [&](const Val& index) {
            const auto last = emit(sir::Op::Sub, indexType, {count.op, one.op}, sl);
            swap(index, emit(sir::Op::Sub, indexType, {last.op, index.op}, sl));
        });
    } else {
        const auto keys = buffer(keyType);
        forRange(zero, count, [&](const Val& index) { push(keys, keyOf(read(index))); });
        const auto keyValues = bufferValue(keys);
        forRange(one, count, [&](const Val& index) {
            const auto position = newFrameVar(indexType);
            emitVoid(sir::Op::Fstore, {sir::Operand::frame(position), index.op}, sl);
            const auto head = newBlock(), compare = newBlock(), move = newBlock(),
                       done = newBlock();
            jump(head, sl);
            setBlock(head);
            const auto at = emit(sir::Op::Fload, indexType, {sir::Operand::frame(position)}, sl);
            const auto more = emit(sir::Op::Ne, sir::u2(1), {at.op, zero.op}, sl);
            terminate(sir::Op::BrIf,
                      {more.op, sir::Operand::block(compare), sir::Operand::block(done)}, sl);
            setBlock(compare);
            const auto before = emit(sir::Op::Sub, indexType, {at.op, one.op}, sl);
            const auto current = emit(sir::Op::ContainerRead, keyType, {keyValues.op, at.op}, sl);
            const auto previous =
                emit(sir::Op::ContainerRead, keyType, {keyValues.op, before.op}, sl);
            const auto precedes =
                compareValues(name == "sort" ? sir::Op::Lt : sir::Op::Gt, current, previous, sl);
            terminate(sir::Op::BrIf,
                      {precedes.op, sir::Operand::block(move), sir::Operand::block(done)}, sl);
            setBlock(move);
            swap(before, at);
            emitVoid(sir::Op::ContainerSwapValues,
                     {sir::Operand::typeRow(keys.type.detail), keys.op, before.op, at.op}, sl);
            emitVoid(sir::Op::Fstore, {sir::Operand::frame(position), before.op}, sl);
            jump(head, sl);
            setBlock(done);
        });
    }
    return Val{sir::Operand::temp(sir::TempId{0}), sir::voidType()};
}

}  // namespace kirimasim::front
