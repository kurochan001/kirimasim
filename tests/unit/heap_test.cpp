// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/heap.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <new>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/containers.h"
#include "kirimasim/valsem/value.h"
#include "gtest/gtest.h"

namespace {

using namespace kirimasim::kernel;
namespace vs = kirimasim::valsem;

std::vector<HeapWord> integralLayout(unsigned width, bool four) {
    std::vector<HeapWord> out;
    for (unsigned at = 0; at < width; at += 64) {
        const unsigned bits = std::min(64U, width - at);
        const std::uint64_t mask = bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1;
        out.push_back({mask, four, four ? W4{mask, mask} : W4{0, 0}, kNone});
    }
    return out;
}
std::vector<W4> encoded(const vs::Value& value) {
    std::vector<std::uint64_t> a, b;
    vs::toWords(value, a, b);
    std::vector<W4> out;
    for (std::size_t i = 0; i < a.size(); ++i)
        out.push_back({a[i], b[i]});
    return out;
}
void expectWords(std::span<const W4> got, std::span<const W4> want) {
    ASSERT_EQ(got.size(), want.size());
    for (std::size_t i = 0; i < got.size(); ++i) {
        EXPECT_EQ(got[i].a, want[i].a) << i;
        EXPECT_EQ(got[i].b, want[i].b) << i;
    }
}
HeapLayoutId scalar(Heap& h) {
    const std::array<HeapWord, 1> words{{{~std::uint64_t{0}, false, {0, 0}, kNone}}};
    const auto result = h.addLayout(words);
    EXPECT_TRUE(result);
    return result.value;
}
HeapTypeId arrayType(Heap& h, HeapLayoutId l) {
    const auto result = h.addType({ContainerKind::DynamicArray, l, 0});
    EXPECT_TRUE(result);
    return result.value;
}
HeapTypeId queueType(Heap& h, HeapLayoutId l, std::uint64_t bound = 0) {
    const auto result = h.addType({ContainerKind::Queue, l, bound});
    EXPECT_TRUE(result);
    return result.value;
}
ContainerToken values(Heap& h, HeapTypeId t, std::initializer_list<std::uint64_t> items) {
    std::vector<W4> words;
    for (const auto n : items)
        words.push_back({n, 0});
    const auto result = h.fromValues(t, words);
    EXPECT_TRUE(result);
    return result.value;
}
std::uint64_t at(Heap& h, HeapTypeId t, ContainerToken c, std::int64_t index) {
    std::array<W4, 1> out{};
    EXPECT_TRUE(h.read(t, c, index, out));
    EXPECT_EQ(out[0].b, 0U);
    return out[0].a;
}
HeapStatus put(Heap& h, HeapTypeId t, ContainerToken& c, std::int64_t index, std::uint64_t n) {
    const std::array<W4, 1> words{{{n, 0}}};
    return h.write(t, c, index, words);
}

class FailingMemory final : public std::pmr::memory_resource {
public:
    void arm(std::size_t after) { remaining_ = after; }
    void disarm() { remaining_.reset(); }

private:
    std::optional<std::size_t> remaining_;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining_) {
            if (*remaining_ == 0) throw std::bad_alloc{};
            --*remaining_;
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

TEST(Heap, DescriptorAndHandleValidation) {
    Heap h;
    EXPECT_EQ(h.addLayout({}).status.error, HeapError::InvalidType);
    std::array<HeapWord, 1> bad{{{0, false, {0, 0}, kNone}}};
    EXPECT_EQ(h.addLayout(bad).status.error, HeapError::InvalidType);
    bad[0] = {1, false, {0, 1}, kNone};
    EXPECT_EQ(h.addLayout(bad).status.error, HeapError::InvalidType);
    bad[0] = {1, true, {2, 0}, kNone};
    EXPECT_EQ(h.addLayout(bad).status.error, HeapError::InvalidType);
    bad[0] = {~std::uint64_t{0}, false, {0, 0}, HeapTypeId{10}};
    EXPECT_EQ(h.addLayout(bad).status.error, HeapError::InvalidType);
    const HeapLayoutId l = scalar(h);
    EXPECT_EQ(h.addType({ContainerKind::DynamicArray, l, 4}).status.error, HeapError::InvalidType);
    EXPECT_EQ(h.addType({ContainerKind::Queue, HeapLayoutId{20}, 0}).status.error,
              HeapError::InvalidType);
    const HeapTypeId a = arrayType(h, l), q = queueType(h, l);
    EXPECT_EQ(h.construct(q, 3).status.error, HeapError::InvalidType);
    EXPECT_EQ(h.construct(a, -1).status.error, HeapError::InvalidSize);
    EXPECT_EQ(h.construct(a, std::numeric_limits<std::int64_t>::max()).status.error,
              HeapError::ResourceLimit);
    EXPECT_EQ(h.construct(HeapTypeId{100}, 1).status.error, HeapError::InvalidType);
    ContainerToken c = values(h, a, {1, 2});
    EXPECT_EQ(h.size(q, c).status.error, HeapError::InvalidHandle);
    EXPECT_EQ(h.size(a, ContainerToken{1}).status.error, HeapError::InvalidHandle);
    EXPECT_EQ(h.insert(a, c, 0, std::array<W4, 1>{{{5, 0}}}).error, HeapError::InvalidType);
    EXPECT_EQ(h.words(HeapLayoutId{100}), 0U);
    EXPECT_EQ(h.stats().cells, 2U);
}

TEST(Heap, IntegralOperationsAndStableReferencesMatchIndependentModel) {
    for (const unsigned width : {1U, 7U, 32U, 64U, 65U, 129U, 257U}) {
        for (const bool four : {false, true}) {
            for (const unsigned capacity : {0U, 1U, 4U, 17U}) {
                SCOPED_TRACE(::testing::Message() << width << '/' << four << '/' << capacity);
                Heap h;
                const auto l = h.addLayout(integralLayout(width, four));
                ASSERT_TRUE(l);
                const HeapTypeId a = arrayType(h, l.value);
                const HeapTypeId q = queueType(h, l.value, capacity);
                const auto initial =
                    vs::filled(four ? vs::Logic::X : vs::Logic::Zero, width, false);
                vs::ContainerValue am(initial);
                vs::ContainerValue qm(initial, capacity ? std::optional<std::size_t>{capacity}
                                                        : std::nullopt);
                ContainerToken av, qv;
                struct Ref {
                    CellToken actual;
                    std::uint64_t model;
                    bool queue;
                };
                std::vector<Ref> refs;
                std::mt19937_64 random(5091);
                std::vector<W4> out((width + 63) / 64);
                for (unsigned round = 0; round < 450; ++round) {
                    std::vector<std::uint64_t> aw(out.size()), bw(out.size());
                    for (std::size_t k = 0; k < aw.size(); ++k) {
                        aw[k] = random();
                        bw[k] = random();
                    }
                    auto v = vs::fromWords(aw, bw, width, false);
                    if (!four) v = vs::to2(v);
                    const auto data = encoded(v);
                    const bool isQueue = (random() & 1U) != 0;
                    auto& model = isQueue ? qm : am;
                    auto& container = isQueue ? qv : av;
                    const HeapTypeId type = isQueue ? q : a;
                    const std::int64_t i =
                        static_cast<std::int64_t>(random() % (model.size() + 5)) - 2;
                    const auto index =
                        random() % 9 == 0 ? std::nullopt : std::optional<std::int64_t>{i};
                    switch (random() % 10) {
                        case 0: {
                            const auto n = static_cast<std::int64_t>(random() % 12);
                            const bool keep = (random() & 1U) != 0;
                            const auto made = h.construct(a, n, keep ? av : ContainerToken{});
                            ASSERT_TRUE(made);
                            av = made.value;
                            am.resize(static_cast<std::size_t>(n), keep);
                            break;
                        }
                        case 1:
                            ASSERT_TRUE(h.write(type, container, index, data));
                            model.write(index, v, isQueue);
                            break;
                        case 2: {
                            const auto ref = h.cell(type, container, index);
                            ASSERT_TRUE(ref);
                            refs.push_back({ref.value, model.reference(index), isQueue});
                            break;
                        }
                        case 3:
                            if (!refs.empty()) {
                                const Ref r = refs[random() % refs.size()];
                                ASSERT_TRUE(h.writeCell(l.value, r.actual, data));
                                (r.queue ? qm : am).writeReference(r.model, v);
                            }
                            break;
                        case 4:
                            ASSERT_TRUE(h.insert(q, qv, index, data));
                            (void)qm.insert(index, v);
                            break;
                        case 5:
                            ASSERT_TRUE(h.erase(q, qv, index));
                            (void)qm.erase(index);
                            break;
                        case 6: {
                            const bool front = (random() & 1U) != 0;
                            ASSERT_TRUE(h.pop(q, qv, front, out));
                            expectWords(out, encoded(qm.pop(front)));
                            break;
                        }
                        case 7: {
                            const auto copied = h.copy(type, container);
                            ASSERT_TRUE(copied);
                            container = copied.value;
                            model.replace(model.values());
                            break;
                        }
                        case 8:
                            ASSERT_TRUE(h.clear(type, container));
                            model.clear();
                            break;
                        default: {
                            const auto at = static_cast<std::int64_t>(qm.size());
                            ASSERT_TRUE(h.insert(q, qv, at, data));
                            (void)qm.insert(at, v);
                            break;
                        }
                    }
                    // Collect with each kind of live root after every operation.
                    std::vector<HeapRoot> roots{HeapRoot::container(av), HeapRoot::container(qv)};
                    for (const Ref& r : refs)
                        roots.push_back(HeapRoot::cell(r.actual));
                    ASSERT_TRUE(h.collect(roots));
                    for (const bool queue : {false, true}) {
                        auto& m = queue ? qm : am;
                        const auto c = queue ? qv : av;
                        const auto t = queue ? q : a;
                        const auto size = h.size(t, c);
                        ASSERT_TRUE(size);
                        ASSERT_EQ(size.value, m.size());
                        for (std::int64_t at = -1; at <= static_cast<std::int64_t>(m.size());
                             ++at) {
                            ASSERT_TRUE(h.read(t, c, at, out));
                            expectWords(out, encoded(m.read(at)));
                        }
                        ASSERT_TRUE(h.read(t, c, std::nullopt, out));
                        expectWords(out, encoded(initial));
                    }
                    for (const Ref& r : refs) {
                        ASSERT_TRUE(h.readCell(l.value, r.actual, out));
                        expectWords(out, encoded((r.queue ? qm : am).readReference(r.model)));
                    }
                }
                ASSERT_TRUE(h.collect({}));
                EXPECT_EQ(h.stats().cells, 0U);
                EXPECT_EQ(h.stats().containers, 0U);
            }
        }
    }
}

TEST(Heap, SameSizeResizeAndSelfCopyDetachReferences) {
    Heap h;
    const HeapLayoutId l = scalar(h);
    const HeapTypeId a = arrayType(h, l);
    ContainerToken v = values(h, a, {11, 22});
    const CellToken before = h.cell(a, v, 1).value;
    const auto resized = h.construct(a, 2, v);
    ASSERT_TRUE(resized);
    v = resized.value;
    EXPECT_NE(h.cell(a, v, 1).value, before);
    const std::array<W4, 1> data{{{99, 0}}};
    ASSERT_TRUE(h.writeCell(l, before, data));
    EXPECT_EQ(at(h, a, v, 1), 22U);
    const CellToken old = h.cell(a, v, 0).value;
    const auto copied = h.copy(a, v);
    ASSERT_TRUE(copied);
    v = copied.value;
    ASSERT_TRUE(h.writeCell(l, old, data));
    EXPECT_EQ(at(h, a, v, 0), 11U);
    const std::array<HeapRoot, 3> roots{
        {HeapRoot::container(v), HeapRoot::cell(old), HeapRoot::cell(before)}};
    ASSERT_TRUE(h.collect(roots));
    std::array<W4, 1> out{};
    ASSERT_TRUE(h.readCell(l, before, out));
    EXPECT_EQ(out[0].a, 99U);
    ASSERT_TRUE(h.clear(a, v));
    ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::cell(before)}}));
    ASSERT_TRUE(h.readCell(l, before, out));
    EXPECT_EQ(out[0].a, 99U);
    EXPECT_EQ(h.stats().cells, 1U);
    EXPECT_EQ(h.stats().containers, 0U);
}

TEST(Heap, BoundedQueueMovementAndDiscardKeepRetainedCellIdentity) {
    Heap h;
    const auto l = scalar(h);
    const auto q = queueType(h, l, 3);
    ContainerToken v = values(h, q, {10, 20, 30});
    const CellToken middle = h.cell(q, v, 1).value, tail = h.cell(q, v, 2).value;
    const std::array<W4, 1> data{{{7, 0}}};
    const auto status = h.push(q, v, true, data);
    ASSERT_TRUE(status);
    EXPECT_EQ(status.warning, HeapWarning::BoundExceeded);
    EXPECT_EQ(h.cell(q, v, 2).value, middle);
    EXPECT_EQ(at(h, q, v, 0), 7U);
    EXPECT_EQ(h.size(q, v).value, 3U);
    ASSERT_TRUE(h.collect(std::array<HeapRoot, 2>{{HeapRoot::container(v), HeapRoot::cell(tail)}}));
    std::array<W4, 1> out{};
    ASSERT_TRUE(h.readCell(l, tail, out));
    EXPECT_EQ(out[0].a, 30U);
    ASSERT_TRUE(h.writeCell(l, tail, data));
    EXPECT_EQ(at(h, q, v, 2), 20U);
    EXPECT_EQ(h.push(q, v, false, data).warning, HeapWarning::BoundExceeded);
    EXPECT_EQ(h.write(q, v, 3, data).warning, HeapWarning::BoundExceeded);
    EXPECT_EQ(h.size(q, v).value, 3U);
    ASSERT_TRUE(h.erase(q, v, 0));
    EXPECT_EQ(h.cell(q, v, 1).value, middle);
    ASSERT_TRUE(h.pop(q, v, true, out));
    EXPECT_EQ(out[0].a, 10U);
    EXPECT_EQ(h.cell(q, v, 0).value, middle);
    ASSERT_TRUE(h.pop(q, v, false, out));
    EXPECT_EQ(out[0].a, 20U);
    EXPECT_EQ(h.pop(q, v, false, out).warning, HeapWarning::EmptyPop);
    EXPECT_EQ(out[0].a, 0U);
    EXPECT_EQ(h.insert(q, v, std::nullopt, data).warning, HeapWarning::InvalidIndex);
    EXPECT_EQ(h.erase(q, v, -1).warning, HeapWarning::InvalidIndex);
    ASSERT_TRUE(h.write(q, v, 0, data));
    EXPECT_EQ(h.size(q, v).value, 1U);
}

TEST(Heap, CopyUsesDestinationBoundsAndIndependentCells) {
    Heap h;
    const auto l = scalar(h);
    const auto a = arrayType(h, l), q = queueType(h, l, 2);
    ContainerToken original = values(h, a, {8, 9, 10});
    const auto bounded = h.copy(q, original);
    ASSERT_TRUE(bounded);
    EXPECT_EQ(bounded.status.warning, HeapWarning::BoundExceeded);
    ContainerToken v = bounded.value;
    EXPECT_EQ(h.size(q, v).value, 2U);
    ASSERT_TRUE(put(h, a, original, 0, 17));
    EXPECT_EQ(at(h, q, v, 0), 8U);
    ASSERT_TRUE(put(h, q, v, 1, 27));
    EXPECT_EQ(at(h, a, original, 1), 9U);
    const auto fromBounded = h.construct(a, 4, v);
    ASSERT_TRUE(fromBounded);
    EXPECT_EQ(at(h, a, fromBounded.value, 1), 27U);
    EXPECT_EQ(at(h, a, fromBounded.value, 3), 0U);
    EXPECT_EQ(h.copy(a, {}).value.bits, 0U);
    EXPECT_EQ(h.construct(a, 0, original).value.bits, 0U);
}

struct Nested {
    HeapLayoutId leaf, outer;
    HeapTypeId innerType, outerType;
    ContainerToken inner, value;
};
Nested nested(Heap& h) {
    Nested n;
    n.leaf = scalar(h);
    n.innerType = queueType(h, n.leaf);
    const std::array<HeapWord, 1> desc{{{~std::uint64_t{0}, false, {0, 0}, n.innerType}}};
    const auto layout = h.addLayout(desc);
    EXPECT_TRUE(layout);
    n.outer = layout.value;
    n.outerType = queueType(h, n.outer);
    n.inner = values(h, n.innerType, {3, 5, 7});
    const std::array<W4, 2> words{{{n.inner.bits, 0}, {n.inner.bits, 0}}};
    const auto out = h.fromValues(n.outerType, words);
    EXPECT_TRUE(out);
    n.value = out.value;
    return n;
}
TEST(Heap, NestedCopiesAndPopsDoNotShareMutableContainers) {
    Heap h;
    auto n = nested(h);
    ContainerToken first{at(h, n.outerType, n.value, 0)}, second{at(h, n.outerType, n.value, 1)};
    ASSERT_NE(first, second);
    ASSERT_NE(first, n.inner);
    ASSERT_TRUE(put(h, n.innerType, n.inner, 0, 90));
    EXPECT_EQ(at(h, n.innerType, first, 0), 3U);
    const CellToken old = h.cell(n.outerType, n.value, 0).value;
    std::array<W4, 1> out{};
    ASSERT_TRUE(h.pop(n.outerType, n.value, true, out));
    ContainerToken popped{out[0].a};
    ASSERT_NE(popped, first);
    ASSERT_TRUE(put(h, n.innerType, first, 1, 91));
    EXPECT_EQ(at(h, n.innerType, popped, 1), 5U);
    const std::array<HeapRoot, 3> roots{
        {HeapRoot::container(n.value), HeapRoot::container(popped), HeapRoot::cell(old)}};
    ASSERT_TRUE(h.collect(roots));
    EXPECT_EQ(at(h, n.innerType, first, 1), 91U);
    EXPECT_EQ(at(h, n.innerType, second, 1), 5U);
    EXPECT_EQ(h.stats().containers, 4U);
    const auto copy = h.copy(n.outerType, n.value);
    ASSERT_TRUE(copy);
    ContainerToken copiedChild{at(h, n.outerType, copy.value, 0)};
    ASSERT_NE(copiedChild, second);
    ASSERT_TRUE(put(h, n.innerType, second, 2, 92));
    EXPECT_EQ(at(h, n.innerType, copiedChild, 2), 7U);
}

TEST(Heap, MixedElementLayoutDefaultsAndNormalization) {
    Heap h;
    const auto l = scalar(h);
    const auto inner = arrayType(h, l);
    const std::array<HeapWord, 6> layout{
        {{0xff, true, {0xff, 0xff}, kNone},
         {7, false, {0, 0}, kNone},
         {~std::uint64_t{0}, false, {0, 0}, kNone},  // real bits
         {0xffffffffU, false, {0, 0}, kNone},        // shortreal bits
         {~std::uint64_t{0}, false, {0, 0}, kNone},  // immutable string handle
         {~std::uint64_t{0}, false, {0, 0}, inner}}};
    const auto aggregate = h.addLayout(layout);
    ASSERT_TRUE(aggregate);
    const auto type = arrayType(h, aggregate.value);
    const auto built = h.construct(type, 2);
    ASSERT_TRUE(built);
    ContainerToken value = built.value;
    std::array<W4, 6> out{};
    ASSERT_TRUE(h.read(type, value, 0, out));
    EXPECT_EQ(out[0].a, 255U);
    EXPECT_EQ(out[0].b, 255U);
    for (std::size_t k = 1; k < out.size(); ++k)
        EXPECT_EQ(out[k].a | out[k].b, 0U);
    const auto child = values(h, inner, {55});
    const std::array<W4, 6> data{{{0x1234, 0x4321},
                                  {0xff, 2},
                                  {std::bit_cast<std::uint64_t>(-1.25), 0},
                                  {std::bit_cast<std::uint32_t>(2.5F), 0},
                                  {42, 0},
                                  {child.bits, 0}}};
    ASSERT_TRUE(h.write(type, value, 0, data));
    ASSERT_TRUE(h.read(type, value, 0, out));
    EXPECT_EQ(out[0].a, 0x34U);
    EXPECT_EQ(out[0].b, 0x21U);
    EXPECT_EQ(out[1].a, 5U);
    EXPECT_EQ(out[1].b, 0U);
    EXPECT_EQ(out[2].a, data[2].a);
    EXPECT_EQ(out[3].a, data[3].a);
    EXPECT_EQ(out[4].a, 42U);
    EXPECT_NE(out[5].a, child.bits);
    EXPECT_EQ(at(h, inner, ContainerToken{out[5].a}, 0), 55U);
    const auto roots = std::array<HeapRoot, 1>{{HeapRoot::container(value)}};
    ASSERT_TRUE(h.collect(roots));
    EXPECT_EQ(h.stats().containers, 2U);
    auto invalid = data;
    invalid[5] = {child.bits, 0};  // source was collected
    EXPECT_EQ(h.write(type, value, 0, invalid).error, HeapError::InvalidHandle);
    invalid[5] = {0, 1};
    EXPECT_EQ(h.write(type, value, 0, invalid).error, HeapError::InvalidHandle);
    std::array<W4, 6> after{};
    ASSERT_TRUE(h.read(type, value, 0, after));
    expectWords(after, out);
}

TEST(Heap, DeepNestedCopyAndTracingUseNoNativeRecursion) {
    Heap h;
    HeapLayoutId l = scalar(h);
    HeapTypeId t = arrayType(h, l);
    ContainerToken value = values(h, t, {91});
    constexpr unsigned depth = 2500;
    for (unsigned i = 0; i < depth; ++i) {
        const std::array<HeapWord, 1> desc{{{~std::uint64_t{0}, false, {0, 0}, t}}};
        const auto nextLayout = h.addLayout(desc);
        ASSERT_TRUE(nextLayout);
        l = nextLayout.value;
        t = arrayType(h, l);
        const std::array<W4, 1> words{{{value.bits, 0}}};
        const auto next = h.fromValues(t, words);
        ASSERT_TRUE(next);
        value = next.value;
        ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(value)}}));
    }
    const auto copied = h.copy(t, value);
    ASSERT_TRUE(copied);
    ASSERT_NE(copied.value, value);
    ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(copied.value)}}));
    EXPECT_EQ(h.stats().containers, depth + 1U);
    EXPECT_EQ(h.stats().cells, depth + 1U);
    ASSERT_TRUE(h.collect({}));
    EXPECT_EQ(h.stats().cells, 0U);
}

TEST(Heap, AllocationFailureRollsBackEveryConstructionAndCopyStage) {
    for (const bool copy : {false, true}) {
        bool completed = false;
        for (std::size_t point = 0; point < 300; ++point) {
            SCOPED_TRACE(::testing::Message() << copy << '/' << point);
            FailingMemory memory;
            Heap h(&memory);
            const auto n = nested(h);
            const auto dynamic = arrayType(h, n.leaf);
            const auto before = h.stats();
            memory.arm(point);
            const auto made =
                copy ? h.copy(n.outerType, n.value) : h.construct(dynamic, 11, n.inner);
            memory.disarm();
            if (made) {
                completed = true;
                EXPECT_GT(point, 10U);
                break;
            }
            EXPECT_EQ(made.status.error, HeapError::ResourceLimit);
            EXPECT_EQ(h.stats().containers, before.containers);
            EXPECT_EQ(h.stats().cells, before.cells);
            EXPECT_EQ(at(h, n.innerType, n.inner, 1), 5U);
            ASSERT_TRUE(h.collect(std::array<HeapRoot, 2>{
                {HeapRoot::container(n.value), HeapRoot::container(n.inner)}}));
        }
        EXPECT_TRUE(completed);
    }
}

TEST(Heap, AllocationFailureLeavesMutationAndOutputUnchanged) {
    // Each operation copies nested containers and can fail at several pool,
    // payload and transaction allocations, including after partial cloning.
    for (unsigned operation = 0; operation < 5; ++operation) {
        bool completed = false;
        for (std::size_t point = 0; point < 300; ++point) {
            SCOPED_TRACE(::testing::Message() << operation << '/' << point);
            FailingMemory memory;
            Heap h(&memory);
            auto n = nested(h);
            const auto before = h.stats();
            const auto saved = n.value;
            const auto first = h.cell(n.outerType, n.value, 0).value;
            const auto oldChild = at(h, n.outerType, n.value, 0);
            const std::array<W4, 1> input{{{n.inner.bits, 0}}};
            std::array<W4, 1> output{{{123456, 654321}}};
            memory.arm(point);
            HeapStatus status;
            switch (operation) {
                case 0: status = h.insert(n.outerType, n.value, 1, input); break;
                case 1: status = h.writeCell(n.outer, first, input); break;
                case 2: status = h.pop(n.outerType, n.value, true, output); break;
                case 3: status = h.fromValues(n.outerType, input).status; break;
                default: {
                    ContainerToken empty;
                    status = h.push(n.outerType, empty, false, input);
                    if (!status) EXPECT_EQ(empty.bits, 0U);
                }
            }
            memory.disarm();
            if (status) {
                completed = true;
                EXPECT_GT(point, 5U);
                break;
            }
            EXPECT_EQ(status.error, HeapError::ResourceLimit);
            EXPECT_EQ(n.value, saved);
            EXPECT_EQ(h.size(n.outerType, n.value).value, 2U);
            EXPECT_EQ(at(h, n.outerType, n.value, 0), oldChild);
            EXPECT_EQ(h.cell(n.outerType, n.value, 0).value, first);
            EXPECT_EQ(h.stats().containers, before.containers);
            EXPECT_EQ(h.stats().cells, before.cells);
            EXPECT_EQ(output[0].a, 123456U);
            EXPECT_EQ(output[0].b, 654321U);
        }
        EXPECT_TRUE(completed);
    }
}

TEST(Heap, FailedMarkingNeverSweepsAndNextCollectionStartsFresh) {
    bool completed = false;
    for (std::size_t point = 0; point < 50; ++point) {
        FailingMemory memory;
        Heap h(&memory);
        const auto n = nested(h);
        const auto garbage = values(h, n.innerType, {19, 23, 29});
        const auto before = h.stats();
        memory.arm(point);
        const auto status = h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(n.value)}});
        memory.disarm();
        if (status) {
            completed = true;
            EXPECT_GT(point, 1U);
            break;
        }
        EXPECT_EQ(status.error, HeapError::ResourceLimit);
        EXPECT_EQ(h.stats().containers, before.containers);
        EXPECT_EQ(h.stats().cells, before.cells);
        EXPECT_EQ(at(h, n.innerType, garbage, 2), 29U);
        ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(n.value)}}));
        EXPECT_EQ(h.stats().containers, 3U);
        EXPECT_EQ(h.stats().cells, 8U);
    }
    EXPECT_TRUE(completed);
}

TEST(Heap, InvalidRootDoesNotPartiallyCollect) {
    Heap h;
    const auto l = scalar(h);
    const auto a = arrayType(h, l);
    const auto live = values(h, a, {17, 31});
    const auto garbage = values(h, a, {99});
    const std::array<HeapRoot, 2> roots{
        {HeapRoot::container(ContainerToken{1}), HeapRoot::container(live)}};
    EXPECT_EQ(h.collect(roots).error, HeapError::InvalidHandle);
    EXPECT_EQ(at(h, a, garbage, 0), 99U);
    EXPECT_EQ(h.stats().cells, 3U);
    ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(live)}}));
    EXPECT_EQ(h.size(a, garbage).status.error, HeapError::InvalidHandle);
}

TEST(Heap, ReclaimedTablesAreReusedWithoutRevivingStaleTokens) {
    Heap h;
    const auto l = scalar(h);
    const auto a = arrayType(h, l);
    ContainerToken stale;
    CellToken staleCell;
    for (unsigned i = 0; i < 4096; ++i) {
        const auto value = values(h, a, {i, i + 1U});
        const auto cell = h.cell(a, value, 0).value;
        EXPECT_NE(value, stale);
        EXPECT_NE(cell, staleCell);
        if (i != 0) {
            EXPECT_EQ(h.size(a, stale).status.error, HeapError::InvalidHandle);
            std::array<W4, 1> out{};
            EXPECT_EQ(h.readCell(l, staleCell, out).error, HeapError::InvalidHandle);
        }
        stale = value;
        staleCell = cell;
        ASSERT_TRUE(h.collect({}));
    }
    EXPECT_EQ(h.stats().containerEntries, 1U);
    EXPECT_EQ(h.stats().cellEntries, 2U);
}

TEST(Heap, ContextsOwnIndependentStorageAndCollectors) {
    Context one(OutputDestinations{nullptr, nullptr}), two(OutputDestinations{nullptr, nullptr});
    const auto l1 = scalar(one.heap()), l2 = scalar(two.heap());
    const auto a1 = arrayType(one.heap(), l1), a2 = arrayType(two.heap(), l2);
    auto v1 = values(one.heap(), a1, {10}), v2 = values(two.heap(), a2, {20});
    ASSERT_TRUE(put(one.heap(), a1, v1, 0, 30));
    EXPECT_EQ(at(two.heap(), a2, v2, 0), 20U);
    ASSERT_TRUE(one.heap().collect({}));
    EXPECT_EQ(two.heap().stats().cells, 1U);
    EXPECT_EQ(at(two.heap(), a2, v2, 0), 20U);
}

}  // namespace
