// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/urandom.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
namespace vm = kirimasim::vm;
using s::Op;
using s::Operand;
using Mode = vm::InitValuesMode;

struct Program {
    s::Design d;
    Program() {
        d.sources = {"init-values.sv"};
        d.instances = {{"top", "top", s::kNone}};
        s::Process process;
        process.name = "top.main";
        process.kind = s::ProcessKind::Initial;
        process.loc = {0, 1, 1};
        process.blocks.resize(1);
        d.processes.push_back(std::move(process));
    }
    Operand slot(s::Type type, bool sourceDefault = false, std::vector<s::ArrayDim> dims = {},
                 s::SlotKind kind = s::SlotKind::Var) {
        s::Slot slot;
        const auto id = static_cast<std::uint32_t>(d.slots.size());
        slot.name = "top.v" + std::to_string(id);
        slot.type = type;
        slot.kind = kind;
        slot.dims = std::move(dims);
        slot.flags = s::kObservable;
        slot.defaultInit = sourceDefault;
        d.slots.push_back(std::move(slot));
        d.processes[0].writes.push_back({s::SlotId{id}});
        return Operand::slot(s::SlotId{id});
    }
    Operand emit(Op op, std::vector<Operand> operands = {}, s::Type type = s::voidType(),
                 bool sub = false) {
        auto& blocks = sub ? d.subroutines[0].blocks : d.processes[0].blocks;
        auto& temps = sub ? d.subroutines[0].numTemps : d.processes[0].numTemps;
        const s::TempId id{type == s::voidType() ? s::kNone : temps++};
        blocks[0].instrs.push_back({op, type, id, std::move(operands), {0, 1, 1}});
        return Operand::temp(id);
    }
    void store(const Operand& target, const Operand& value) { emit(Op::Store, {target, value}); }
    ::testing::AssertionResult valid() const {
        const auto errors = s::verify(d);
        if (errors.empty()) return ::testing::AssertionSuccess();
        std::string message;
        for (const auto& error : errors)
            message += error.where + ": " + error.message + "\n";
        return ::testing::AssertionFailure() << message;
    }
};

vm::RunOptions options(Mode mode, std::uint64_t seed = 91) {
    vm::RunOptions options;
    options.initValues = mode;
    options.seed = seed;
    return options;
}

k::W4 expected(Mode mode, unsigned bits, k::Rng& rng) {
    const auto mask = k::mask(bits);
    switch (mode) {
        case Mode::X: return {mask, mask};
        case Mode::Zero: return {0, 0};
        case Mode::One: return {mask, 0};
        case Mode::Random: return {rng.next() & mask, 0};
    }
    return {};
}

void checkSlot(const k::Context& context, const Operand& slot, unsigned width, Mode mode,
               k::Rng& rng, unsigned element = 0) {
    const auto* words = context.store().element(k::SlotId{slot.id}, element);
    for (unsigned word = 0; word < (width + 63) / 64; ++word) {
        const auto want = expected(mode, std::min(64U, width - 64 * word), rng);
        EXPECT_EQ(words[std::size_t{word} * 2], want.a) << word;
        EXPECT_EQ(words[std::size_t{word} * 2 + 1], want.b) << word;
    }
}

TEST(InitValues, StaticMarkersInitializeEachWideArrayElementAndLeaveOtherDefaults) {
    for (const auto mode : {Mode::X, Mode::Zero, Mode::One, Mode::Random}) {
        SCOPED_TRACE(static_cast<unsigned>(mode));
        Program p;
        const auto source = p.slot(s::u4(129), true, {{1, 0}});
        const auto unmarked = p.slot(s::u4(129));
        const auto net = p.slot(s::u4(1), false, {}, s::SlotKind::Wire);
        const auto two = p.slot(s::u2(129));
        p.emit(Op::Ret);
        ASSERT_TRUE(p.valid());
        k::Context context;
        const auto result = vm::run(p.d, context, options(mode));
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        k::Rng rng{91};
        checkSlot(context, source, 129, mode, rng, 0);
        checkSlot(context, source, 129, mode, rng, 1);
        checkSlot(context, unmarked, 129, Mode::X, rng);
        const auto* netWord = context.store().element(k::SlotId{net.id}, 0);
        EXPECT_EQ(netWord[0], 0U);
        EXPECT_EQ(netWord[1], 1U);
        const auto* twoWords = context.store().element(k::SlotId{two.id}, 0);
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_EQ(twoWords[i], 0U);
    }
}

TEST(InitValues, DefaultInitOpcodeUsesFreshWideKnownBitsAndTheRunSeed) {
    for (const auto seed : {0ULL, 91ULL, 0x123456789abcdef0ULL}) {
        for (const auto mode : {Mode::X, Mode::Zero, Mode::One, Mode::Random}) {
            Program p;
            const auto first = p.slot(s::u4(129)), second = p.slot(s::u4(129));
            p.store(first, p.emit(Op::DefaultInit, {}, s::u4(129)));
            p.store(second, p.emit(Op::DefaultInit, {}, s::u4(129)));
            p.emit(Op::Ret);
            ASSERT_TRUE(p.valid());
            k::Context context;
            const auto result = vm::run(p.d, context, options(mode, seed));
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            k::Rng rng{seed};
            checkSlot(context, first, 129, mode, rng);
            checkSlot(context, second, 129, mode, rng);
        }
    }
}

TEST(InitValues, OrdinaryAndScopedOutputsAndReturnsInitializeOncePerCall) {
    for (const bool scoped : {false, true}) {
        // Output task, automatic function return, and static function return.
        for (unsigned kind = 0; kind < 3; ++kind) {
            for (const auto mode : {Mode::X, Mode::Zero, Mode::One, Mode::Random}) {
                SCOPED_TRACE(::testing::Message()
                             << scoped << '/' << kind << '/' << static_cast<unsigned>(mode));
                Program p;
                const auto type = s::u4(129);
                const auto first = p.slot(type), second = p.slot(type);
                s::Subroutine sub;
                sub.name = "top.f";
                sub.kind = kind == 0 ? s::SubroutineKind::Task : s::SubroutineKind::Function;
                sub.automatic = kind != 2;
                sub.returnType = kind == 0 ? s::voidType() : type;
                sub.loc = {0, 1, 1};
                sub.blocks.resize(1);
                sub.frame.push_back({"fresh", type});
                sub.frame[0].defaultInit = true;
                if (scoped) {
                    sub.frame[0].home = s::FrameHome::Scoped;
                    sub.frame[0].activationScope = s::ActivationScopeId{0};
                    sub.activation.root = s::ActivationScopeId{0};
                    sub.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
                }
                if (kind == 0) {
                    sub.params = {
                        {"fresh", s::Direction::Output, type, s::kNone, s::FrameVarId{0}}};
                    p.d.processes[0].frame.push_back({"actual", type});
                }
                p.d.subroutines.push_back(std::move(sub));
                const auto frame = Operand::frame(s::FrameVarId{0});
                if (kind == 0)
                    p.emit(Op::Ret, {}, s::voidType(), true);
                else {
                    const auto value = p.emit(Op::Fload, {frame}, type, true);
                    p.emit(Op::Ret, {value}, s::voidType(), true);
                }
                for (const auto& target : {first, second}) {
                    std::vector<Operand> args{Operand::subroutine(s::SubroutineId{0})};
                    if (kind == 0) args.push_back(frame);
                    auto value =
                        p.emit(Op::Call, std::move(args), kind == 0 ? s::voidType() : type);
                    if (kind == 0) value = p.emit(Op::Fload, {frame}, type);
                    p.store(target, value);
                }
                p.emit(Op::Ret);
                ASSERT_TRUE(p.valid());
                k::Context context;
                const auto result = vm::run(p.d, context, options(mode));
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                k::Rng rng{91};
                checkSlot(context, first, 129, mode, rng);
                checkSlot(context, second, 129, mode, rng);
                EXPECT_EQ(context.activations().stats().live, 0U);
            }
        }
    }
}

TEST(InitValues, RandomInitializationDoesNotConsumeRandomOrUrandomGenerators) {
    Program p;
    p.slot(s::u4(129), true, {{0, 4}});
    const auto urandom = p.slot(s::u2(32)), random = p.slot(s::s2(32));
    p.emit(Op::DefaultInit, {}, s::u4(129));
    p.store(urandom, p.emit(Op::Syscall, {Operand::str("$urandom")}, s::u2(32)));
    p.store(random, p.emit(Op::Syscall, {Operand::str("$random")}, s::s2(32)));
    p.emit(Op::Ret);
    ASSERT_TRUE(p.valid());
    k::Context baseline, initialized;
    const auto normal = vm::run(p.d, baseline, options(Mode::X));
    const auto randomized = vm::run(p.d, initialized, options(Mode::Random));
    ASSERT_EQ(normal.reason, k::EndReason::Quiescent) << normal.error;
    ASSERT_EQ(randomized.reason, k::EndReason::Quiescent) << randomized.error;
    for (const auto& slot : {urandom, random})
        EXPECT_EQ(baseline.store().element(k::SlotId{slot.id}, 0)[0],
                  initialized.store().element(k::SlotId{slot.id}, 0)[0]);
    EXPECT_EQ(baseline.randomSeed(), initialized.randomSeed());
}

TEST(InitValues, UnmarkedScopedImplicitReturnsUseXWithoutConsumingPolicyRandomBits) {
    for (const bool automatic : {false, true}) {
        for (const auto width : {1U, 65U, 129U}) {
            for (const auto mode : {Mode::X, Mode::Zero, Mode::One, Mode::Random}) {
                SCOPED_TRACE(::testing::Message()
                             << automatic << '/' << width << '/' << static_cast<unsigned>(mode));
                Program p;
                const auto type = s::u4(width);
                const auto first = p.slot(type), second = p.slot(type), policy = p.slot(type);
                s::Subroutine sub;
                sub.name = "top.f";
                sub.automatic = automatic;
                sub.returnType = type;
                sub.loc = {0, 1, 1};
                sub.blocks.resize(1);
                sub.frame.push_back({"implicit_return", type});
                sub.frame[0].home = s::FrameHome::Scoped;
                sub.frame[0].activationScope = s::ActivationScopeId{0};
                sub.activation.root = s::ActivationScopeId{0};
                sub.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
                p.d.subroutines.push_back(std::move(sub));
                const auto value =
                    p.emit(Op::Fload, {Operand::frame(s::FrameVarId{0})}, type, true);
                p.emit(Op::Ret, {value}, s::voidType(), true);
                for (const auto& target : {first, second})
                    p.store(target,
                            p.emit(Op::Call, {Operand::subroutine(s::SubroutineId{0})}, type));
                p.store(policy, p.emit(Op::DefaultInit, {}, type));
                p.emit(Op::Ret);
                ASSERT_TRUE(p.valid());
                k::Context context;
                const auto result = vm::run(p.d, context, options(mode));
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                k::Rng rng{91};
                checkSlot(context, first, width, Mode::X, rng);
                checkSlot(context, second, width, Mode::X, rng);
                checkSlot(context, policy, width, mode, rng);
                EXPECT_EQ(context.activations().stats().live, 0U);
            }
        }
    }
}

struct FreshWords {
    std::uint64_t calls = 0;
    static std::uint64_t word(void* frame) noexcept {
        auto& state = *static_cast<FreshWords*>(frame);
        return ++state.calls;
    }
    k::HeapFreshInit hook() { return {word, this}; }
};

TEST(InitValuesHeap, OnlyFreshFourStateTailWordsUseTheConstructionInitializer) {
    k::Heap heap;
    const std::array<k::HeapWord, 3> words{
        {{0x7f, true, {0x7f, 0x7f}}, {0xff, false, {0, 0}}, {1, true, {1, 1}}}};
    const auto layout = heap.addLayout(words);
    ASSERT_TRUE(layout);
    const auto type = heap.addType({k::ContainerKind::DynamicArray, layout.value});
    const auto queue = heap.addType({k::ContainerKind::Queue, layout.value});
    ASSERT_TRUE(type);
    ASSERT_TRUE(queue);
    const std::array<k::W4, 3> original{{{0x52, 0x25}, {0xab, 0}, {0, 1}}};
    const auto source = heap.fromValues(type.value, original);
    ASSERT_TRUE(source);
    FreshWords state;
    const auto result = heap.construct(type.value, 4, source.value, state.hook());
    ASSERT_TRUE(result);
    EXPECT_EQ(state.calls, 6U);
    std::array<k::W4, 3> value;
    ASSERT_TRUE(heap.read(type.value, result.value, 0, value));
    for (unsigned word = 0; word < 3; ++word) {
        EXPECT_EQ(value[word].a, original[word].a);
        EXPECT_EQ(value[word].b, original[word].b);
    }
    for (unsigned e = 1; e < 4; ++e) {
        ASSERT_TRUE(heap.read(type.value, result.value, e, value));
        EXPECT_EQ(value[0].a, 2 * e - 1);
        EXPECT_EQ(value[0].b, 0U);
        EXPECT_EQ(value[1].a, 0U);
        EXPECT_EQ(value[1].b, 0U);
        EXPECT_EQ(value[2].a, (2 * e) & 1U);
        EXPECT_EQ(value[2].b, 0U);
    }
    ASSERT_TRUE(heap.construct(type.value, 1, result.value, state.hook()));
    ASSERT_TRUE(heap.construct(type.value, 4, result.value, state.hook()));
    ASSERT_TRUE(heap.snapshot(result.value));
    ASSERT_TRUE(heap.copy(type.value, result.value));
    EXPECT_EQ(state.calls, 6U);
    ASSERT_TRUE(heap.read(type.value, result.value, 9, value));
    for (unsigned word = 0; word < 3; ++word) {
        EXPECT_EQ(value[word].a, words[word].initial.a);
        EXPECT_EQ(value[word].b, words[word].initial.b);
    }
    k::ContainerToken empty;
    ASSERT_TRUE(heap.pop(queue.value, empty, false, value));
    for (unsigned word = 0; word < 3; ++word) {
        EXPECT_EQ(value[word].a, words[word].initial.a);
        EXPECT_EQ(value[word].b, words[word].initial.b);
    }
}

class FailingMemory final : public std::pmr::memory_resource {
public:
    std::optional<unsigned> remaining;

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

TEST(InitValuesHeap, FailedConstructionNeverConsumesInitializerWords) {
    unsigned failures = 0;
    bool succeeded = false;
    for (unsigned after = 0; after < 80; ++after) {
        FailingMemory memory;
        k::Heap heap{&memory};
        const std::array<k::HeapWord, 1> words{{{0x7f, true, {0x7f, 0x7f}}}};
        const auto layout = heap.addLayout(words);
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::DynamicArray, layout.value});
        ASSERT_TRUE(type);
        memory.remaining = after;
        FreshWords state;
        const auto result = heap.construct(type.value, 4, {}, state.hook());
        if (result) {
            EXPECT_EQ(state.calls, 4U);
            succeeded = true;
            break;
        }
        EXPECT_EQ(result.status.error, k::HeapError::ResourceLimit);
        EXPECT_EQ(state.calls, 0U);
        EXPECT_EQ(heap.stats().containers, 0U);
        EXPECT_EQ(heap.stats().cells, 0U);
        ++failures;
    }
    EXPECT_GT(failures, 0U);
    EXPECT_TRUE(succeeded);
}
}  // namespace
