// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "frame_value.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
using kirimasim::vm::detail::FrameValue;

void equalWords(std::span<const k::W4> actual, std::span<const k::W4> expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual[i].a, expected[i].a) << i;
        EXPECT_EQ(actual[i].b, expected[i].b) << i;
    }
}

std::vector<k::W4> pattern(std::uint32_t width, std::uint64_t seed) {
    std::vector<k::W4> value((width + 63) / 64);
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        const auto state = (seed + std::uint64_t{bit} * 3 + bit / 7) % 4;
        const auto mask = std::uint64_t{1} << (bit % 64);
        if ((state & 1U) != 0) value[bit / 64].a |= mask;
        if ((state & 2U) != 0) value[bit / 64].b |= mask;
    }
    return value;
}

/// Independent bit-by-bit transport model, with no wide helper calls.
void put(std::vector<k::W4>& out, std::uint32_t lsb, std::uint32_t width,
         const std::vector<k::W4>& value) {
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        const auto from = std::uint64_t{1} << (bit % 64);
        const auto to = std::uint64_t{1} << ((lsb + bit) % 64);
        auto& word = out[(lsb + bit) / 64];
        word.a = (word.a & ~to) | ((value[bit / 64].a & from) != 0 ? to : 0);
        word.b = (word.b & ~to) | ((value[bit / 64].b & from) != 0 ? to : 0);
    }
}

TEST(VmFrameValue, OrdinaryAndScopedSelectionsMatchAnIndependentBitModel) {
    unsigned cases = 0;
    for (const std::uint32_t width : {1U, 7U, 32U, 63U, 64U, 65U, 127U, 128U, 129U, 257U}) {
        k::Context context;
        auto& store = context.activations();
        const auto layout = store.addLayout(
            std::array{k::ActivationField{k::ActivationKind::Four, width, {{-1, 1}}}});
        ASSERT_TRUE(layout);
        auto owner = store.create(layout.value);
        ASSERT_TRUE(owner);
        const auto binding = store.bind(owner.value, 0);
        ASSERT_TRUE(binding);
        for (const auto element : {0U, 1U, 2U}) {
            SCOPED_TRACE(width);
            SCOPED_TRACE(element);
            auto expected = pattern(width, element);
            auto words = expected;
            const FrameValue ordinary{words, width};
            const FrameValue scoped{store, context.scheduler(), binding.value, element, width};
            ASSERT_EQ(scoped.write(expected), k::ActivationError::None);
            for (const auto lsb : {0U, width / 2, width - 1}) {
                for (const auto n : {1U, std::min(65U, width - lsb), width - lsb}) {
                    const auto value = pattern(n, lsb + n);
                    put(expected, lsb, n, value);
                    ASSERT_EQ(ordinary.write(value, lsb, n), k::ActivationError::None);
                    ASSERT_EQ(scoped.write(value, lsb, n), k::ActivationError::None);
                    std::vector<k::W4> read(words.size());
                    ASSERT_EQ(scoped.read(read), k::ActivationError::None);
                    equalWords(read, expected);
                    equalWords(words, expected);
                    std::vector<k::W4> actual((n + 63) / 64);
                    ASSERT_EQ(ordinary.read(actual, lsb, n), k::ActivationError::None);
                    equalWords(actual, value);
                    ASSERT_EQ(scoped.read(actual, lsb, n), k::ActivationError::None);
                    equalWords(actual, value);
                    ++cases;
                }
            }
        }
    }
    EXPECT_EQ(cases, 270U);
}

TEST(VmFrameValue, BorrowedWordsRetainReferencePayloadAndGuardWords) {
    std::array<k::W4, 3> words{{{71, 19}, {0, 0}, {91, 29}}};
    const FrameValue view{{words.data() + 1, 1}, 64};
    const k::W4 reference{0xfedcba9876543210ULL, 0x8123456789abcdefULL};
    ASSERT_EQ(view.write({&reference, 1}), k::ActivationError::None);
    k::W4 actual{};
    ASSERT_EQ(view.read({&actual, 1}), k::ActivationError::None);
    EXPECT_EQ(actual.a, reference.a);
    EXPECT_EQ(actual.b, reference.b);
    EXPECT_EQ(words[0].a, 71U);
    EXPECT_EQ(words[0].b, 19U);
    EXPECT_EQ(words[2].a, 91U);
    EXPECT_EQ(words[2].b, 29U);
}

TEST(VmFrameValue, InvalidRangesBuffersAndReadonlyAccessAreNonmutating) {
    k::Context context;
    auto& store = context.activations();
    const auto layout =
        store.addLayout(std::array{k::ActivationField{k::ActivationKind::Four, 129, {}}});
    ASSERT_TRUE(layout);
    auto owner = store.create(layout.value);
    ASSERT_TRUE(owner);
    const auto binding = store.bind(owner.value, 0);
    ASSERT_TRUE(binding);
    auto words = pattern(129, 2);
    const FrameValue ordinary{words, 129};
    const FrameValue scoped{store, context.scheduler(), binding.value, 0, 129};
    ASSERT_EQ(scoped.write(words), k::ActivationError::None);
    const auto original = words;
    const auto data = pattern(129, 1);
    for (const FrameValue& view : {ordinary, scoped}) {
        EXPECT_EQ(view.write(data, 129, 1), k::ActivationError::OutOfRange);
        EXPECT_EQ(view.write(data, 128, 2), k::ActivationError::OutOfRange);
        EXPECT_EQ(view.write(data, 1, 0), k::ActivationError::OutOfRange);
        EXPECT_EQ(view.write({data.data(), 2}), k::ActivationError::OutOfRange);
        std::array<k::W4, 2> small{{{7, 9}, {11, 13}}};
        EXPECT_EQ(view.read(small), k::ActivationError::OutOfRange);
        EXPECT_EQ(small[0].a, 7U);
        EXPECT_EQ(small[1].b, 13U);
    }
    const FrameValue missing{{}, 0xffffffffU};
    EXPECT_EQ(missing.read({}), k::ActivationError::OutOfRange);
    EXPECT_EQ(missing.write({}), k::ActivationError::OutOfRange);
    const auto readonly = store.readOnly(binding.value);
    ASSERT_TRUE(readonly);
    EXPECT_EQ((FrameValue{words, 129, true}.write(data)), k::ActivationError::ReadOnly);
    EXPECT_EQ((FrameValue{store, context.scheduler(), readonly.value, 0, 129}.write(data)),
              k::ActivationError::ReadOnly);
    equalWords(words, original);
    std::vector<k::W4> after(3);
    ASSERT_EQ(scoped.read(after), k::ActivationError::None);
    equalWords(after, original);
}

struct Observer {
    FrameValue view;
    unsigned evals = 0, resumes = 0;
    bool valid = true;
    std::array<k::W4, 3> last{};
    static void eval(kirima_context*, void* frame, std::uint64_t* out) {
        auto& self = *static_cast<Observer*>(frame);
        ++self.evals;
        self.valid = self.view.read(self.last) == k::ActivationError::None && self.valid;
        for (std::size_t i = 0; i < self.last.size(); ++i) {
            out[2 * i] = self.last[i].a;
            out[2 * i + 1] = self.last[i].b;
        }
    }
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<Observer*>(frame)->resumes;
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(VmFrameValue, WideCommitNotifiesWithTheCompleteValueBeforeReturning) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context context;
        auto& store = context.activations();
        const auto layout =
            store.addLayout(std::array{k::ActivationField{k::ActivationKind::Four, 129, {}}});
        ASSERT_TRUE(layout);
        auto owner = store.create(layout.value);
        ASSERT_TRUE(owner);
        const auto binding = store.bind(owner.value, 0);
        ASSERT_TRUE(binding);
        Observer observer{{store, context.scheduler(), binding.value, 0, 129}};
        const auto process = context.scheduler().addProcess(&Observer::resume, &observer, group);
        k::MonitorSpec spec;
        spec.eval = &Observer::eval;
        spec.frame = &observer;
        spec.width = 129;
        spec.activation.bindings = {binding.value};
        spec.activation.capture = true;
        context.scheduler().waitAny(process, {}, {&spec, 1});
        ASSERT_EQ(observer.evals, 1U);
        const auto value = pattern(129, 3);
        ASSERT_EQ(observer.view.write(value), k::ActivationError::None);
        EXPECT_EQ(observer.evals, 2U);
        EXPECT_TRUE(observer.valid);
        equalWords(observer.last, value);
        EXPECT_EQ(observer.view.write(value), k::ActivationError::None);
        EXPECT_EQ(observer.evals, 2U);
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(observer.resumes, 1U);
    }
}

TEST(VmFrameValue, ReadAndWriteFootprintsContainOnlyTheRequestedSelection) {
    k::Context context;
    auto& store = context.activations();
    const auto layout =
        store.addLayout(std::array{k::ActivationField{k::ActivationKind::Four, 129, {}}});
    ASSERT_TRUE(layout);
    auto owner = store.create(layout.value);
    ASSERT_TRUE(owner);
    const auto binding = store.bind(owner.value, 0);
    ASSERT_TRUE(binding);
    struct Accesses {
        unsigned reads = 0, writes = 0;
        k::ActivationAccess lastRead{}, lastWrite{};
    } accesses;
    store.setReadHook({[](void* frame, const k::ActivationAccess& access) noexcept {
                           auto& self = *static_cast<Accesses*>(frame);
                           ++self.reads;
                           self.lastRead = access;
                       },
                       &accesses});
    store.setChangeHook({[](void* frame, const k::ActivationAccess& access) noexcept {
                             auto& self = *static_cast<Accesses*>(frame);
                             ++self.writes;
                             self.lastWrite = access;
                         },
                         &accesses});
    const FrameValue view{store, context.scheduler(), binding.value, 0, 129};
    const std::array<k::W4, 2> part{{{1, 1}, {1, 0}}};
    ASSERT_EQ(view.write(part, 63, 65), k::ActivationError::None);
    EXPECT_EQ(accesses.reads, 0U);
    EXPECT_EQ(accesses.writes, 1U);
    EXPECT_EQ(accesses.lastWrite.lsb, 63U);
    EXPECT_EQ(accesses.lastWrite.width, 65U);
    std::array<k::W4, 2> actual{};
    ASSERT_EQ(view.read(actual, 63, 65), k::ActivationError::None);
    equalWords(actual, part);
    EXPECT_EQ(accesses.reads, 1U);
    EXPECT_EQ(accesses.lastRead.lsb, 63U);
    EXPECT_EQ(accesses.lastRead.width, 65U);
    store.setReadHook({});
    store.setChangeHook({});
}

TEST(VmFrameValue, ViewsDoNotOwnScopesAndOldViewsCannotAliasReusedStorage) {
    k::Context context;
    auto& store = context.activations();
    const auto layout =
        store.addLayout(std::array{k::ActivationField{k::ActivationKind::Two, 32, {}}});
    ASSERT_TRUE(layout);
    auto owner = store.create(layout.value);
    ASSERT_TRUE(owner);
    const auto binding = store.bind(owner.value, 0);
    ASSERT_TRUE(binding);
    const FrameValue old{store, context.scheduler(), binding.value, 0, 32};
    const k::W4 value{0xffffffffU, 0xff00U};
    EXPECT_EQ(old.write({&value, 1}), k::ActivationError::None);
    k::W4 actual{};
    ASSERT_EQ(old.read({&actual, 1}), k::ActivationError::None);
    EXPECT_EQ(actual.a, 0xffff00ffU);
    EXPECT_EQ(actual.b, 0U);
    EXPECT_EQ(store.stats().leases, 1U);
    owner.value.reset();
    auto next = store.create(layout.value);
    ASSERT_TRUE(next);
    EXPECT_EQ(store.stats().entries, 1U);
    EXPECT_EQ(old.write({&value, 1}), k::ActivationError::InvalidBinding);
    actual = {71, 19};
    EXPECT_EQ(old.read({&actual, 1}), k::ActivationError::InvalidBinding);
    EXPECT_EQ(actual.a, 71U);
    EXPECT_EQ(actual.b, 19U);
    const auto replacement = store.bind(next.value, 0);
    ASSERT_TRUE(replacement);
    ASSERT_EQ(store.read(replacement.value, 0, {&actual, 1}), k::ActivationError::None);
    EXPECT_EQ(actual.a, 0U);
    EXPECT_EQ(actual.b, 0U);
}

TEST(VmFrameValue, ExecutionUsesLogicalRowsForWideArraysReferencesAndScalarCopies) {
    namespace s = kirimasim::sir;
    unsigned cases = 0;
    for (const auto width : {7U, 32U, 64U, 65U, 129U, 257U}) {
        for (const bool four : {false, true}) {
            for (const bool ref : {false, true}) {
                SCOPED_TRACE(::testing::Message() << width << '/' << four << '/' << ref);
                const auto type = four ? s::u4(width) : s::u2(width);
                const auto partWidth = std::min(65U, width);
                const auto partType = four ? s::u4(partWidth) : s::u2(partWidth);
                auto initial = pattern(width, 2), part = pattern(partWidth, 3);
                const auto prefix = pattern(129, 1);
                if (!four) {
                    for (auto& word : initial)
                        word.b = 0;
                    for (auto& word : part)
                        word.b = 0;
                }
                s::Design design;
                design.sources = {"frame_transport.sv"};
                design.instances = {{"top", "top", s::kNone}};
                design.slots = {{"top.answer",
                                 type,
                                 s::SlotKind::Var,
                                 {},
                                 s::kObservable,
                                 s::InstanceId{0},
                                 s::kNone,
                                 s::kNone,
                                 0},
                                {"top.prefix",
                                 s::u4(129),
                                 s::SlotKind::Var,
                                 {},
                                 s::kObservable,
                                 s::InstanceId{0},
                                 s::kNone,
                                 s::kNone,
                                 0}};
                const auto constant = [&](s::Type t, const std::vector<k::W4>& value) {
                    s::Constant c;
                    c.type = t;
                    for (const auto word : value) {
                        c.aWords.push_back(word.a);
                        if (s::isFourState(t)) c.bWords.push_back(word.b);
                    }
                    const auto id = s::ConstId{static_cast<std::uint32_t>(design.constants.size())};
                    design.constants.push_back(std::move(c));
                    return s::Operand::constant(id);
                };
                const auto a = constant(type, initial), bits = constant(partType, part),
                           padding = constant(s::u4(129), prefix),
                           index = constant(s::s4(64), {{0, 0}}),
                           pos = constant(s::s4(64), {{static_cast<std::uint64_t>(-3), 0}});
                s::Process process;
                process.name = "top.initial0";
                process.writes = {{s::SlotId{0}}, {s::SlotId{1}}};
                process.numTemps = 4;
                process.frame = {{"prefix", s::u4(129)},
                                 {"array", type, s::kNone, {{-1, 1}}},
                                 {"alias", s::refType(), s::kNone, {{-1, 1}}, type},
                                 {"copy", type}};
                process.blocks.resize(1);
                auto& code = process.blocks[0].instrs;
                const auto emit = [&](s::Op op, s::Type t, s::TempId result,
                                      std::vector<s::Operand> operands) {
                    code.push_back({op, t, result, std::move(operands), {0, 1, 1}});
                };
                const auto frame = [](std::uint32_t id) {
                    return s::Operand::frame(s::FrameVarId{id});
                };
                const auto temp = [](std::uint32_t id) { return s::Operand::temp(s::TempId{id}); };
                emit(s::Op::Fstore, s::voidType(), s::kNone, {frame(0), padding});
                emit(s::Op::StoreElem, s::voidType(), s::kNone, {frame(1), index, a});
                if (ref) {
                    emit(s::Op::RefMake, s::refType(), s::TempId{0}, {frame(1)});
                    emit(s::Op::Fstore, s::voidType(), s::kNone, {frame(2), temp(0)});
                    emit(s::Op::StoreRefBits, s::voidType(), s::kNone,
                         {frame(2), index, pos, bits});
                    emit(s::Op::LoadRefElem, type, s::TempId{1}, {frame(2), index});
                } else {
                    emit(s::Op::StoreBits, s::voidType(), s::kNone, {frame(1), index, pos, bits});
                    emit(s::Op::LoadElem, type, s::TempId{1}, {frame(1), index});
                }
                emit(s::Op::Fstore, s::voidType(), s::kNone, {frame(3), temp(1)});
                emit(s::Op::Fload, type, s::TempId{2}, {frame(3)});
                emit(s::Op::Store, s::voidType(), s::kNone,
                     {s::Operand::slot(s::SlotId{0}), temp(2)});
                emit(s::Op::Fload, s::u4(129), s::TempId{3}, {frame(0)});
                emit(s::Op::Store, s::voidType(), s::kNone,
                     {s::Operand::slot(s::SlotId{1}), temp(3)});
                emit(s::Op::Ret, s::voidType(), s::kNone, {});
                design.processes.push_back(std::move(process));
                const auto errors = s::verify(design);
                ASSERT_TRUE(errors.empty()) << errors.front().message;
                k::Context context;
                const auto result = kirimasim::vm::run(design, context);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                auto expected = initial;
                for (std::uint32_t j = 3; j < partWidth && j - 3 < width; ++j) {
                    const auto from = std::uint64_t{1} << (j % 64);
                    const auto to = std::uint64_t{1} << ((j - 3) % 64);
                    auto& word = expected[(j - 3) / 64];
                    word.a = (word.a & ~to) | ((part[j / 64].a & from) != 0 ? to : 0);
                    word.b = (word.b & ~to) | ((part[j / 64].b & from) != 0 ? to : 0);
                }
                const auto* words = context.store().element(k::SlotId{0}, 0);
                for (std::size_t i = 0; i < expected.size(); ++i) {
                    EXPECT_EQ(words[i * (four ? 2U : 1U)], expected[i].a);
                    if (four) EXPECT_EQ(words[2 * i + 1], expected[i].b);
                }
                words = context.store().element(k::SlotId{1}, 0);
                for (std::size_t i = 0; i < prefix.size(); ++i) {
                    EXPECT_EQ(words[2 * i], prefix[i].a);
                    EXPECT_EQ(words[2 * i + 1], prefix[i].b);
                }
                ++cases;
            }
        }
    }
    EXPECT_EQ(cases, 24U);
}

}  // namespace
