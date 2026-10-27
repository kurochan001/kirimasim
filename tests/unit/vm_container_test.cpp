// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original hand-built SIR inputs test the execution adapter independently
// of front (SIR_SPEC 5.8; 7.5, 7.10, 13.5.2). Language lowering is separate.
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory_resource>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/valsem/value.h"
#include "kirimasim/vm/vm.h"

namespace {
using namespace kirimasim::sir;
namespace k = kirimasim::kernel;

struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string out;
        std::array<char, 512> buffer{};
        while (const auto n = std::fread(buffer.data(), 1, buffer.size(), file))
            out.append(buffer.data(), n);
        return out;
    }
};

struct RootObserver {
    k::ProcessId process{0};
    bool waited = false;
    bool observed = false;
    k::HeapStats stats;
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& state = *static_cast<RootObserver*>(frame);
        auto& ctx = k::fromC(context);
        if (!state.waited) {
            state.waited = true;
            ctx.scheduler().waitDelay(state.process, 0);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        state.observed = true;
        state.stats = ctx.heap().stats();
        return static_cast<kirima_status>(k::Status::Done);
    }
};

struct Program {
    Design d;
    Program() {
        d.sources = {"container.sv"};
        d.instances = {{"top", "top", kNone}};
        Process p;
        p.name = "top.initial";
        p.loc = {0, 1, 1};
        p.blocks.resize(1);
        d.processes.push_back(std::move(p));
    }
    Type container(Type element, bool queue, std::uint64_t bound = 0) {
        TypeEntry te;
        te.kind = queue ? TypeEntryKind::Queue : TypeEntryKind::DynArray;
        te.name = "type" + std::to_string(d.types.size());
        te.value = element;
        te.maxElements = bound;
        const TypeId id{static_cast<std::uint32_t>(d.types.size())};
        d.types.push_back(std::move(te));
        return queue ? queueType(id) : dynarrayType(id);
    }
    Operand constant(Type type, std::uint64_t n, std::uint64_t b = 0) {
        Constant c;
        c.type = type;
        const auto words = isIntegral(type) ? (type.width + 63) / 64 : 1;
        for (unsigned w = 0; w < words; ++w) {
            const auto mask =
                isIntegral(type) ? k::mask(std::min(64U, type.width - w * 64)) : ~std::uint64_t{0};
            c.aWords.push_back((n ^ (0x913246587abcdef0ULL * w)) & mask);
            if (isFourState(type)) c.bWords.push_back(b & mask);
        }
        return constant(std::move(c));
    }
    Operand constant(Constant c) {
        const ConstId id{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back(std::move(c));
        return Operand::constant(id);
    }
    Operand emit(Op op, Type type, std::vector<Operand> args) {
        return emitAt(0, op, type, std::move(args));
    }
    Operand emitAt(std::size_t block, Op op, Type type, std::vector<Operand> args) {
        auto& p = d.processes[0];
        if (p.blocks.size() <= block) p.blocks.resize(block + 1);
        const TempId id = type == voidType() ? TempId{kNone} : TempId{p.numTemps++};
        p.blocks[block].instrs.push_back({op, type, id, std::move(args), {0, 1, 1}});
        return Operand::temp(id);
    }
    Operand slot(Type type) {
        const SlotId id{static_cast<std::uint32_t>(d.slots.size())};
        d.slots.push_back({"top.value" + std::to_string(id.index()), type});
        d.processes[0].writes.push_back({id});
        return Operand::slot(id);
    }
    Operand frame(Type type, Type target = voidType()) {
        auto& vars = d.processes[0].frame;
        const FrameVarId id{static_cast<std::uint32_t>(vars.size())};
        FrameVar fv;
        fv.name = "local" + std::to_string(id.index());
        fv.type = type;
        fv.target = target;
        vars.push_back(std::move(fv));
        return Operand::frame(id);
    }
    Operand load(Type type, const Operand& home) {
        return emit(home.kind == Operand::Kind::Slot ? Op::Load : Op::Fload, type, {home});
    }
    void store(const Operand& home, const Operand& value) {
        emit(home.kind == Operand::Kind::Slot ? Op::Store : Op::Fstore, voidType(), {home, value});
    }
    void end() { emit(Op::Ret, voidType(), {}); }
};

::testing::AssertionResult verifies(const Design& d) {
    const auto errors = verify(d);
    if (errors.empty()) return ::testing::AssertionSuccess();
    std::string out;
    for (const auto& error : errors)
        out += error.where + ": " + error.message + "\n";
    return ::testing::AssertionFailure() << out;
}
std::vector<k::W4> stored(const k::Context& c, const Operand& slot, Type type) {
    const auto* data = c.store().element(k::SlotId{slot.id}, 0);
    std::vector<k::W4> out;
    const auto words = isIntegral(type) ? (type.width + 63) / 64 : 1;
    out.reserve(words);
    for (std::size_t w = 0; w < words; ++w)
        out.push_back(isFourState(type) ? k::W4{data[w * 2], data[w * 2 + 1]} : k::W4{data[w], 0});
    return out;
}

TEST(VmContainer, AllInstructionsAcrossElementTypesBoundsAndStorage) {
    std::vector<Type> elements;
    for (unsigned width : {1U, 32U, 64U, 65U, 129U})
        for (auto t : {u2(width), s2(width), u4(width), s4(width)})
            elements.push_back(t);
    elements.insert(elements.end(), {{TypeKind::Real, 0},
                                     {TypeKind::ShortReal, 0},
                                     {TypeKind::String, 0},
                                     {TypeKind::Chandle, 0}});
    unsigned cases = 0;
    for (Type e : elements)
        for (unsigned bound : {0U, 1U, 3U})
            for (unsigned homeKind : {0U, 1U, 2U}) {
                SCOPED_TRACE(::testing::Message()
                             << toString(e) << '/' << bound << '/' << homeKind);
                Program p;
                const Type a = p.container(e, false), q = p.container(e, true, bound);
                std::array<Operand, 3> items;
                for (unsigned i = 0; i < items.size(); ++i) {
                    if (e.kind == TypeKind::String) {
                        Constant c;
                        c.type = e;
                        c.bytes = std::array<std::string, 3>{{"alpha", "beta", "gamma"}}[i];
                        items[i] = p.constant(std::move(c));
                    } else if (e.kind == TypeKind::Real) {
                        items[i] =
                            p.constant(e, std::bit_cast<std::uint64_t>(i == 0 ? -1.25 : 2.5));
                    } else if (e.kind == TypeKind::ShortReal) {
                        items[i] =
                            p.constant(e, std::bit_cast<std::uint32_t>(i == 0 ? -1.25F : 2.5F));
                    } else {
                        items[i] = p.constant(e,
                                              e.kind == TypeKind::Chandle ? 0
                                              : i == 1                    ? 0x5a
                                                                          : 0xa5,
                                              i == 1 ? 0x11 : 0x22);
                    }
                }
                const auto index = [&](unsigned n) { return p.constant(s4(64), n); };
                const auto packed = p.emit(Op::ContainerPack, a, {items[0], items[1], items[2]});
                const auto array = p.emit(Op::ContainerNew, a, {index(4), packed});
                const auto copied = p.emit(Op::ContainerCopy, q, {packed});
                const auto snap = p.emit(Op::ContainerSnapshot, q, {copied});
                const auto home = homeKind == 1 ? p.frame(q) : p.slot(q);
                p.store(home, copied);
                auto ref = p.emit(Op::RefMake, refType(q.detail), {home});
                if (homeKind == 2) {
                    const auto formal = p.frame(refType(q.detail), q);
                    p.store(formal, ref);
                    ref = p.emit(Op::RefMake, refType(q.detail), {formal});
                }
                std::vector<std::pair<Operand, unsigned>> checks;
                const auto record = [&](const Operand& value, Type t, unsigned expected) {
                    const auto s = p.slot(t);
                    p.store(s, value);
                    checks.push_back({s, expected});
                };
                const auto size = [&](const Operand& value, unsigned n) {
                    record(p.emit(Op::ContainerSize, u2(64), {value}), u2(64), 100 + n);
                };
                size(array, 4);
                record(p.emit(Op::ContainerRead, e, {array, index(3)}), e, 3);
                const unsigned count = bound == 0 ? 3 : std::min(3U, bound);
                size(copied, count);
                const auto cell = p.emit(Op::ContainerCell, refType(),
                                         {Operand::typeRow(q.detail), ref, index(0)});
                std::vector<std::pair<int, unsigned>> model;
                model.reserve(count + 2);
                for (unsigned i = 0; i < count; ++i)
                    model.push_back({static_cast<int>(i), i});
                const auto trim = [&] {
                    if (bound != 0 && model.size() > bound) model.resize(bound);
                };
                p.emit(Op::QueuePushFront, voidType(), {Operand::typeRow(q.detail), ref, items[2]});
                model.insert(model.begin(), {-1, 2});
                trim();
                p.emit(Op::StoreRef, voidType(), {cell, items[1]});
                for (auto& m : model)
                    if (m.first == 0) m.second = 1;
                record(p.emit(Op::LoadRef, e, {cell}), e, 1);
                p.emit(Op::QueueErase, voidType(), {Operand::typeRow(q.detail), ref, index(0)});
                model.erase(model.begin());
                p.emit(Op::QueueInsert, voidType(),
                       {Operand::typeRow(q.detail), ref, index(0), items[0]});
                model.insert(model.begin(), {-2, 0});
                trim();
                p.emit(Op::QueuePushBack, voidType(), {Operand::typeRow(q.detail), ref, items[1]});
                model.push_back({-3, 1});
                trim();
                record(p.emit(Op::QueuePopFront, e, {Operand::typeRow(q.detail), ref}), e,
                       model.front().second);
                model.erase(model.begin());
                const unsigned back = model.empty() ? 3 : model.back().second;
                record(p.emit(Op::QueuePopBack, e, {Operand::typeRow(q.detail), ref}), e, back);
                if (!model.empty()) model.pop_back();
                p.emit(Op::ContainerWrite, voidType(),
                       {Operand::typeRow(q.detail), ref, index(0), items[2]});
                const unsigned retainedCellValue =
                    !model.empty() && model.front().first == 0 ? 2 : 1;
                record(p.emit(Op::ContainerRead, e, {p.load(q, home), index(0)}), e, 2);
                p.emit(Op::ContainerClear, voidType(), {Operand::typeRow(q.detail), ref});
                size(p.load(q, home), 0);
                record(p.emit(Op::LoadRef, e, {cell}), e, retainedCellValue);
                p.emit(Op::ContainerAssign, voidType(), {Operand::typeRow(q.detail), ref, snap});
                record(p.emit(Op::ContainerRead, e, {p.load(q, home), index(0)}), e, 0);
                p.end();
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                for (const auto& [slot, expected] : checks) {
                    SCOPED_TRACE(slot.id);
                    if (expected >= 100) {
                        EXPECT_EQ(stored(ctx, slot, u2(64))[0].a, expected - 100);
                        continue;
                    }
                    const auto actual = stored(ctx, slot, e);
                    if (e.kind == TypeKind::String) {
                        EXPECT_EQ(ctx.strings().get(actual[0].a),
                                  expected == 3 ? "" : p.d.constants[items[expected].id].bytes);
                    } else {
                        for (unsigned w = 0; w < actual.size(); ++w) {
                            const auto& c = p.d.constants[items[expected == 3 ? 0 : expected].id];
                            const auto mask = isIntegral(e)
                                                  ? k::mask(std::min(64U, e.width - w * 64))
                                                  : ~std::uint64_t{0};
                            const auto aWord =
                                expected == 3 ? (isFourState(e) ? mask : 0) : c.aWords[w];
                            const auto bWord = expected == 3    ? (isFourState(e) ? mask : 0)
                                               : isFourState(e) ? c.bWords[w]
                                                                : 0;
                            EXPECT_EQ(actual[w].a, aWord);
                            EXPECT_EQ(actual[w].b, bWord);
                        }
                    }
                }
                ++cases;
            }
    EXPECT_EQ(cases, 216U);
}

TEST(VmContainer, RefCallsRetainActualBoundsForEmptyAndNestedVariables) {
    unsigned cases = 0;
    for (unsigned actualBound : {0U, 1U, 3U})
        for (unsigned formalBound : {0U, 1U, 3U})
            for (bool automatic : {false, true})
                for (bool empty : {false, true})
                    for (bool nested : {false, true}) {
                        SCOPED_TRACE(::testing::Message()
                                     << actualBound << '/' << formalBound << '/' << automatic << '/'
                                     << empty << '/' << nested);
                        Program p;
                        const Type a = p.container(s4(32), false);
                        const Type actual = p.container(s4(32), true, actualBound);
                        const Type formal = p.container(s4(32), true, formalBound);
                        const Type actualArg = nested ? p.container(actual, true) : actual;
                        const Type formalArg = nested ? p.container(formal, true) : formal;
                        const auto zero = p.constant(s4(64), 0);
                        auto input = empty ? p.constant(actual, 0)
                                           : p.emit(Op::ContainerPack, actual,
                                                    {p.constant(s4(32), 7), p.constant(s4(32), 11),
                                                     p.constant(s4(32), 13)});
                        if (nested) input = p.emit(Op::ContainerPack, actualArg, {input});
                        const auto home = automatic ? p.frame(actualArg) : p.slot(actualArg);
                        p.store(home, input);
                        const auto ref = p.emit(Op::RefMake, refType(actualArg.detail), {home});

                        Program fn;
                        fn.d.types = p.d.types;
                        fn.d.constants = p.d.constants;
                        const auto param = fn.frame(refType(formalArg.detail), formalArg);
                        auto target = fn.emit(Op::RefMake, refType(formalArg.detail), {param});
                        if (nested)
                            target = fn.emit(Op::ContainerCell, refType(formal.detail),
                                             {Operand::typeRow(formalArg.detail), target, zero});
                        const auto value = fn.constant(s4(32), 23);
                        fn.emit(Op::QueuePushBack, voidType(),
                                {Operand::typeRow(formal.detail), target, value});
                        const auto initialSize = fn.emit(Op::ContainerSize, u2(64),
                                                         {fn.emit(Op::LoadRef, formal, {target})});
                        const auto packed = fn.emit(Op::ContainerPack, a, {value, value, value});
                        fn.emit(Op::ContainerAssign, voidType(),
                                {Operand::typeRow(formal.detail), target, packed});
                        const auto snap = fn.emit(Op::ContainerSnapshot, formal,
                                                  {fn.emit(Op::LoadRef, formal, {target})});
                        fn.emit(Op::ContainerClear, voidType(),
                                {Operand::typeRow(formal.detail), target});
                        for (unsigned i = 0; i < 3; ++i)
                            fn.emit(Op::QueuePushBack, voidType(),
                                    {Operand::typeRow(formal.detail), target, value});
                        const auto finalSize = fn.emit(Op::ContainerSize, u2(64),
                                                       {fn.emit(Op::LoadRef, formal, {target})});
                        const auto snapSize = fn.emit(Op::ContainerSize, u2(64), {snap});
                        const auto sum =
                            fn.emit(Op::Add, u2(64),
                                    {initialSize, fn.emit(Op::Add, u2(64), {finalSize, snapSize})});
                        fn.emit(Op::Ret, voidType(), {sum});
                        Subroutine sub;
                        sub.name = "top.mutate";
                        sub.automatic = true;
                        sub.params = {{"q", Direction::Ref, formalArg, kNone, FrameVarId{0}}};
                        sub.returnType = u2(64);
                        sub.frame = fn.d.processes[0].frame;
                        sub.blocks = fn.d.processes[0].blocks;
                        sub.numTemps = fn.d.processes[0].numTemps;
                        sub.loc = {0, 1, 1};
                        p.d.constants = std::move(fn.d.constants);
                        p.d.subroutines.push_back(std::move(sub));
                        const auto answer = p.slot(u2(64));
                        p.store(answer, p.emit(Op::Call, u2(64),
                                               {Operand::subroutine(SubroutineId{0}), ref}));
                        auto after = p.load(actualArg, home);
                        if (nested) after = p.emit(Op::ContainerRead, actual, {after, zero});
                        const auto countSlot = p.slot(u2(64));
                        p.store(countSlot, p.emit(Op::ContainerSize, u2(64), {after}));
                        p.end();
                        ASSERT_TRUE(verifies(p.d));
                        Capture capture;
                        ASSERT_NE(capture.file, nullptr);
                        k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                        const auto result = kirimasim::vm::run(p.d, ctx);
                        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                        const unsigned count = actualBound == 0 ? 3 : actualBound;
                        const unsigned initial = empty ? 1 : actualBound == 0 ? 4 : actualBound;
                        EXPECT_EQ(stored(ctx, answer, u2(64))[0].a, initial + count * 2);
                        EXPECT_EQ(stored(ctx, countSlot, u2(64))[0].a, count);
                        ++cases;
                    }
    EXPECT_EQ(cases, 72U);
}

TEST(VmContainer, SelectedFixedArrayRefsPreserveActualMetadata) {
    for (bool automatic : {false, true})
        for (bool descending : {false, true}) {
            Program p;
            const Type q = p.container(s4(32), true, 1);
            const auto home = automatic ? p.frame(q) : p.slot(q);
            const std::vector<ArrayDim> dims{{descending ? 2 : 0, descending ? 0 : 2}};
            if (automatic)
                p.d.processes[0].frame[home.id].dims = dims;
            else
                p.d.slots[home.id].dims = dims;
            const auto whole = p.emit(Op::RefMake, refType(), {home});
            const auto forwarded = p.frame(refType(), q);
            p.d.processes[0].frame[forwarded.id].dims = dims;
            p.store(forwarded, whole);
            const auto at = p.constant(s4(64), 1);
            const auto selected = p.emit(Op::RefMake, refType(q.detail), {forwarded, at});
            for (unsigned n : {7U, 11U})
                p.emit(Op::QueuePushBack, voidType(),
                       {Operand::typeRow(q.detail), selected, p.constant(s4(32), n)});
            const auto queue = p.emit(Op::LoadElem, q, {home, at});
            const auto count = p.slot(u2(64));
            p.store(count, p.emit(Op::ContainerSize, u2(64), {queue}));
            const auto neighbor = p.emit(Op::LoadElem, q, {home, p.constant(s4(64), 0)});
            const auto neighborCount = p.slot(u2(64));
            p.store(neighborCount, p.emit(Op::ContainerSize, u2(64), {neighbor}));
            p.end();
            ASSERT_TRUE(verifies(p.d));
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            k::Context ctx(k::OutputDestinations{capture.file, capture.file});
            const auto result = kirimasim::vm::run(p.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            EXPECT_EQ(stored(ctx, count, u2(64))[0].a, 1U);
            EXPECT_EQ(stored(ctx, neighborCount, u2(64))[0].a, 0U);
        }
}

TEST(VmContainer, InvalidSizesAndOverflowPreserveTheDestination) {
    for (unsigned sizeKind = 0; sizeKind < 4; ++sizeKind) {
        SCOPED_TRACE(sizeKind);
        Program p;
        const Type a = p.container(s4(32), false);
        const auto home = p.slot(a);
        p.store(home, p.emit(Op::ContainerPack, a, {p.constant(s4(32), 7)}));
        const auto size = p.constant(s4(64),
                                     sizeKind == 0   ? ~std::uint64_t{0}
                                     : sizeKind == 3 ? std::numeric_limits<std::int64_t>::max()
                                                     : 0,
                                     sizeKind == 1   ? 1
                                     : sizeKind == 2 ? ~std::uint64_t{0}
                                                     : 0);
        const auto failed = p.emit(Op::ContainerNew, a, {size, p.constant(a, 0)});
        p.d.processes[0].blocks[0].instrs.back().loc = {0, 42, 9};
        p.store(home, failed);
        p.end();
        ASSERT_TRUE(verifies(p.d));
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context ctx(k::OutputDestinations{capture.file, capture.file});
        const auto result = kirimasim::vm::run(p.d, ctx);
        EXPECT_EQ(result.reason, sizeKind == 3 ? k::EndReason::ResourceLimit : k::EndReason::Error);
        const auto token = k::ContainerToken{stored(ctx, home, a)[0].a};
        const auto actual = ctx.heap().actualType(token);
        ASSERT_TRUE(actual);
        EXPECT_EQ(ctx.heap().size(actual.value, token).value, 1U);
        EXPECT_EQ(ctx.heap().stats().containers, 1U);
        EXPECT_EQ(ctx.heap().stats().cells, 1U);
        const auto text = capture.text();
        EXPECT_NE(text.find("container.sv:42: error:"), std::string::npos);
        EXPECT_NE(text.find(sizeKind == 3 ? "container resource limit"
                                          : "dynamic-array size is negative or contains X/Z bits"),
                  std::string::npos);
    }
}

TEST(VmContainer, MethodWarningsAreOncePerSourcePosition) {
    Program p;
    const auto q = p.container(s4(32), true, 1);
    const auto home = p.slot(q);
    const auto ref = p.emit(Op::RefMake, refType(q.detail), {home});
    const auto item = p.constant(s4(32), 7);
    const auto badIndex = p.constant(s4(64), ~std::uint64_t{0});
    const auto method = [&](Op op, Type result, std::vector<Operand> operands, unsigned line) {
        p.emit(op, result, std::move(operands));
        p.d.processes[0].blocks[0].instrs.back().loc = {0, line, 3};
    };
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        method(Op::QueuePopBack, s4(32), {Operand::typeRow(q.detail), ref}, 10);
        method(Op::QueueInsert, voidType(), {Operand::typeRow(q.detail), ref, badIndex, item}, 11);
    }
    for (unsigned repeat = 0; repeat < 3; ++repeat)
        method(Op::QueuePushBack, voidType(), {Operand::typeRow(q.detail), ref, item}, 12);
    p.end();
    ASSERT_TRUE(verifies(p.d));
    Capture capture;
    ASSERT_NE(capture.file, nullptr);
    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
    EXPECT_EQ(kirimasim::vm::run(p.d, ctx).reason, k::EndReason::Quiescent);
    const auto text = capture.text();
    EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 3);
    EXPECT_NE(text.find("container.sv:10:3: warning: pop from an empty queue"), std::string::npos);
    EXPECT_NE(text.find("container.sv:11:3: warning: queue method index is invalid"),
              std::string::npos);
    EXPECT_NE(text.find("container.sv:12:3: warning: queue operation exceeds its declared bound"),
              std::string::npos);
}

TEST(VmContainer, PostponedMutationsUseTheReferenceOrigin) {
    for (bool local : {false, true})
        for (bool nested : {false, true})
            for (bool cellWrite : {false, true}) {
                SCOPED_TRACE(::testing::Message() << local << '/' << nested << '/' << cellWrite);
                Program p;
                const auto q = p.container(s4(32), true);
                const auto outer = nested ? p.container(q, true) : q;
                const auto global = p.slot(outer);
                const auto item = p.constant(s4(32), 7);
                const auto index = p.constant(s4(64), 0);
                auto initial = p.emit(Op::ContainerPack, q, {item});
                if (nested) initial = p.emit(Op::ContainerPack, outer, {initial});
                p.store(global, initial);
                p.emit(Op::Strobe, voidType(), {Operand::process(ProcessId{1})});
                p.end();
                Program post;
                post.d.types = p.d.types;
                post.d.constants = p.d.constants;
                post.d.slots = p.d.slots;
                auto home = global;
                if (local) {
                    home = post.frame(outer);
                    auto value = post.emit(Op::ContainerPack, q, {item});
                    if (nested) value = post.emit(Op::ContainerPack, outer, {value});
                    post.store(home, value);
                }
                auto ref = post.emit(Op::RefMake, refType(outer.detail), {home});
                if (nested)
                    ref = post.emit(Op::ContainerCell, refType(q.detail),
                                    {Operand::typeRow(outer.detail), ref, index});
                if (cellWrite) {
                    const auto cell = post.emit(Op::ContainerCell, refType(),
                                                {Operand::typeRow(q.detail), ref, index});
                    post.emit(Op::StoreRef, voidType(), {cell, item});
                } else {
                    post.emit(Op::QueuePushBack, voidType(),
                              {Operand::typeRow(q.detail), ref, item});
                }
                const auto size =
                    post.emit(Op::ContainerSize, u2(64), {post.emit(Op::LoadRef, q, {ref})});
                post.emit(Op::Syscall, voidType(),
                          {Operand::str("$display"), Operand::str("%0d"), size});
                post.end();
                auto process = std::move(post.d.processes[0]);
                process.kind = ProcessKind::Postponed;
                process.name = "top.postponed";
                if (!local) process.writes.push_back({global.asSlot()});
                p.d.processes.push_back(std::move(process));
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                EXPECT_EQ(result.reason, local ? k::EndReason::Quiescent : k::EndReason::Error)
                    << result.error;
                const auto text = capture.text();
                if (local)
                    EXPECT_EQ(text, cellWrite ? "1\n" : "2\n");
                else
                    EXPECT_NE(text.find("a write or an event in the Postponed region"),
                              std::string::npos);
            }
}

class VmHeapMemory final : public std::pmr::memory_resource {
public:
    std::optional<std::size_t> remaining;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining) {
            if (*remaining == 0) throw std::bad_alloc{};
            --*remaining;
        }
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
struct ArmHeap {
    VmHeapMemory& memory;
    std::size_t point;
    k::ProcessId process{0};
    bool waited = false;
    k::SlotId snapshotSlot = k::kNone;
    k::ContainerToken original;
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& a = *static_cast<ArmHeap*>(frame);
        if (!a.waited) {
            a.waited = true;
            k::fromC(context).scheduler().waitDelay(a.process, 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        if (a.snapshotSlot != k::kNone)
            a.original = {*k::fromC(context).store().element(a.snapshotSlot, 0)};
        a.memory.remaining = a.point;
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(VmContainer, AllocationFailuresDuringAssignmentReturnResourceLimitWithoutReplacement) {
    bool completed = false;
    unsigned assignmentFailures = 0, collectionFailures = 0;
    for (std::size_t point = 0; point < 100; ++point) {
        SCOPED_TRACE(point);
        Program p;
        const auto q = p.container(s4(32), true);
        const auto nested = p.container(q, true);
        const auto home = p.slot(nested);
        const auto item = p.constant(s4(32), 7);
        const auto child = p.emit(Op::ContainerPack, q, {item, item, item});
        p.store(home, p.emit(Op::ContainerPack, nested, {child}));
        p.emit(Op::WaitDelay, voidType(), {p.constant(u2(64), 2), Operand::block(BlockId{1})});
        auto& process = p.d.processes[0];
        process.blocks.resize(2);
        const auto ref = TempId{process.numTemps++};
        const auto source = TempId{process.numTemps++};
        process.blocks[1].instrs = {
            {Op::RefMake, refType(nested.detail), ref, {home}, {0, 42, 9}},
            {Op::Load, nested, source, {home}, {0, 42, 9}},
            {Op::ContainerAssign,
             voidType(),
             kNone,
             {Operand::typeRow(nested.detail), Operand::temp(ref), Operand::temp(source)},
             {0, 42, 9}},
            {Op::Ret, voidType(), kNone, {}, {0, 42, 9}}};
        ASSERT_TRUE(verifies(p.d));
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        VmHeapMemory memory;
        k::Context ctx(k::OutputDestinations{capture.file, capture.file}, nullptr, &memory);
        ArmHeap arm{memory, point};
        arm.snapshotSlot = k::SlotId{home.id};
        arm.process = ctx.scheduler().addProcess(&ArmHeap::resume, &arm, k::Group::Active);
        ctx.scheduler().start(arm.process);
        const auto result = kirimasim::vm::run(p.d, ctx);
        memory.remaining.reset();
        const auto token = k::ContainerToken{stored(ctx, home, nested)[0].a};
        const auto type = ctx.heap().actualType(token);
        ASSERT_TRUE(type);
        EXPECT_EQ(ctx.heap().size(type.value, token).value, 1U);
        if (result.reason == k::EndReason::Quiescent) {
            completed = true;
            EXPECT_GT(point, 5U);
            EXPECT_NE(token, arm.original);
            EXPECT_EQ(ctx.heap().stats().containers, 2U);
            EXPECT_EQ(ctx.heap().stats().cells, 4U);
            break;
        }
        EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
        const auto message = capture.text();
        const bool collection =
            message.find("container collection resource limit") != std::string::npos;
        if (collection) {
            ++collectionFailures;
            EXPECT_NE(message.find("container.sv:42: error: container collection resource limit"),
                      std::string::npos);
            // Assignment committed before the safe point. Failed marking
            // leaves BOTH complete values alive, with distinct cells.
            EXPECT_NE(token, arm.original);
            EXPECT_TRUE(ctx.heap().actualType(arm.original));
            EXPECT_EQ(ctx.heap().stats().containers, 4U);
            EXPECT_EQ(ctx.heap().stats().cells, 8U);
            ASSERT_TRUE(
                ctx.heap().collect(std::array<k::HeapRoot, 1>{{k::HeapRoot::container(token)}}));
            EXPECT_EQ(ctx.heap().stats().containers, 2U);
            EXPECT_EQ(ctx.heap().stats().cells, 4U);
            EXPECT_EQ(ctx.heap().actualType(arm.original).status.error,
                      k::HeapError::InvalidHandle);
        } else {
            ++assignmentFailures;
            EXPECT_NE(message.find("container.sv:42: error: container resource limit"),
                      std::string::npos);
            EXPECT_EQ(token, arm.original);
            // The initial unowned temporary was collected at time 0.
            // A failed replacement preserves exactly the reachable value.
            EXPECT_EQ(ctx.heap().stats().containers, 2U);
            EXPECT_EQ(ctx.heap().stats().cells, 4U);
        }
    }
    EXPECT_TRUE(completed);
    EXPECT_GT(assignmentFailures, 0U);
    EXPECT_GT(collectionFailures, 0U);
}

TEST(VmContainer, WaitingTaskKeepsDetachedElementReference) {
    for (const Type element : {s2(32), s4(32), s2(129), s4(129)}) {
        SCOPED_TRACE(toString(element));
        Program p;
        const auto q = p.container(element, true);
        const auto home = p.slot(q);
        const auto oldValue = p.slot(element), currentValue = p.slot(element);
        const auto index = p.constant(s4(64), 0);
        const auto first = p.constant(element, 7), replacement = p.constant(element, 11),
                   changed = p.constant(element, 13);
        p.store(home, p.emit(Op::ContainerPack, q, {first}));
        const auto receiver = p.emit(Op::RefMake, refType(q.detail), {home});
        const auto cell =
            p.emit(Op::ContainerCell, refType(), {Operand::typeRow(q.detail), receiver, index});
        const auto saved = p.frame(refType(), element);
        p.store(saved, cell);
        p.emit(Op::CallTask, voidType(),
               {Operand::subroutine(SubroutineId{0}), cell, Operand::block(BlockId{1})});
        auto& main = p.d.processes[0];
        main.blocks.resize(2);
        const auto ref = TempId{main.numTemps++}, old = TempId{main.numTemps++},
                   queue = TempId{main.numTemps++}, now = TempId{main.numTemps++};
        main.blocks[1].instrs = {
            {Op::Fload, refType(), ref, {saved}, {0, 1, 1}},
            {Op::LoadRef, element, old, {Operand::temp(ref)}, {0, 1, 1}},
            {Op::Store, voidType(), kNone, {oldValue, Operand::temp(old)}, {0, 1, 1}},
            {Op::Load, q, queue, {home}, {0, 1, 1}},
            {Op::ContainerRead, element, now, {Operand::temp(queue), index}, {0, 1, 1}},
            {Op::Store, voidType(), kNone, {currentValue, Operand::temp(now)}, {0, 1, 1}},
            {Op::Ret, voidType(), kNone, {}, {0, 1, 1}}};
        Subroutine task;
        task.name = "top.waiting";
        task.kind = SubroutineKind::Task;
        task.automatic = true;
        task.mayWait = true;
        task.params = {{"item", Direction::Ref, element, kNone, FrameVarId{0}}};
        FrameVar formal;
        formal.type = refType();
        formal.target = element;
        formal.name = "item";
        task.frame = {formal};
        task.blocks.resize(2);
        task.blocks[0].instrs = {{Op::WaitDelay,
                                  voidType(),
                                  kNone,
                                  {p.constant(u2(64), 2), Operand::block(BlockId{1})},
                                  {0, 1, 1}}};
        task.blocks[1].instrs = {
            {Op::StoreRef, voidType(), kNone, {Operand::frame(FrameVarId{0}), changed}, {0, 1, 1}},
            {Op::Ret, voidType(), kNone, {}, {0, 1, 1}}};
        p.d.subroutines.push_back(std::move(task));
        Process replace;
        replace.name = "top.replace";
        replace.writes = {{home.asSlot()}};
        replace.numTemps = 2;
        replace.blocks.resize(2);
        replace.blocks[0].instrs = {{Op::WaitDelay,
                                     voidType(),
                                     kNone,
                                     {p.constant(u2(64), 1), Operand::block(BlockId{1})},
                                     {0, 1, 1}}};
        replace.blocks[1].instrs = {
            {Op::RefMake, refType(q.detail), TempId{0}, {home}, {0, 1, 1}},
            {Op::ContainerClear,
             voidType(),
             kNone,
             {Operand::typeRow(q.detail), Operand::temp(TempId{0})},
             {0, 1, 1}},
            {Op::ContainerPack, q, TempId{1}, {replacement}, {0, 1, 1}},
            {Op::ContainerAssign,
             voidType(),
             kNone,
             {Operand::typeRow(q.detail), Operand::temp(TempId{0}), Operand::temp(TempId{1})},
             {0, 1, 1}},
            {Op::Ret, voidType(), kNone, {}, {0, 1, 1}}};
        p.d.processes.push_back(std::move(replace));
        ASSERT_TRUE(verifies(p.d));
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context ctx(k::OutputDestinations{capture.file, capture.file});
        const auto result = kirimasim::vm::run(p.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        const auto oldWords = stored(ctx, oldValue, element),
                   currentWords = stored(ctx, currentValue, element);
        for (unsigned w = 0; w < oldWords.size(); ++w) {
            EXPECT_EQ(oldWords[w].a, p.d.constants[changed.id].aWords[w]);
            EXPECT_EQ(currentWords[w].a, p.d.constants[replacement.id].aWords[w]);
        }
        EXPECT_EQ(ctx.heap().stats().containers, 1U);
        EXPECT_EQ(ctx.heap().stats().cells, 1U);
    }
}

TEST(VmContainer, CapturedCellRefsKeepTheSelectedElementAfterMovementAndDeletion) {
    for (Type e : {s2(32), s4(32), s2(65), s4(65), s2(129), s4(129)})
        for (bool automatic : {false, true})
            for (unsigned selected = 0; selected < 3; ++selected) {
                SCOPED_TRACE(::testing::Message()
                             << toString(e) << '/' << automatic << '/' << selected);
                Program p;
                const auto q = p.container(e, true);
                const auto home = automatic ? p.frame(q) : p.slot(q);
                std::array<Operand, 3> original;
                for (unsigned i = 0; i < original.size(); ++i)
                    original[i] = p.constant(e, i + 1);
                p.store(home,
                        p.emit(Op::ContainerPack, q, {original[0], original[1], original[2]}));
                const auto receiver = p.emit(Op::RefMake, refType(q.detail), {home});
                const auto cell =
                    p.emit(Op::ContainerCell, refType(),
                           {Operand::typeRow(q.detail), receiver, p.constant(s4(64), selected)});
                p.emit(Op::QueuePushFront, voidType(),
                       {Operand::typeRow(q.detail), receiver, p.constant(e, 4)});
                p.emit(Op::QueueErase, voidType(),
                       {Operand::typeRow(q.detail), receiver, p.constant(s4(64), selected + 1)});
                p.emit(Op::ContainerWrite, voidType(),
                       {Operand::typeRow(q.detail), receiver, p.constant(s4(64), 0),
                        p.constant(e, 5)});
                const auto changed = p.constant(e, 0x77, 0x22);
                p.emit(Op::StoreRef, voidType(), {cell, changed});
                const auto detached = p.slot(e);
                p.store(detached, p.emit(Op::LoadRef, e, {cell}));
                std::vector<Operand> outputs;
                const auto current = p.load(q, home);
                for (unsigned i = 0; i < 3; ++i) {
                    outputs.push_back(p.slot(e));
                    p.store(outputs.back(),
                            p.emit(Op::ContainerRead, e, {current, p.constant(s4(64), i)}));
                }
                p.end();
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                std::vector<unsigned> expected{5};
                for (unsigned i = 0; i < 3; ++i)
                    if (i != selected) expected.push_back(i + 1);
                EXPECT_EQ(stored(ctx, detached, e)[0].a, 0x77U);
                EXPECT_EQ(stored(ctx, detached, e)[0].b, isFourState(e) ? 0x22U : 0U);
                for (unsigned i = 0; i < outputs.size(); ++i)
                    EXPECT_EQ(stored(ctx, outputs[i], e)[0].a, expected[i]);
            }
}

TEST(VmContainer, PackedWritesThroughCellRefsMatchTheValueReference) {
    namespace vs = kirimasim::valsem;
    for (unsigned width : {1U, 32U, 65U, 129U})
        for (Type element : {s2(width), s4(width)})
            for (unsigned partWidth : {1U, 16U, 129U})
                for (unsigned position = 0; position < 4; ++position) {
                    SCOPED_TRACE(::testing::Message()
                                 << toString(element) << '/' << partWidth << '/' << position);
                    Program p;
                    const auto q = p.container(element, true);
                    const auto home = p.slot(q);
                    const auto initial = p.constant(element, 0xa5, 0x22);
                    const auto data = p.constant(s4(partWidth), 0x5a, 0x11);
                    const std::uint64_t pos = position == 0   ? 0
                                              : position == 1 ? width - 1
                                                              : ~std::uint64_t{0};
                    const auto index = p.constant(s4(64), pos, position == 3 ? 1 : 0);
                    p.store(home, p.emit(Op::ContainerPack, q, {initial}));
                    const auto receiver = p.emit(Op::RefMake, refType(q.detail), {home});
                    const auto cell =
                        p.emit(Op::ContainerCell, refType(),
                               {Operand::typeRow(q.detail), receiver, p.constant(s4(64), 0)});
                    const auto formal = p.frame(refType(), element);
                    p.store(formal, cell);
                    p.emit(Op::StoreRefBits, voidType(), {formal, index, data});
                    const auto answer = p.slot(element);
                    p.store(answer, p.emit(Op::LoadRef, element, {cell}));
                    p.end();
                    ASSERT_TRUE(verifies(p.d));
                    const auto& init = p.d.constants[initial.id];
                    const auto& bits = p.d.constants[data.id];
                    const auto& selector = p.d.constants[index.id];
                    const auto x = vs::fromWords(init.aWords, init.bWords, width, true);
                    const auto y = vs::fromWords(bits.aWords, bits.bWords, partWidth, true);
                    const auto at = vs::fromWords(selector.aWords, selector.bWords, 64, true);
                    auto expected = vs::writeBits(x, at, y);
                    if (!isFourState(element)) expected = vs::to2(expected);
                    std::vector<std::uint64_t> a, b;
                    vs::toWords(expected, a, b);
                    Capture capture;
                    ASSERT_NE(capture.file, nullptr);
                    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                    const auto result = kirimasim::vm::run(p.d, ctx);
                    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                    const auto actual = stored(ctx, answer, element);
                    for (unsigned w = 0; w < actual.size(); ++w) {
                        EXPECT_EQ(actual[w].a, a[w]);
                        EXPECT_EQ(actual[w].b, isFourState(element) ? b[w] : 0U);
                    }
                }
}

TEST(VmContainer, CapturedReceiverUsesCurrentValueAfterArgumentSideEffects) {
    for (bool automatic : {false, true})
        for (unsigned selected : {0U, 1U})
            for (unsigned bound : {0U, 1U, 3U}) {
                SCOPED_TRACE(::testing::Message() << automatic << '/' << selected << '/' << bound);
                Program p;
                const auto q = p.container(s4(32), true, bound);
                const auto home = automatic ? p.frame(q) : p.slot(q);
                if (automatic)
                    p.d.processes[0].frame[home.id].dims = {{0, 1}};
                else
                    p.d.slots[home.id].dims = {{0, 1}};
                const auto indexHome = p.slot(s4(64));
                p.store(indexHome, p.constant(s4(64), selected));
                const auto ref =
                    p.emit(Op::RefMake, refType(q.detail), {home, p.load(s4(64), indexHome)});
                // Arguments change both the selector and the bound
                // variable's value after its binding was captured.
                p.store(indexHome, p.constant(s4(64), 1 - selected));
                const auto fresh =
                    p.emit(Op::ContainerPack, q, {p.constant(s4(32), 11), p.constant(s4(32), 13)});
                const auto same =
                    p.emit(Op::RefMake, refType(q.detail), {home, p.constant(s4(64), selected)});
                p.emit(Op::ContainerAssign, voidType(), {Operand::typeRow(q.detail), same, fresh});
                p.emit(Op::QueuePushBack, voidType(),
                       {Operand::typeRow(q.detail), ref, p.constant(s4(32), 17)});
                const auto count = p.slot(u2(64)), neighborCount = p.slot(u2(64));
                const auto current = p.emit(Op::LoadElem, q, {home, p.constant(s4(64), selected)});
                const auto neighbor = p.emit(Op::LoadElem, q, {home, p.load(s4(64), indexHome)});
                p.store(count, p.emit(Op::ContainerSize, u2(64), {current}));
                p.store(neighborCount, p.emit(Op::ContainerSize, u2(64), {neighbor}));
                const auto first = p.slot(s4(32));
                p.store(first, p.emit(Op::ContainerRead, s4(32), {current, p.constant(s4(64), 0)}));
                p.end();
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                EXPECT_EQ(stored(ctx, count, u2(64))[0].a, bound == 1 ? 1U : 3U);
                EXPECT_EQ(stored(ctx, neighborCount, u2(64))[0].a, 0U);
                EXPECT_EQ(stored(ctx, first, s4(32))[0].a, 11U);
            }
}

TEST(VmContainer, InvalidIndicesReadDefaultsAndLeaveStorageUnchanged) {
    for (Type e : {s2(1), s4(1), s2(65), s4(65)})
        for (bool empty : {false, true})
            for (unsigned invalid = 0; invalid < 4; ++invalid) {
                SCOPED_TRACE(::testing::Message() << toString(e) << '/' << empty << '/' << invalid);
                Program p;
                const auto q = p.container(e, true);
                const auto home = p.slot(q);
                const auto item = p.constant(e, 7);
                p.store(home, empty ? p.constant(q, 0) : p.emit(Op::ContainerPack, q, {item}));
                const auto ref = p.emit(Op::RefMake, refType(q.detail), {home});
                Operand index;
                if (invalid == 1) {
                    Constant wide;
                    wide.type = s4(129);
                    wide.aWords = {0, 1, 0};
                    wide.bWords = {0, 0, 0};
                    index = p.constant(std::move(wide));
                } else {
                    index = p.constant(s4(64),
                                       invalid == 0   ? ~std::uint64_t{0}
                                       : invalid == 2 ? 1
                                                      : 0,
                                       invalid >= 2 ? 1 : 0);
                }
                const auto cell =
                    p.emit(Op::ContainerCell, refType(), {Operand::typeRow(q.detail), ref, index});
                p.emit(Op::StoreRef, voidType(), {cell, p.constant(e, 11)});
                p.emit(Op::ContainerWrite, voidType(),
                       {Operand::typeRow(q.detail), ref, index, p.constant(e, 13)});
                const auto direct = p.slot(e), indirect = p.slot(e), count = p.slot(u2(64));
                p.store(direct, p.emit(Op::ContainerRead, e, {p.load(q, home), index}));
                p.store(indirect, p.emit(Op::LoadRef, e, {cell}));
                p.store(count, p.emit(Op::ContainerSize, u2(64), {p.load(q, home)}));
                p.end();
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                EXPECT_EQ(stored(ctx, count, u2(64))[0].a, empty ? 0U : 1U);
                for (const auto& output : {direct, indirect}) {
                    const auto words = stored(ctx, output, e);
                    for (unsigned w = 0; w < words.size(); ++w) {
                        const auto mask = k::mask(std::min(64U, e.width - w * 64));
                        EXPECT_EQ(words[w].a, isFourState(e) ? mask : 0U);
                        EXPECT_EQ(words[w].b, isFourState(e) ? mask : 0U);
                    }
                }
                EXPECT_TRUE(capture.text().empty());
            }
}

TEST(VmContainer, HeapWatchesWakeForSizeAndStableCellChanges) {
    for (bool cellRef : {false, true})
        for (bool event : {false, true})
            for (bool throughCall : {false, true}) {
                SCOPED_TRACE(::testing::Message() << cellRef << '/' << event << '/' << throughCall);
                Program p;
                const auto q = p.container(s4(32), true);
                const auto home = p.slot(q), done = p.slot(u2(1));
                const auto zero = p.constant(s4(64), 0);
                const auto one = p.constant(s4(32), 1);
                const auto saved = p.frame(refType(), s4(32));
                p.store(home, p.emit(Op::ContainerPack, q, {p.constant(s4(32), 0)}));
                if (cellRef) {
                    const auto ref = p.emit(Op::RefMake, refType(q.detail), {home});
                    p.store(saved, p.emit(Op::ContainerCell, refType(),
                                          {Operand::typeRow(q.detail), ref, zero}));
                }
                p.emit(event ? Op::WaitEvent : Op::WaitCond, voidType(),
                       {Operand::subroutine(SubroutineId{throughCall ? 1U : 0U}),
                        Operand::block(BlockId{1})});
                p.d.processes[0].blocks.resize(2);
                p.emitAt(1, Op::Store, voidType(), {done, p.constant(u2(1), 1)});
                p.emitAt(1, Op::Ret, voidType(), {});
                Subroutine guard;
                guard.name = "top.watch";
                guard.kind = event ? SubroutineKind::Watch : SubroutineKind::Guard;
                guard.returnType = u2(1);
                guard.frameOf = {Owner::Kind::Process, 0};
                guard.loc = {0, 43, 5};
                guard.blocks.resize(1);
                auto& code = guard.blocks[0].instrs;
                if (cellRef) {
                    code = {
                        {Op::LoadRef, s4(32), TempId{0}, {saved}, {0, 43, 5}},
                        {Op::Ceq, u2(1), TempId{1}, {Operand::temp(TempId{0}), one}, {0, 43, 5}},
                        {Op::Ret, voidType(), kNone, {Operand::temp(TempId{1})}, {0, 43, 5}}};
                    guard.numTemps = 2;
                } else {
                    code = {{Op::Load, q, TempId{0}, {home}, {0, 43, 5}},
                            {Op::ContainerSize,
                             u2(64),
                             TempId{1},
                             {Operand::temp(TempId{0})},
                             {0, 43, 5}},
                            {Op::Ceq,
                             u2(1),
                             TempId{2},
                             {Operand::temp(TempId{1}), p.constant(u2(64), 2)},
                             {0, 43, 5}},
                            {Op::Ret, voidType(), kNone, {Operand::temp(TempId{2})}, {0, 43, 5}}};
                    guard.numTemps = 3;
                }
                if (throughCall) {
                    const auto target = cellRef ? s4(32) : q;
                    const auto refTypeActual = refType(cellRef ? TypeId{kNone} : q.detail);
                    Subroutine fn;
                    fn.name = "top.read_ref";
                    fn.automatic = true;
                    fn.returnType = cellRef ? s4(32) : u2(64);
                    FrameVar param;
                    param.name = "value";
                    param.type = refTypeActual;
                    param.target = target;
                    fn.frame.push_back(param);
                    fn.params = {{"value", Direction::Ref, target, kNone, FrameVarId{0}}};
                    fn.blocks.resize(1);
                    fn.blocks[0].instrs = {{Op::LoadRef,
                                            target,
                                            TempId{0},
                                            {Operand::frame(FrameVarId{0})},
                                            {0, 42, 1}}};
                    if (!cellRef)
                        fn.blocks[0].instrs.push_back({Op::ContainerSize,
                                                       u2(64),
                                                       TempId{1},
                                                       {Operand::temp(TempId{0})},
                                                       {0, 42, 1}});
                    fn.blocks[0].instrs.push_back({Op::Ret,
                                                   voidType(),
                                                   kNone,
                                                   {Operand::temp(TempId{cellRef ? 0U : 1U})},
                                                   {0, 42, 1}});
                    fn.numTemps = cellRef ? 1 : 2;
                    p.d.subroutines.push_back(std::move(fn));
                    code = {{Op::RefMake,
                             refTypeActual,
                             TempId{0},
                             {cellRef ? saved : home},
                             {0, 43, 5}},
                            {Op::Call,
                             cellRef ? s4(32) : u2(64),
                             TempId{1},
                             {Operand::subroutine(SubroutineId{0}), Operand::temp(TempId{0})},
                             {0, 43, 5}},
                            {Op::Ceq,
                             u2(1),
                             TempId{2},
                             {Operand::temp(TempId{1}), cellRef ? one : p.constant(u2(64), 2)},
                             {0, 43, 5}},
                            {Op::Ret, voidType(), kNone, {Operand::temp(TempId{2})}, {0, 43, 5}}};
                    guard.numTemps = 3;
                }
                p.d.subroutines.push_back(std::move(guard));
                Process writer;
                writer.name = "top.writer";
                writer.loc = {0, 45, 1};
                writer.blocks.resize(2);
                writer.numTemps = 1;
                writer.blocks[0].instrs = {{Op::WaitDelay,
                                            voidType(),
                                            kNone,
                                            {p.constant(u2(64), 1), Operand::block(BlockId{1})},
                                            {0, 45, 1}}};
                auto& codeWriter = writer.blocks[1].instrs;
                codeWriter.push_back(
                    {Op::RefMake, refType(q.detail), TempId{0}, {home}, {0, 46, 1}});
                std::vector<Operand> mutation{Operand::typeRow(q.detail), Operand::temp(TempId{0})};
                if (cellRef) mutation.push_back(zero);
                mutation.push_back(one);
                codeWriter.push_back({cellRef ? Op::ContainerWrite : Op::QueuePushBack,
                                      voidType(),
                                      kNone,
                                      mutation,
                                      {0, 47, 1}});
                codeWriter.push_back({Op::Ret, voidType(), kNone, {}, {0, 48, 1}});
                writer.writes = {{home.asSlot()}};
                p.d.processes.push_back(std::move(writer));
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error << capture.text();
                EXPECT_EQ(stored(ctx, done, u2(1))[0].a, 1U);
                EXPECT_EQ(ctx.scheduler().time(), 1U);
                EXPECT_TRUE(capture.text().empty());
            }
}

// Ref values can be loaded with fload, without ref.make in the watch body
// (SIR_SPEC 7.4, 8). Only the callee dereferences the retained heap cell;
// collectReads must enable heap capture even with no own RefRead entry.
TEST(VmContainer, HeapWatchesCaptureCalleeReadsOfRetainedRefValues) {
    for (bool event : {false, true})
        for (Type element : {s4(32), s4(129)}) {
            SCOPED_TRACE(::testing::Message() << event << '/' << element.width);
            Program p;
            const auto q = p.container(element, true);
            const auto home = p.slot(q), done = p.slot(u2(1));
            const auto saved = p.frame(refType(), element);
            const auto zero = p.constant(s4(64), 0);
            const auto one = p.constant(element, 1);
            p.store(home, p.emit(Op::ContainerPack, q, {p.constant(element, 0)}));
            const auto receiver = p.emit(Op::RefMake, refType(q.detail), {home});
            p.store(saved, p.emit(Op::ContainerCell, refType(),
                                  {Operand::typeRow(q.detail), receiver, zero}));
            p.emit(event ? Op::WaitEvent : Op::WaitCond, voidType(),
                   {Operand::subroutine(SubroutineId{1}), Operand::block(BlockId{1})});
            p.emitAt(1, Op::Store, voidType(), {done, p.constant(u2(1), 1)});
            p.emitAt(1, Op::Ret, voidType(), {});

            Subroutine getter;
            getter.name = "top.read_retained_ref";
            getter.automatic = true;
            getter.returnType = element;
            FrameVar param;
            param.name = "value";
            param.type = refType();
            param.target = element;
            getter.frame.push_back(param);
            getter.params = {{"value", Direction::ConstRef, element, kNone, FrameVarId{0}}};
            getter.numTemps = 1;
            getter.blocks.resize(1);
            getter.blocks[0].instrs = {
                {Op::LoadRef, element, TempId{0}, {Operand::frame(FrameVarId{0})}, {0, 42, 1}},
                {Op::Ret, voidType(), kNone, {Operand::temp(TempId{0})}, {0, 42, 1}}};
            p.d.subroutines.push_back(std::move(getter));

            Subroutine guard;
            guard.name = "top.watch";
            guard.kind = event ? SubroutineKind::Watch : SubroutineKind::Guard;
            guard.returnType = u2(1);
            guard.frameOf = {Owner::Kind::Process, 0};
            guard.loc = {0, 43, 5};
            guard.blocks.resize(1);
            auto& code = guard.blocks[0].instrs;
            const TempId loaded{guard.numTemps++};
            code.push_back({Op::Fload, refType(), loaded, {saved}, {0, 43, 5}});
            const TempId result{guard.numTemps++}, matches{guard.numTemps++};
            code.push_back({Op::Call,
                            element,
                            result,
                            {Operand::subroutine(SubroutineId{0}), Operand::temp(loaded)},
                            {0, 43, 5}});
            code.push_back({Op::Ceq, u2(1), matches, {Operand::temp(result), one}, {0, 43, 5}});
            code.push_back({Op::Ret, voidType(), kNone, {Operand::temp(matches)}, {0, 43, 5}});
            p.d.subroutines.push_back(std::move(guard));

            Process writer;
            writer.name = "top.writer";
            writer.loc = {0, 45, 1};
            writer.numTemps = 1;
            writer.blocks.resize(2);
            writer.blocks[0].instrs = {{Op::WaitDelay,
                                        voidType(),
                                        kNone,
                                        {p.constant(u2(64), 1), Operand::block(BlockId{1})},
                                        {0, 45, 1}}};
            writer.blocks[1].instrs = {
                {Op::RefMake, refType(q.detail), TempId{0}, {home}, {0, 46, 1}},
                {Op::ContainerWrite,
                 voidType(),
                 kNone,
                 {Operand::typeRow(q.detail), Operand::temp(TempId{0}), zero, one},
                 {0, 47, 1}},
                {Op::Ret, voidType(), kNone, {}, {0, 48, 1}}};
            writer.writes = {{home.asSlot()}};
            p.d.processes.push_back(std::move(writer));

            ASSERT_TRUE(verifies(p.d));
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            k::Context ctx(k::OutputDestinations{capture.file, capture.file});
            const auto run = kirimasim::vm::run(p.d, ctx);
            ASSERT_EQ(run.reason, k::EndReason::Quiescent) << run.error << capture.text();
            EXPECT_EQ(stored(ctx, done, u2(1))[0].a, 1U);
            EXPECT_EQ(ctx.scheduler().time(), 1U);
            EXPECT_TRUE(capture.text().empty());
        }
}

TEST(VmContainer, IllegalCellNbasReportTheStandardRuleWithoutModifyingTheValue) {
    for (Type e : {s4(32), s4(129)}) {
        Program p;
        const auto q = p.container(e, true);
        const auto home = p.slot(q);
        const auto initial = p.constant(e, 7);
        p.store(home, p.emit(Op::ContainerPack, q, {initial}));
        const auto ref = p.emit(Op::RefMake, refType(q.detail), {home});
        const auto cell = p.emit(Op::ContainerCell, refType(),
                                 {Operand::typeRow(q.detail), ref, p.constant(s4(64), 0)});
        p.emit(Op::NbaRef, voidType(), {cell, p.constant(e, 11)});
        p.d.processes[0].blocks[0].instrs.back().loc = {0, 43, 5};
        p.end();
        ASSERT_TRUE(verifies(p.d));
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context ctx(k::OutputDestinations{capture.file, capture.file});
        EXPECT_EQ(kirimasim::vm::run(p.d, ctx).reason, k::EndReason::Error);
        EXPECT_NE(capture.text().find("container.sv:43: error: nonblocking assignment to a "
                                      "dynamically sized array element is illegal "
                                      "(IEEE 1800-2023 10.4.2)"),
                  std::string::npos);
        const k::ContainerToken token{stored(ctx, home, q)[0].a};
        const auto type = ctx.heap().actualType(token);
        ASSERT_TRUE(type);
        std::vector<k::W4> out((e.width + 63) / 64);
        ASSERT_TRUE(ctx.heap().read(type.value, token, 0, out));
        for (unsigned w = 0; w < out.size(); ++w)
            EXPECT_EQ(out[w].a, p.d.constants[initial.id].aWords[w]);
    }
}

TEST(VmContainer, RegionCollectionKeepsTypedFramesCellsAndBoundedPools) {
    for (Type e : {s2(1), s4(32), s4(129)})
        for (bool automatic : {false, true})
            for (bool nested : {false, true})
                for (bool churn : {false, true}) {
                    SCOPED_TRACE(::testing::Message() << toString(e) << '/' << automatic << '/'
                                                      << nested << '/' << churn);
                    Program p;
                    const auto inner = p.container(e, true);
                    const auto q = nested ? p.container(inner, true) : inner;
                    const Type element = nested ? inner : e;
                    const auto home = automatic ? p.frame(q) : p.slot(q);
                    const auto saved =
                        p.frame(refType(nested ? inner.detail : TypeId{kNone}), element);
                    const auto counter = p.frame(u2(64));
                    const auto zero = p.constant(s4(64), 0), tick = p.constant(u2(64), 0),
                               one = p.constant(u2(64), 1), hundred = p.constant(u2(64), 100);
                    const auto item = p.constant(e, 7), changed = p.constant(e, 11);
                    const auto load = [&](std::size_t block, Type t, const Operand& h) {
                        return p.emitAt(
                            block, h.kind == Operand::Kind::Frame ? Op::Fload : Op::Load, t, {h});
                    };
                    const auto store = [&](std::size_t block, const Operand& h, const Operand& v) {
                        p.emitAt(block, h.kind == Operand::Kind::Frame ? Op::Fstore : Op::Store,
                                 voidType(), {h, v});
                    };
                    const auto make = [&](std::size_t block, const Operand& value) {
                        const auto child =
                            nested ? p.emitAt(block, Op::ContainerPack, inner, {value}) : value;
                        return p.emitAt(block, Op::ContainerPack, q, {child});
                    };
                    store(0, home, make(0, item));
                    const auto firstRef = p.emit(Op::RefMake, refType(q.detail), {home});
                    p.store(saved, p.emit(Op::ContainerCell,
                                          refType(nested ? inner.detail : TypeId{kNone}),
                                          {Operand::typeRow(q.detail), firstRef, zero}));
                    p.emit(Op::Br, voidType(), {Operand::block(BlockId{1})});
                    if (churn) {
                        const auto ref = p.emitAt(1, Op::RefMake, refType(q.detail), {home});
                        const auto extra =
                            nested ? p.emitAt(1, Op::ContainerPack, inner, {item}) : item;
                        p.emitAt(1, Op::QueuePushBack, voidType(),
                                 {Operand::typeRow(q.detail), ref, extra});
                        p.emitAt(1, Op::QueuePopBack, element, {Operand::typeRow(q.detail), ref});
                    } else
                        store(1, home, make(1, item));
                    store(1, counter,
                          p.emitAt(1, Op::Add, u2(64), {load(1, u2(64), counter), one}));
                    p.emitAt(1, Op::WaitDelay, voidType(), {tick, Operand::block(BlockId{2})});
                    const auto again =
                        p.emitAt(2, Op::Lt, u2(1), {load(2, u2(64), counter), hundred});
                    p.emitAt(2, Op::BrIf, voidType(),
                             {again, Operand::block(BlockId{1}), Operand::block(BlockId{3})});
                    const auto oldRef =
                        load(3, refType(nested ? inner.detail : TypeId{kNone}), saved);
                    const auto before = p.emitAt(3, Op::LoadRef, element, {oldRef});
                    const auto oldLeaf =
                        nested ? p.emitAt(3, Op::ContainerRead, e, {before, zero}) : before;
                    const auto oldResult = p.slot(e);
                    store(3, oldResult, oldLeaf);
                    if (nested) {
                        const auto newChild = p.emitAt(3, Op::ContainerPack, inner, {changed});
                        p.emitAt(3, Op::ContainerAssign, voidType(),
                                 {Operand::typeRow(inner.detail), oldRef, newChild});
                    } else
                        p.emitAt(3, Op::StoreRef, voidType(), {oldRef, changed});
                    const auto now =
                        p.emitAt(3, Op::ContainerRead, element, {load(3, q, home), zero});
                    const auto current =
                        nested ? p.emitAt(3, Op::ContainerRead, e, {now, zero}) : now;
                    const auto answer = p.slot(e);
                    store(3, answer, current);
                    p.emitAt(3, Op::Ret, voidType(), {});
                    ASSERT_TRUE(verifies(p.d));
                    Capture capture;
                    ASSERT_NE(capture.file, nullptr);
                    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                    RootObserver observer;
                    observer.process = ctx.scheduler().addProcess(&RootObserver::resume, &observer,
                                                                  k::Group::Active);
                    ctx.scheduler().start(observer.process);
                    const auto result = kirimasim::vm::run(p.d, ctx);
                    ASSERT_EQ(result.reason, k::EndReason::Quiescent)
                        << result.error << capture.text();
                    ASSERT_TRUE(observer.observed);
                    EXPECT_EQ(observer.stats.containers, nested ? churn ? 2U : 3U : 1U);
                    EXPECT_EQ(observer.stats.cells, nested ? churn ? 2U : 4U : churn ? 1U : 2U);
                    EXPECT_EQ(stored(ctx, oldResult, e)[0].a, p.d.constants[item.id].aWords[0]);
                    EXPECT_EQ(stored(ctx, answer, e)[0].a,
                              p.d.constants[(churn ? changed : item).id].aWords[0]);
                    EXPECT_EQ(ctx.heap().stats().containers, automatic ? 0U : nested ? 2U : 1U);
                    EXPECT_EQ(ctx.heap().stats().cells, automatic ? 0U : nested ? 2U : 1U);
                    EXPECT_LE(ctx.heap().stats().containerEntries, 8U);
                    EXPECT_LE(ctx.heap().stats().cellEntries, 8U);
                }
}

// No wait/delay boundary occurs: a branch must collect with the live SSA published.
TEST(VmContainer, ZeroTimeLoopsBoundContainerAndUniqueStringStorage) {
    for (const bool string : {false, true}) {
        SCOPED_TRACE(string);
        Program p;
        const Type valueType = string ? Type{TypeKind::String, 0} : p.container(s4(32), true);
        const auto home = p.slot(valueType), counter = p.slot(u2(64));
        const auto zero = p.constant(u2(64), 0), one = p.constant(u2(64), 1),
                   count = p.constant(u2(64), 10000);
        p.store(counter, zero);
        p.emit(Op::Br, voidType(), {Operand::block(BlockId{1})});
        const auto i = p.emitAt(1, Op::Load, u2(64), {counter});
        const auto value =
            string ? p.emitAt(1, Op::Syscall, valueType,
                              {Operand::str("$sformatf"), Operand::str("value=%0d"), i})
                   : p.emitAt(1, Op::ContainerPack, valueType, {p.constant(s4(32), 73)});
        p.emitAt(1, Op::Store, voidType(), {home, value});
        const auto next = p.emitAt(1, Op::Add, u2(64), {i, one});
        p.emitAt(1, Op::Store, voidType(), {counter, next});
        const auto again = p.emitAt(1, Op::Lt, u2(1), {next, count});
        p.emitAt(1, Op::BrIf, voidType(),
                 {again, Operand::block(BlockId{1}), Operand::block(BlockId{2})});
        p.emitAt(2, Op::Ret, voidType(), {});
        ASSERT_TRUE(verifies(p.d));
        Capture output;
        ASSERT_NE(output.file, nullptr);
        k::Context context(k::OutputDestinations{output.file, output.file});
        const auto result = kirimasim::vm::run(p.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error << output.text();
        EXPECT_EQ(context.scheduler().time(), 0U);
        EXPECT_EQ(stored(context, counter, u2(64))[0].a, 10000U);
        const auto token = stored(context, home, valueType)[0].a;
        if (string) {
            EXPECT_EQ(context.strings().get(token), "value=9999");
            EXPECT_LE(context.strings().stats().entries, 256U);
        } else {
            const auto type = context.heap().actualType({token});
            ASSERT_TRUE(type);
            std::array<k::W4, 1> leaf{};
            ASSERT_TRUE(context.heap().read(type.value, {token}, 0, leaf));
            EXPECT_EQ(leaf[0].a, 73U);
            EXPECT_LE(context.heap().stats().containerEntries, 1025U);
            EXPECT_LE(context.heap().stats().cellEntries, 1025U);
        }
    }
}

TEST(VmContainer, DroppingTheLastReferenceCollectsWithoutAnotherHeapMutation) {
    Program p;
    const auto q = p.container(s4(32), true);
    const auto home = p.slot(q), ordinary = p.slot(s4(32));
    const auto saved = p.frame(refType(), s4(32));
    const auto zero = p.constant(s4(64), 0), tick = p.constant(u2(64), 1);
    p.store(home, p.emit(Op::ContainerPack, q, {p.constant(s4(32), 7)}));
    const auto ref = p.emit(Op::RefMake, refType(q.detail), {home});
    p.store(saved, p.emit(Op::ContainerCell, refType(), {Operand::typeRow(q.detail), ref, zero}));
    p.emit(Op::ContainerClear, voidType(), {Operand::typeRow(q.detail), ref});
    p.emit(Op::WaitDelay, voidType(), {tick, Operand::block(BlockId{1})});
    const auto plain = p.emitAt(1, Op::RefMake, refType(), {ordinary});
    p.emitAt(1, Op::Fstore, voidType(), {saved, plain});
    p.emitAt(1, Op::WaitDelay, voidType(), {tick, Operand::block(BlockId{2})});
    p.emitAt(2, Op::Ret, voidType(), {});
    ASSERT_TRUE(verifies(p.d));
    Capture capture;
    ASSERT_NE(capture.file, nullptr);
    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
    EXPECT_EQ(kirimasim::vm::run(p.d, ctx).reason, k::EndReason::Quiescent);
    EXPECT_EQ(ctx.heap().stats().containers, 0U);
    EXPECT_EQ(ctx.heap().stats().cells, 0U);
}

TEST(VmContainer, PendingNbaValuesAreRootsInBothSchedulingGroups) {
    for (bool program : {false, true})
        for (bool nested : {false, true})
            for (unsigned delay : {0U, 1U, 9U}) {
                SCOPED_TRACE(::testing::Message() << program << '/' << nested << '/' << delay);
                Program p;
                if (program) p.d.instances[0].kind = InstanceKind::Program;
                const auto inner = p.container(s4(32), true);
                const auto q = nested ? p.container(inner, true) : inner;
                const auto destination = p.slot(q), source = p.frame(q), answer = p.slot(s4(32));
                const auto zero = p.constant(s4(64), 0), item = p.constant(s4(32), 7);
                const auto first = p.emit(Op::ContainerPack, inner, {item});
                const auto value = nested ? p.emit(Op::ContainerPack, q, {first}) : first;
                p.store(source, value);
                p.emit(Op::NbaDelay, voidType(),
                       {destination, p.load(q, source), p.constant(u2(64), delay)});
                p.store(source, p.constant(q, 0));
                p.emit(Op::WaitDelay, voidType(),
                       {p.constant(u2(64), delay + 1), Operand::block(BlockId{1})});
                auto current = p.emitAt(1, Op::Load, q, {destination});
                if (nested) current = p.emitAt(1, Op::ContainerRead, inner, {current, zero});
                const auto leaf = p.emitAt(1, Op::ContainerRead, s4(32), {current, zero});
                p.emitAt(1, Op::Store, voidType(), {answer, leaf});
                p.emitAt(1, Op::Ret, voidType(), {});
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                    << result.error << capture.text();
                EXPECT_EQ(stored(ctx, answer, s4(32))[0].a, 7U);
                EXPECT_EQ(ctx.heap().stats().containers, nested ? 2U : 1U);
                EXPECT_EQ(ctx.heap().stats().cells, nested ? 2U : 1U);
            }
}

TEST(VmContainer, FixedArraysOfContainerValuesSurviveRegionBoundaries) {
    for (bool automatic : {false, true})
        for (bool nested : {false, true})
            for (bool descending : {false, true}) {
                SCOPED_TRACE(::testing::Message()
                             << automatic << '/' << nested << '/' << descending);
                Program p;
                const auto inner = p.container(s4(129), true);
                const auto q = nested ? p.container(inner, true) : inner;
                p.frame(s4(129));  // Root field offsets must include wide scalar fields.
                const auto home = automatic ? p.frame(q) : p.slot(q);
                const std::vector<ArrayDim> dims{{descending ? 2 : 0, descending ? 0 : 2}};
                if (automatic)
                    p.d.processes[0].frame[home.id].dims = dims;
                else
                    p.d.slots[home.id].dims = dims;
                p.frame(u2(64));
                const auto zero = p.constant(s4(64), 0);
                std::vector<std::pair<Operand, Operand>> checks;
                for (unsigned index = 0; index < 3; ++index) {
                    const auto leaf = p.constant(s4(129), index + 1);
                    auto value = p.emit(Op::ContainerPack, inner, {leaf});
                    if (nested) value = p.emit(Op::ContainerPack, q, {value});
                    const auto at = p.constant(s4(64), index);
                    p.emit(Op::StoreElem, voidType(), {home, at, value});
                    const auto output = p.slot(s4(129));
                    auto current = p.emitAt(1, Op::LoadElem, q, {home, at});
                    if (nested) current = p.emitAt(1, Op::ContainerRead, inner, {current, zero});
                    p.emitAt(1, Op::Store, voidType(),
                             {output, p.emitAt(1, Op::ContainerRead, s4(129), {current, zero})});
                    checks.push_back({output, leaf});
                }
                p.emit(Op::WaitDelay, voidType(),
                       {p.constant(u2(64), 1), Operand::block(BlockId{1})});
                p.emitAt(1, Op::Ret, voidType(), {});
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error << capture.text();
                for (const auto& [output, expected] : checks) {
                    const auto value = stored(ctx, output, s4(129));
                    for (unsigned w = 0; w < value.size(); ++w)
                        EXPECT_EQ(value[w].a, p.d.constants[expected.id].aWords[w]);
                }
                EXPECT_EQ(ctx.heap().stats().containers, automatic ? 0U : nested ? 6U : 3U);
                EXPECT_EQ(ctx.heap().stats().cells, automatic ? 0U : nested ? 6U : 3U);
            }
}

TEST(VmContainer, PrimitiveWordsThatResembleTokensAreNotRoots) {
    for (Type e : {u2(64), u4(64), Type{TypeKind::Real, 0}})
        for (bool automatic : {false, true}) {
            Program p;
            const auto q = p.container(s4(32), true);
            const auto home = automatic ? p.frame(e) : p.slot(e);
            const auto bits = p.constant(e, 0x100000001ULL, 0x8000000000000000ULL);
            p.store(home, bits);
            p.emit(Op::ContainerPack, q, {p.constant(s4(32), 7)});  // Unowned result.
            p.emit(Op::WaitDelay, voidType(), {p.constant(u2(64), 1), Operand::block(BlockId{1})});
            const auto output = p.slot(e);
            const auto read = p.emitAt(1, automatic ? Op::Fload : Op::Load, e, {home});
            p.emitAt(1, Op::Store, voidType(), {output, read});
            p.emitAt(1, Op::Ret, voidType(), {});
            ASSERT_TRUE(verifies(p.d));
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            k::Context ctx(k::OutputDestinations{capture.file, capture.file});
            ASSERT_EQ(kirimasim::vm::run(p.d, ctx).reason, k::EndReason::Quiescent);
            EXPECT_EQ(stored(ctx, output, e)[0].a, 0x100000001ULL);
            EXPECT_EQ(stored(ctx, output, e)[0].b, isFourState(e) ? 0x8000000000000000ULL : 0U);
            EXPECT_EQ(ctx.heap().stats().containers, 0U);
            EXPECT_EQ(ctx.heap().stats().cells, 0U);
        }
}

TEST(VmContainer, DeferredInputSnapshotsRemainRootsUntilObservedOrPostponedCalls) {
    for (bool final : {false, true})
        for (bool nested : {false, true})
            for (bool multiple : {false, true})
                for (bool automatic : {false, true}) {
                    SCOPED_TRACE(::testing::Message()
                                 << final << '/' << nested << '/' << multiple << '/' << automatic);
                    Program p;
                    const auto inner = p.container(s4(32), true);
                    const auto q = nested ? p.container(inner, true) : inner;
                    const auto source = p.slot(q);
                    const auto zero = p.constant(s4(64), 0);
                    std::vector<Operand> call{Operand::str(final ? "final" : "#0"),
                                              Operand::subroutine(SubroutineId{0})};
                    for (unsigned i = 0; i < (multiple ? 2U : 1U); ++i) {
                        const auto first =
                            p.emit(Op::ContainerPack, inner, {p.constant(s4(32), 7 + 2 * i)});
                        const auto value = nested ? p.emit(Op::ContainerPack, q, {first}) : first;
                        p.store(source, value);
                        call.push_back(p.emit(Op::ContainerSnapshot, q, {p.load(q, source)}));
                    }
                    p.emit(Op::ReportCall, voidType(), std::move(call));
                    const auto sourceRef = p.emit(Op::RefMake, refType(q.detail), {source});
                    p.emit(Op::ContainerClear, voidType(), {Operand::typeRow(q.detail), sourceRef});
                    p.emit(Op::WaitDelay, voidType(),
                           {p.constant(u2(64), 1), Operand::block(BlockId{1})});
                    p.emitAt(1, Op::Ret, voidType(), {});
                    Program fn;
                    fn.d.types = p.d.types;
                    fn.d.constants = p.d.constants;
                    Subroutine function;
                    function.name = "top.report";
                    function.automatic = automatic;
                    for (unsigned i = 0; i < (multiple ? 2U : 1U); ++i) {
                        const auto param = automatic ? fn.frame(q) : p.slot(q);
                        if (!automatic) p.d.slots[param.id].subroutine = SubroutineId{0};
                        function.params.push_back(
                            {"value" + std::to_string(i), Direction::Input, q,
                             automatic ? SlotId{kNone} : param.asSlot(),
                             automatic ? param.asFrame() : FrameVarId{kNone}});
                        auto value = fn.load(q, param);
                        if (nested) value = fn.emit(Op::ContainerRead, inner, {value, zero});
                        const auto leaf = fn.emit(Op::ContainerRead, s4(32), {value, zero});
                        fn.emit(Op::Syscall, voidType(),
                                {Operand::str("$display"), Operand::str("value=%0d"), leaf});
                    }
                    fn.end();
                    function.frame = fn.d.processes[0].frame;
                    function.blocks = fn.d.processes[0].blocks;
                    function.numTemps = fn.d.processes[0].numTemps;
                    function.loc = {0, 1, 1};
                    p.d.constants = std::move(fn.d.constants);
                    p.d.subroutines.push_back(std::move(function));
                    ASSERT_TRUE(verifies(p.d));
                    Capture capture;
                    ASSERT_NE(capture.file, nullptr);
                    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                    const auto result = kirimasim::vm::run(p.d, ctx);
                    ASSERT_EQ(result.reason, k::EndReason::Quiescent)
                        << result.error << capture.text();
                    EXPECT_EQ(capture.text(), multiple ? "value=7\nvalue=9\n" : "value=7\n");
                    const unsigned count = automatic ? 0 : (multiple ? 2 : 1) * (nested ? 2 : 1);
                    EXPECT_EQ(ctx.heap().stats().containers, count);
                    EXPECT_EQ(ctx.heap().stats().cells, count);
                }
}

TEST(VmContainer, RootHookIsRestoredAfterNormalAndFailedRuns) {
    for (bool fail : {false, true}) {
        Program p;
        const auto q = p.container(s4(32), true);
        const auto home = p.slot(q);
        p.store(home, p.emit(Op::ContainerPack, q, {p.constant(s4(32), 7)}));
        p.end();
        ASSERT_TRUE(verifies(p.d));
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        VmHeapMemory memory;
        if (fail) memory.remaining = 0;
        k::Context ctx(k::OutputDestinations{capture.file, capture.file}, nullptr, &memory);
        unsigned visits = 0;
        const auto callback = +[](void* frame) noexcept { ++*static_cast<unsigned*>(frame); };
        ctx.scheduler().setSafePointHook({callback, &visits});
        EXPECT_EQ(kirimasim::vm::run(p.d, ctx).reason,
                  fail ? k::EndReason::ResourceLimit : k::EndReason::Quiescent);
        memory.remaining.reset();
        const auto restored = ctx.scheduler().setSafePointHook({});
        EXPECT_EQ(restored.fn, callback);
        EXPECT_EQ(restored.frame, &visits);
        restored.fn(restored.frame);
        EXPECT_EQ(visits, 1U);
    }
}

struct StopSuspendedProcess {
    k::ProcessId process{0};
    k::ProcessId target{1};
    bool waited = false;
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& state = *static_cast<StopSuspendedProcess*>(frame);
        auto& scheduler = k::fromC(context).scheduler();
        if (!state.waited) {
            state.waited = true;
            scheduler.waitDelay(state.process, 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        scheduler.kill(state.target);
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(VmContainer, KernelEndedActivationsDoNotKeepDeadValuesRooted) {
    Program p;
    const auto q = p.container(s4(32), true);
    const auto home = p.frame(q);
    p.store(home, p.emit(Op::ContainerPack, q, {p.constant(s4(32), 7)}));
    p.emit(Op::WaitDelay, voidType(), {p.constant(u2(64), 10), Operand::block(BlockId{1})});
    p.emitAt(1, Op::Ret, voidType(), {});
    ASSERT_TRUE(verifies(p.d));
    Capture capture;
    ASSERT_NE(capture.file, nullptr);
    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
    StopSuspendedProcess stop;
    stop.process =
        ctx.scheduler().addProcess(&StopSuspendedProcess::resume, &stop, k::Group::Active);
    ctx.scheduler().start(stop.process);
    EXPECT_EQ(kirimasim::vm::run(p.d, ctx).reason, k::EndReason::Quiescent);
    EXPECT_EQ(ctx.heap().stats().containers, 0U);
    EXPECT_EQ(ctx.heap().stats().cells, 0U);
}

struct MonitorHeapArm {
    VmHeapMemory& memory;
    k::ProcessId process{0};
    bool waited = false;
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& arm = *static_cast<MonitorHeapArm*>(frame);
        if (!arm.waited) {
            arm.waited = true;
            k::fromC(context).scheduler().waitDelay(arm.process, 0);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        arm.memory.remaining = 0;
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(VmContainer, HeapMonitorFailuresHavePositionsAndWaitHooksNeverOutliveTheInterpreter) {
    for (bool fail : {false, true})
        for (bool event : {false, true}) {
            SCOPED_TRACE(::testing::Message() << fail << '/' << event);
            Program p;
            const auto q = p.container(s4(32), true);
            const auto home = p.slot(q);
            p.store(home, p.emit(Op::ContainerPack, q, {p.constant(s4(32), 7)}));
            p.emit(Op::WaitDelay, voidType(), {p.constant(u2(64), 0), Operand::block(BlockId{1})});
            p.emitAt(1, event ? Op::WaitEvent : Op::WaitCond, voidType(),
                     {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{2})});
            p.d.processes[0].blocks[1].instrs.back().loc = {0, 43, 5};
            p.emitAt(2, Op::Ret, voidType(), {});
            Subroutine guard;
            guard.name = "top.watch";
            guard.kind = event ? SubroutineKind::Watch : SubroutineKind::Guard;
            guard.returnType = u2(1);
            guard.frameOf = {Owner::Kind::Process, 0};
            guard.loc = {0, 43, 5};
            guard.blocks.resize(1);
            guard.numTemps = 3;
            guard.blocks[0].instrs = {
                {Op::Load, q, TempId{0}, {home}, {0, 43, 5}},
                {Op::ContainerSize, u2(64), TempId{1}, {Operand::temp(TempId{0})}, {0, 43, 5}},
                {Op::Ceq,
                 u2(1),
                 TempId{2},
                 {Operand::temp(TempId{1}), p.constant(u2(64), 2)},
                 {0, 43, 5}},
                {Op::Ret, voidType(), kNone, {Operand::temp(TempId{2})}, {0, 43, 5}}};
            p.d.subroutines.push_back(std::move(guard));
            ASSERT_TRUE(verifies(p.d));
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            VmHeapMemory memory;
            k::Context ctx(k::OutputDestinations{capture.file, capture.file}, nullptr, &memory);
            MonitorHeapArm arm{memory};
            if (fail) {
                arm.process =
                    ctx.scheduler().addProcess(&MonitorHeapArm::resume, &arm, k::Group::Active);
                ctx.scheduler().start(arm.process);
            }
            struct Hook {
                unsigned calls = 0;
                static void change(void* frame, k::HeapRoot) noexcept {
                    ++static_cast<Hook*>(frame)->calls;
                }
            } hook;
            ctx.heap().setChangeHook({&Hook::change, &hook});
            const auto result = kirimasim::vm::run(p.d, ctx);
            memory.remaining.reset();
            ASSERT_EQ(result.reason, fail ? k::EndReason::ResourceLimit : k::EndReason::Quiescent)
                << result.error << capture.text();
            if (fail)
                EXPECT_NE(
                    capture.text().find("container.sv:43: error: container monitor resource limit"),
                    std::string::npos);
            else
                EXPECT_TRUE(capture.text().empty());
            const auto restored = ctx.heap().setChangeHook({});
            EXPECT_EQ(restored.fn, &Hook::change);
            EXPECT_EQ(restored.frame, &hook);
            ctx.heap().setChangeHook(restored);
            k::ContainerToken value{stored(ctx, home, q)[0].a};
            const auto type = ctx.heap().actualType(value);
            ASSERT_TRUE(type);
            ASSERT_TRUE(ctx.heap().write(type.value, value, 0, std::array<k::W4, 1>{{{11, 0}}}));
            ctx.scheduler().flushHeapChanges();
            EXPECT_EQ(hook.calls, 1U);
        }
}

TEST(VmContainer, HeapWaitsExcludeCalledPrivateContainersButKeepHierarchicalAndGlobalReads) {
    for (bool called : {false, true})
        for (bool nested : {false, true})
            for (bool forwarded : {false, true})
                for (bool event : {false, true}) {
                    SCOPED_TRACE(::testing::Message()
                                 << called << '/' << nested << '/' << forwarded << '/' << event);
                    Program p;
                    const auto inner = p.container(s4(32), true);
                    const auto q = nested ? p.container(inner, true) : inner;
                    const auto privateQ = p.slot(q), publicQ = p.slot(q);
                    p.d.slots[privateQ.id].subroutine = SubroutineId{0};
                    const auto phase = p.slot(u2(32)), observed = p.slot(u2(32));
                    const auto zero = p.constant(s4(64), 0), leaf = p.constant(s4(32), 1);
                    for (const auto& home : {privateQ, publicQ}) {
                        const auto value = nested ? p.emit(Op::ContainerPack, inner, {leaf}) : leaf;
                        p.store(home, p.emit(Op::ContainerPack, q, {value}));
                    }
                    Subroutine getter;
                    getter.name = "top.getter";
                    getter.returnType = u2(64);
                    getter.blocks.resize(1);
                    auto& code = getter.blocks[0].instrs;
                    const auto emit = [&](Op op, Type t, std::vector<Operand> args) {
                        const TempId id{getter.numTemps++};
                        code.push_back({op, t, id, std::move(args), {0, 40, 1}});
                        return Operand::temp(id);
                    };
                    std::vector<Operand> sizes;
                    for (const auto& home : {privateQ, publicQ}) {
                        auto value = emit(Op::Load, q, {home});
                        if (nested) value = emit(Op::ContainerRead, inner, {value, zero});
                        sizes.push_back(emit(Op::ContainerSize, u2(64), {value}));
                    }
                    const auto sum = emit(Op::Add, u2(64), sizes);
                    code.push_back({Op::Ret, voidType(), kNone, {sum}, {0, 40, 1}});
                    p.d.subroutines.push_back(getter);
                    if (forwarded) {
                        Subroutine wrapper;
                        wrapper.name = "top.wrapper";
                        wrapper.returnType = u2(64);
                        wrapper.numTemps = 1;
                        wrapper.blocks.resize(1);
                        wrapper.blocks[0].instrs = {
                            {Op::Call,
                             u2(64),
                             TempId{0},
                             {Operand::subroutine(SubroutineId{0})},
                             {0, 41, 1}},
                            {Op::Ret, voidType(), kNone, {Operand::temp(TempId{0})}, {0, 41, 1}}};
                        p.d.subroutines.push_back(std::move(wrapper));
                    }
                    const SubroutineId guardId{static_cast<std::uint32_t>(p.d.subroutines.size())};
                    Subroutine guard;
                    guard.name = "top.guard";
                    guard.kind = event ? SubroutineKind::Watch : SubroutineKind::Guard;
                    guard.returnType = u2(1);
                    guard.frameOf = {Owner::Kind::Process, 0};
                    guard.loc = {0, 43, 1};
                    guard.blocks.resize(1);
                    Operand result;
                    if (called) {
                        guard.blocks[0].instrs.push_back(
                            {Op::Call,
                             u2(64),
                             TempId{0},
                             {Operand::subroutine(SubroutineId{forwarded ? 1U : 0U})},
                             {0, 43, 1}});
                        guard.numTemps = 1;
                        result = Operand::temp(TempId{0});
                    } else {
                        guard.blocks = getter.blocks;
                        guard.blocks[0].instrs.pop_back();
                        guard.numTemps = getter.numTemps;
                        result = sum;
                    }
                    const TempId test{guard.numTemps++};
                    guard.blocks[0].instrs.push_back(
                        {Op::Ge, u2(1), test, {result, p.constant(u2(64), 3)}, {0, 43, 1}});
                    guard.blocks[0].instrs.push_back(
                        {Op::Ret, voidType(), kNone, {Operand::temp(test)}, {0, 43, 1}});
                    p.d.subroutines.push_back(std::move(guard));
                    p.emit(event ? Op::WaitEvent : Op::WaitCond, voidType(),
                           {Operand::subroutine(guardId), Operand::block(BlockId{1})});
                    const auto at = p.emitAt(1, Op::Load, u2(32), {phase});
                    p.emitAt(1, Op::Store, voidType(), {observed, at});
                    p.emitAt(1, Op::Ret, voidType(), {});
                    Process writer;
                    writer.name = "top.writer";
                    writer.blocks.resize(3);
                    writer.blocks[0].instrs = {{Op::WaitDelay,
                                                voidType(),
                                                kNone,
                                                {p.constant(u2(64), 1), Operand::block(BlockId{1})},
                                                {0, 45, 1}}};
                    for (unsigned step : {1U, 2U}) {
                        const auto home = step == 1 ? privateQ : publicQ;
                        const TempId binding{(step - 1) * (nested ? 2U : 1U)};
                        const TempId element{binding.index() + 1};
                        auto& out = writer.blocks[step].instrs;
                        out.push_back({Op::Store,
                                       voidType(),
                                       kNone,
                                       {phase, p.constant(u2(32), step)},
                                       {0, 46, 1}});
                        out.push_back(
                            {Op::RefMake, refType(q.detail), binding, {home}, {0, 46, 1}});
                        auto receiver = Operand::temp(binding);
                        if (nested) {
                            out.push_back({Op::ContainerCell,
                                           refType(inner.detail),
                                           element,
                                           {Operand::typeRow(q.detail), receiver, zero},
                                           {0, 46, 1}});
                            receiver = Operand::temp(element);
                        }
                        out.push_back({Op::QueuePushBack,
                                       voidType(),
                                       kNone,
                                       {Operand::typeRow(inner.detail), receiver, leaf},
                                       {0, 46, 1}});
                        out.push_back(
                            step == 1 ? Instr{Op::WaitDelay,
                                              voidType(),
                                              kNone,
                                              {p.constant(u2(64), 1), Operand::block(BlockId{2})},
                                              {0, 47, 1}}
                                      : Instr{Op::Ret, voidType(), kNone, {}, {0, 47, 1}});
                    }
                    writer.numTemps = nested ? 4 : 2;
                    writer.writes = {{privateQ.asSlot()}, {publicQ.asSlot()}, {phase.asSlot()}};
                    p.d.processes.push_back(std::move(writer));
                    ASSERT_TRUE(verifies(p.d));
                    Capture capture;
                    ASSERT_NE(capture.file, nullptr);
                    k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                    const auto resultRun = kirimasim::vm::run(p.d, ctx);
                    ASSERT_EQ(resultRun.reason, k::EndReason::Quiescent)
                        << resultRun.error << capture.text();
                    EXPECT_EQ(stored(ctx, observed, u2(32))[0].a, called ? 2U : 1U);
                    EXPECT_TRUE(capture.text().empty());
                }
}

TEST(VmContainer, SwapTransportsWideIndicesAndPreservesCapturedElementReferences) {
    for (const bool queue : {false, true})
        for (const bool frame : {false, true})
            for (unsigned indexKind = 0; indexKind < 5; ++indexKind) {
                SCOPED_TRACE(::testing::Message() << queue << '/' << frame << '/' << indexKind);
                Program p;
                const auto element = u4(65);
                const auto type = p.container(element, queue, queue ? 3 : 0);
                const auto home = frame ? p.frame(type) : p.slot(type);
                const auto a = p.constant(element, 17), b = p.constant(element, 29);
                p.store(home, p.emit(Op::ContainerPack, type, {a, b}));
                const auto receiver = p.emit(Op::RefMake, refType(type.detail), {home});
                const auto zero = p.constant(u2(64), 0);
                const auto cell = p.emit(Op::ContainerCell, refType(),
                                         {Operand::typeRow(type.detail), receiver, zero});
                Constant index;
                index.type = u4(129);
                index.aWords = {1, 0, indexKind == 1 ? 1U : 0U};
                index.bWords = {indexKind == 2 ? 1U : 0U, 0, 0};
                if (indexKind == 3) {
                    index.type = s4(129);
                    index.aWords = {~std::uint64_t{0}, ~std::uint64_t{0}, 1};
                }
                if (indexKind == 4) index.aWords = {2, 0, 0};
                p.emit(Op::ContainerSwapValues, voidType(),
                       {Operand::typeRow(type.detail), receiver, zero, p.constant(index)});
                const auto observed = p.slot(element);
                p.store(observed, p.emit(Op::LoadRef, element, {cell}));
                p.end();
                ASSERT_TRUE(verifies(p.d));
                Capture capture;
                ASSERT_NE(capture.file, nullptr);
                k::Context ctx(k::OutputDestinations{capture.file, capture.file});
                const auto result = kirimasim::vm::run(p.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                const auto got = stored(ctx, observed, element);
                ASSERT_EQ(got.size(), 2U);
                const auto& expected = p.d.constants[(indexKind == 0 ? b : a).id];
                for (unsigned w = 0; w < 2; ++w) {
                    EXPECT_EQ(got[w].a, expected.aWords[w]);
                    EXPECT_EQ(got[w].b, 0U);
                }
                EXPECT_TRUE(capture.text().empty());
            }
}

}  // namespace
