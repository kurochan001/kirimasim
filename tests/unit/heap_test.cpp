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
#include <string>
#include <utility>
#include <vector>

#include "fuzz.h"
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
    out.reserve(a.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        out.push_back({a[i], b[i]});
    return out;
}
::testing::AssertionResult sameWords(std::span<const W4> got, std::span<const W4> want) {
    if (got.size() != want.size())
        return ::testing::AssertionFailure()
               << "word count " << got.size() << " != " << want.size();
    for (std::size_t i = 0; i < got.size(); ++i)
        if (got[i].a != want[i].a || got[i].b != want[i].b)
            return ::testing::AssertionFailure()
                   << "word " << i << ": (" << got[i].a << ',' << got[i].b << ") != (" << want[i].a
                   << ',' << want[i].b << ')';
    return ::testing::AssertionSuccess();
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

TEST(StringCollection, RetainsLiveInterningAndRejectsReusedGenerations) {
    Strings strings;
    const auto live = strings.intern("live"), dead = strings.intern("dead");
    const std::array roots{live};
    ASSERT_EQ(strings.beginCollection(roots), StringError::None);
    strings.finishCollection();
    EXPECT_EQ(strings.get(live), "live");
    EXPECT_EQ(strings.intern("live"), live);
    EXPECT_TRUE(strings.get(dead).empty());
    const auto reused = strings.intern("replacement");
    EXPECT_NE(reused, dead);
    EXPECT_EQ(static_cast<std::uint32_t>(reused), static_cast<std::uint32_t>(dead));
    EXPECT_TRUE(strings.get(dead).empty());
    EXPECT_EQ(strings.get(reused), "replacement");
}

TEST(StringCollection, NativePinsSurviveReentryAndRejectStaleRetains) {
    Strings strings;
    const auto handle = strings.intern("native owner");
    Strings::Pin pin;
    ASSERT_EQ(strings.pin(handle, pin), StringError::None);
    Strings::Pin moved(std::move(pin));
    for (unsigned i = 0; i < 1000; ++i)
        strings.intern(std::to_string(i));
    ASSERT_EQ(strings.beginCollection({}), StringError::None);
    strings.finishCollection();
    EXPECT_EQ(strings.get(moved.handle()), "native owner");
    moved.reset();
    ASSERT_EQ(strings.beginCollection({}), StringError::None);
    strings.finishCollection();
    const auto replacement = strings.intern("replacement");
    ASSERT_EQ(strings.pin(replacement, pin), StringError::None);
    EXPECT_EQ(strings.pin(handle, pin), StringError::InvalidHandle);
    EXPECT_EQ(pin.handle(), replacement);
    ASSERT_EQ(strings.beginCollection({}), StringError::None);
    strings.finishCollection();
    EXPECT_EQ(strings.get(replacement), "replacement");
}

TEST(StringCollection, ComputedMonitorLastValueRemainsCanonicalAndRetiresWithWait) {
    Context context(OutputDestinations{nullptr, nullptr});
    auto& scheduler = context.scheduler();
    const auto input = context.store().add(Domain::Two, 32, 1, Init::Zero);
    struct Expression {
        unsigned value = 0, wakes = 0;
        static void eval(kirima_context* raw, void* frame, std::uint64_t* out) {
            auto& expression = *static_cast<Expression*>(frame);
            *out = fromC(raw).strings().intern("watch=" + std::to_string(expression.value / 2));
        }
        static kirima_status resume(kirima_context*, void* frame) {
            ++static_cast<Expression*>(frame)->wakes;
            return static_cast<kirima_status>(Status::Done);
        }
    } expression;
    const auto process = scheduler.addProcess(&Expression::resume, &expression, Group::Active);
    MonitorSpec monitor;
    monitor.slots = {{input, 0, 0, 0, 0}};
    monitor.eval = &Expression::eval;
    monitor.frame = &expression;
    monitor.domain = Domain::Handle;
    monitor.width = 64;
    monitor.stringValue = true;
    scheduler.waitMonitor(process, monitor);
    const auto retained = context.strings().intern("watch=0");
    for (unsigned i = 0; i < 1000; ++i)
        context.strings().intern(std::to_string(i));
    const auto collect = [&] {
        EXPECT_EQ(context.strings().beginCollection({}), StringError::None);
        EXPECT_TRUE(scheduler.visitMonitorStrings(
            [](void* raw, std::uint64_t handle) noexcept {
                return static_cast<Strings*>(raw)->mark(handle) == StringError::None;
            },
            &context.strings()));
        context.strings().finishCollection();
    };
    collect();
    EXPECT_EQ(context.strings().get(retained), "watch=0");
    EXPECT_EQ(context.strings().stats().live, 1U);
    for (std::uint64_t value : {1U, 2U}) {
        expression.value = static_cast<unsigned>(value);
        scheduler.write({.slot = input}, &value);
        scheduler.run();
        EXPECT_EQ(expression.wakes, value == 1 ? 0U : 1U);
    }
    collect();
    EXPECT_EQ(context.strings().stats().live, 0U);
}

TEST(StringCollection, InvalidRootsAndFailedMarksNeverSweepValues) {
    Strings strings;
    const auto a = strings.intern("first"), b = strings.intern("second");
    const std::array invalid{a, std::uint64_t{1}};
    EXPECT_EQ(strings.beginCollection(invalid), StringError::InvalidHandle);
    strings.finishCollection();
    EXPECT_EQ(strings.get(a), "first");
    EXPECT_EQ(strings.get(b), "second");
    ASSERT_EQ(strings.beginCollection(std::span(&a, 1)), StringError::None);
    EXPECT_EQ(strings.mark(1), StringError::InvalidHandle);
    strings.finishCollection();
    EXPECT_EQ(strings.stats().live, 2U);
    EXPECT_EQ(strings.intern("second"), b);
}

TEST(StringCollection, DeterministicPressureBoundsDiscardedUniqueValues) {
    Strings strings;
    const auto keep = strings.intern("persistent");
    for (unsigned n = 0; n < 10000; ++n) {
        const auto current = strings.intern(std::to_string(n));
        if (strings.underPressure()) {
            const std::array roots{keep, current};
            ASSERT_EQ(strings.beginCollection(roots), StringError::None);
            strings.finishCollection();
        }
        EXPECT_EQ(strings.get(keep), "persistent");
        EXPECT_LE(strings.stats().entries, 256U);
    }
}

TEST(StringCollection, InternAllocationFailurePreservesPublishedHandles) {
    bool completed = false;
    for (unsigned budget = 0; budget < 20; ++budget) {
        FailingMemory memory;
        Strings strings(&memory);
        const auto keep = strings.intern("persistent");
        memory.arm(budget);
        try {
            const auto value = strings.intern(std::string(256, 'x'));
            EXPECT_EQ(strings.get(value), std::string(256, 'x'));
            completed = true;
        } catch (const std::bad_alloc&) {
            EXPECT_EQ(strings.stats().live, 1U);
        }
        memory.disarm();
        EXPECT_EQ(strings.get(keep), "persistent");
        EXPECT_EQ(strings.intern("persistent"), keep);
        if (completed) break;
    }
    EXPECT_TRUE(completed);
}

TEST(Heap, TypedStringLeavesAreMarkedBeforeEitherTableIsSwept) {
    Strings strings;
    Heap heap;
    HeapWord word;
    word.string = true;
    const auto layout = heap.addLayout(std::span(&word, 1));
    ASSERT_TRUE(layout);
    const auto type = heap.addType({ContainerKind::Queue, layout.value});
    ASSERT_TRUE(type);
    const auto kept = strings.intern("in a detached cell"), dead = strings.intern("garbage");
    const std::array<W4, 1> values{{{kept, 0}}};
    const auto value = heap.fromValues(type.value, values);
    ASSERT_TRUE(value);
    const auto cell = heap.cell(type.value, value.value, 0);
    ASSERT_TRUE(cell);
    const std::array roots{HeapRoot::cell(cell.value)};
    ASSERT_EQ(strings.beginCollection({}), StringError::None);
    const auto status = heap.collect(roots, {[](void* raw, std::uint64_t token) noexcept {
                                                 return static_cast<Strings*>(raw)->mark(token) ==
                                                        StringError::None;
                                             },
                                             &strings});
    ASSERT_TRUE(status);
    strings.finishCollection();
    EXPECT_EQ(heap.stats().containers, 0U);
    EXPECT_EQ(heap.stats().cells, 1U);
    EXPECT_EQ(strings.get(kept), "in a detached cell");
    EXPECT_TRUE(strings.get(dead).empty());
    EXPECT_FALSE(heap.collect(roots));  // A missing string provider must fail closed.
    EXPECT_EQ(heap.stats().cells, 1U);
}

TEST(Heap, FailedJointMarkPreservesBothContainerAndStringTables) {
    bool completed = false;
    unsigned failures = 0;
    for (unsigned budget = 0; budget < 16; ++budget) {
        FailingMemory memory;
        Heap heap(&memory);
        Strings strings;
        HeapWord word;
        word.string = true;
        const auto layout = heap.addLayout(std::span(&word, 1));
        ASSERT_TRUE(layout);
        const auto type = heap.addType({ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        const auto live = strings.intern("live"), dead = strings.intern("unrooted");
        const std::array<W4, 3> leaves{{{live, 0}, {live, 0}, {live, 0}}};
        const auto a = heap.fromValues(type.value, leaves), b = heap.fromValues(type.value, leaves);
        ASSERT_TRUE(a);
        ASSERT_TRUE(b);
        const std::array roots{HeapRoot::container(a.value)};
        ASSERT_EQ(strings.beginCollection({}), StringError::None);
        memory.arm(budget);
        const auto status = heap.collect(roots, {[](void* raw, std::uint64_t handle) noexcept {
                                                     return static_cast<Strings*>(raw)->mark(
                                                                handle) == StringError::None;
                                                 },
                                                 &strings});
        memory.disarm();
        if (status) {
            strings.finishCollection();
            EXPECT_EQ(heap.stats().containers, 1U);
            EXPECT_EQ(heap.stats().cells, 3U);
            EXPECT_TRUE(strings.get(dead).empty());
            completed = true;
        } else {
            strings.cancelCollection();
            EXPECT_EQ(status.error, HeapError::ResourceLimit);
            EXPECT_EQ(heap.stats().containers, 2U);
            EXPECT_EQ(heap.stats().cells, 6U);
            EXPECT_EQ(strings.get(dead), "unrooted");
            ++failures;
        }
        EXPECT_EQ(strings.get(live), "live");
        if (completed) break;
    }
    EXPECT_TRUE(completed);
    EXPECT_GE(failures, 2U);
}

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
    EXPECT_EQ(h.construct(a, std::nullopt).status.error, HeapError::InvalidSize);
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
                for (std::uint64_t sample = 0; sample < kirimasim::test::fuzzScale(); ++sample) {
                    SCOPED_TRACE(sample);
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
                    const auto seed =
                        kirimasim::test::fuzzSeed(5091 + sample * 0x9e3779b97f4a7c15ULL);
                    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
                    std::mt19937_64 random(seed);
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
                                ASSERT_TRUE(sameWords(out, encoded(qm.pop(front))));
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
                        std::vector<HeapRoot> roots{HeapRoot::container(av),
                                                    HeapRoot::container(qv)};
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
                                ASSERT_TRUE(sameWords(out, encoded(m.read(at))));
                            }
                            ASSERT_TRUE(h.read(t, c, std::nullopt, out));
                            ASSERT_TRUE(sameWords(out, encoded(initial)));
                        }
                        for (const Ref& r : refs) {
                            ASSERT_TRUE(h.readCell(l.value, r.actual, out));
                            ASSERT_TRUE(sameWords(
                                out, encoded((r.queue ? qm : am).readReference(r.model))));
                        }
                    }
                    ASSERT_TRUE(h.collect({}));
                    EXPECT_EQ(h.stats().cells, 0U);
                    EXPECT_EQ(h.stats().containers, 0U);
                }
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

TEST(Heap, InvalidQueueWritesWarnButReadsAndDynamicArrayWritesStaySilent) {
    for (bool empty : {false, true}) {
        SCOPED_TRACE(empty);
        Heap h;
        const auto layout = scalar(h);
        const auto queue = queueType(h, layout), array = arrayType(h, layout);
        auto q = empty ? ContainerToken{} : values(h, queue, {10, 20});
        auto a = empty ? ContainerToken{} : values(h, array, {10, 20});
        const auto originalQ = q, originalA = a;
        const std::array<W4, 1> data{{{99, 0}}};
        const std::array<std::optional<std::int64_t>, 4> indices{
            std::nullopt, -1, 3, std::numeric_limits<std::int64_t>::max()};
        for (const auto index : indices) {
            SCOPED_TRACE(index ? std::to_string(*index) : "unknown");
            const auto status = h.write(queue, q, index, data);
            ASSERT_TRUE(status);
            EXPECT_EQ(status.warning, HeapWarning::InvalidQueueWrite);
            const auto arrayStatus = h.write(array, a, index, data);
            ASSERT_TRUE(arrayStatus);
            EXPECT_EQ(arrayStatus.warning, HeapWarning::None);
            EXPECT_EQ(q, originalQ);
            EXPECT_EQ(a, originalA);
            for (const auto& [type, value] : {std::pair{queue, q}, std::pair{array, a}}) {
                const auto selected = h.cell(type, value, index);
                ASSERT_TRUE(selected);
                EXPECT_EQ(selected.value.bits, 0U);
                EXPECT_EQ(selected.status.warning, HeapWarning::None);
                std::array<W4, 1> out{{{77, 77}}};
                const auto read = h.read(type, value, index, out);
                ASSERT_TRUE(read);
                EXPECT_EQ(read.warning, HeapWarning::None);
                EXPECT_EQ(out[0].a, 0U);
                EXPECT_EQ(out[0].b, 0U);
                EXPECT_EQ(h.size(type, value).value, empty ? 0U : 2U);
                if (!empty) {
                    EXPECT_EQ(at(h, type, value, 0), 10U);
                    EXPECT_EQ(at(h, type, value, 1), 20U);
                }
            }
        }
        const std::int64_t appendAt = empty ? 0 : 2;
        const auto append = h.write(queue, q, appendAt, data);
        ASSERT_TRUE(append);
        EXPECT_EQ(append.warning, HeapWarning::None);
        EXPECT_EQ(h.size(queue, q).value, static_cast<std::uint64_t>(appendAt + 1));
        EXPECT_EQ(at(h, queue, q, appendAt), 99U);
    }
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
    ASSERT_TRUE(sameWords(after, out));
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

TEST(Heap, RollbackDuringReusedEntryAllocationPreservesRoots) {
    bool completed = false;
    for (std::size_t point = 0; point < 300; ++point) {
        SCOPED_TRACE(point);
        FailingMemory memory;
        Heap h(&memory);
        const auto n = nested(h);
        const auto array = arrayType(h, n.leaf);
        ContainerToken stale;
        for (unsigned i = 0; i < 4; ++i)
            stale = values(h, n.innerType, {11, 13, 17, 19});
        const auto detached = h.cell(n.innerType, stale, 2).value;
        const auto staleCell = h.cell(n.innerType, stale, 0).value;
        const std::array<HeapRoot, 3> roots{
            {HeapRoot::container(n.inner), HeapRoot::container(n.value), HeapRoot::cell(detached)}};
        ASSERT_TRUE(h.collect(roots));
        const auto before = h.stats();
        memory.arm(point);
        const auto made = h.copy(n.outerType, n.value);
        memory.disarm();
        if (made) {
            completed = true;
        } else {
            EXPECT_EQ(made.status.error, HeapError::ResourceLimit);
            EXPECT_EQ(h.stats().containers, before.containers);
            EXPECT_EQ(h.stats().cells, before.cells);
        }
        // Reuse the rolled-back entries immediately, before any collection.
        const auto retry = h.construct(array, 12, n.inner);
        ASSERT_TRUE(retry);
        EXPECT_EQ(at(h, array, retry.value, 1), 5U);
        EXPECT_EQ(h.size(n.innerType, stale).status.error, HeapError::InvalidHandle);
        std::array<W4, 1> out{};
        EXPECT_EQ(h.readCell(n.leaf, staleCell, out).error, HeapError::InvalidHandle);
        ASSERT_TRUE(h.readCell(n.leaf, detached, out));
        EXPECT_EQ(out[0].a, 17U);
        ASSERT_TRUE(h.collect(roots));
        EXPECT_EQ(h.stats().containers, before.containers);
        EXPECT_EQ(h.stats().cells, before.cells);
        if (completed) {
            EXPECT_GT(point, 5U);
            break;
        }
    }
    EXPECT_TRUE(completed);
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
    const HeapRoot invalid{static_cast<HeapRoot::Kind>(255), 0};
    EXPECT_EQ(h.collect(std::array<HeapRoot, 1>{{invalid}}).error, HeapError::InvalidType);
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

TEST(Heap, SwappingValuesPreservesCellsAndMatchesIndependentReferenceModel) {
    for (const unsigned width : {1U, 5U, 32U, 64U, 65U, 129U})
        for (const bool four : {false, true})
            for (const bool queue : {false, true}) {
                SCOPED_TRACE(::testing::Message() << width << '/' << four << '/' << queue);
                FailingMemory memory;
                Heap heap(&memory);
                const auto layout = heap.addLayout(integralLayout(width, four));
                ASSERT_TRUE(layout);
                const auto type =
                    queue ? queueType(heap, layout.value, 5) : arrayType(heap, layout.value);
                vs::ContainerValue model(vs::filled(vs::Logic::Zero, width, false));
                std::vector<vs::Value> initial;
                std::vector<W4> input;
                std::mt19937_64 random(32499 + width);
                for (unsigned i = 0; i < 5; ++i) {
                    std::vector<std::uint64_t> a((width + 63) / 64), b(a.size());
                    for (std::size_t w = 0; w < a.size(); ++w) {
                        a[w] = random();
                        b[w] = four ? random() : 0;
                    }
                    initial.push_back(vs::fromWords(a, b, width, false));
                    const auto words = encoded(initial.back());
                    input.insert(input.end(), words.begin(), words.end());
                }
                model.replace(initial);
                const auto made = heap.fromValues(type, input);
                ASSERT_TRUE(made);
                const auto value = made.value;
                std::array<CellToken, 5> cells;
                std::array<std::uint64_t, 5> refs;
                for (unsigned i = 0; i < 5; ++i) {
                    cells[i] = heap.cell(type, value, i).value;
                    refs[i] = model.reference(i);
                }
                struct Notices {
                    std::array<HeapRoot, 2> roots;
                    unsigned count = 0;
                    static void notify(void* frame, HeapRoot root) noexcept {
                        auto& self = *static_cast<Notices*>(frame);
                        if (self.count < 2) self.roots[self.count] = root;
                        ++self.count;
                    }
                } notices;
                heap.setChangeHook({Notices::notify, &notices});
                const auto stats = heap.stats();
                std::vector<W4> out((width + 63) / 64);
                for (unsigned round = 0; round < 150; ++round) {
                    const auto index = [&]() -> std::optional<std::int64_t> {
                        if (random() % 8 == 0) return {};
                        return static_cast<std::int64_t>(random() % 9) - 2;
                    };
                    const auto a = index(), b = index();
                    const bool valid = a && b && *a >= 0 && *b >= 0 && *a < 5 && *b < 5;
                    const bool different =
                        valid && !sameWords(encoded(model.read(a)), encoded(model.read(b)));
                    notices.count = 0;
                    memory.arm(0);
                    const auto result = heap.swapValues(type, value, a, b);
                    memory.disarm();
                    ASSERT_TRUE(result);
                    model.swapValues(a, b);
                    ASSERT_EQ(notices.count, different ? 2U : 0U);
                    if (different) {
                        EXPECT_EQ(notices.roots[0], HeapRoot::cell(cells[*a]));
                        EXPECT_EQ(notices.roots[1], HeapRoot::cell(cells[*b]));
                    }
                    for (unsigned i = 0; i < 5; ++i) {
                        EXPECT_EQ(heap.cell(type, value, i).value, cells[i]);
                        EXPECT_EQ(model.reference(i), refs[i]);
                        ASSERT_TRUE(heap.readCell(layout.value, cells[i], out));
                        EXPECT_TRUE(sameWords(out, encoded(model.readReference(refs[i]))));
                        ASSERT_TRUE(heap.read(type, value, i, out));
                        EXPECT_TRUE(sameWords(out, encoded(model.read(i))));
                    }
                    EXPECT_EQ(heap.stats().containers, stats.containers);
                    EXPECT_EQ(heap.stats().cells, stats.cells);
                    EXPECT_EQ(heap.size(type, value).value, 5U);
                }
                heap.setChangeHook({});
                EXPECT_EQ(heap.swapValues(HeapTypeId{9999}, value, 0, 1).error,
                          HeapError::InvalidType);
                EXPECT_EQ(heap.swapValues(type, ContainerToken{1}, 0, 1).error,
                          HeapError::InvalidHandle);
                EXPECT_TRUE(heap.swapValues(type, {}, 0, 1));
            }
}

TEST(Heap, NestedSwapMovesOwnedValuesWithoutCloningOrInvalidatingInnerReferences) {
    FailingMemory memory;
    Heap heap(&memory);
    const auto leaf = scalar(heap);
    const auto inner = queueType(heap, leaf);
    const auto first = values(heap, inner, {11, 12});
    const auto second = values(heap, inner, {21, 22});
    const std::array<HeapWord, 1> nested{{{~std::uint64_t{0}, false, {0, 0}, inner}}};
    const auto layout = heap.addLayout(nested);
    ASSERT_TRUE(layout);
    const auto outer = queueType(heap, layout.value);
    const std::array<W4, 2> words{{{first.bits, 0}, {second.bits, 0}}};
    const auto made = heap.fromValues(outer, words);
    ASSERT_TRUE(made);
    const auto value = made.value;
    const auto a = ContainerToken{at(heap, outer, value, 0)};
    const auto b = ContainerToken{at(heap, outer, value, 1)};
    const auto ref = heap.cell(inner, a, 0);
    ASSERT_TRUE(ref);
    const auto cell = heap.cell(outer, value, 0).value;
    const auto stats = heap.stats();
    memory.arm(0);
    const auto result = heap.swapValues(outer, value, 0, 1);
    memory.disarm();
    ASSERT_TRUE(result);
    EXPECT_EQ(at(heap, outer, value, 0), b.bits);
    EXPECT_EQ(at(heap, outer, value, 1), a.bits);
    EXPECT_EQ(heap.cell(outer, value, 0).value, cell);
    EXPECT_EQ(heap.stats().cells, stats.cells);
    EXPECT_EQ(heap.stats().containers, stats.containers);
    const std::array<W4, 1> replacement{{{99, 0}}};
    ASSERT_TRUE(heap.writeCell(leaf, ref.value, replacement));
    EXPECT_EQ(at(heap, inner, a, 0), 99U);
    const std::array<HeapRoot, 1> roots{{HeapRoot::container(value)}};
    ASSERT_TRUE(heap.collect(roots));
    EXPECT_EQ(at(heap, inner, ContainerToken{at(heap, outer, value, 1)}, 0), 99U);
    EXPECT_EQ(at(heap, inner, ContainerToken{at(heap, outer, value, 0)}, 1), 22U);
}

}  // namespace
