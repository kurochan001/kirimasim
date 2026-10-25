// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original shared-extent checks of automatic-capture-proposal 1, 2, 4, 6, 8.
// This is a kernel foundation, before SIR/front capture execution is enabled.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/activation.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
using Error = k::ActivationError;
using Kind = k::ActivationKind;

k::ActivationLayoutId layout(k::ActivationStore& store, std::vector<k::ActivationField> fields) {
    auto result = store.addLayout(fields);
    EXPECT_TRUE(result);
    return result.value;
}
k::ActivationBinding bind(k::ActivationStore& store, const k::ActivationLease& owner,
                          std::uint32_t field = 0, bool readonly = false) {
    auto result = store.bind(owner, field, readonly);
    EXPECT_TRUE(result);
    return result.value;
}
void write(k::ActivationStore& store, k::ActivationBinding field, std::uint64_t value,
           std::uint32_t element = 0) {
    const std::array<k::W4, 1> words{{{value, 0}}};
    ASSERT_EQ(store.write(field, element, words), Error::None);
}
std::uint64_t read(k::ActivationStore& store, k::ActivationBinding field,
                   std::uint32_t element = 0) {
    std::array<k::W4, 1> words{};
    EXPECT_EQ(store.read(field, element, words), Error::None);
    EXPECT_EQ(words[0].b, 0U);
    return words[0].a;
}

TEST(Activation, SharedFieldsSurviveOwnerReturnAndNestedChildRelease) {
    k::ActivationStore store;
    const auto id =
        layout(store, {{Kind::Two, 32, {}}, {Kind::String, 64, {}}, {Kind::Four, 1, {{7, 6}}}});
    auto owner = store.create(id);
    ASSERT_TRUE(owner);
    const auto number = bind(store, owner.value);
    const auto text = bind(store, owner.value, 1);
    write(store, number, 7);
    write(store, text, 91);
    auto first = store.retain(number);
    auto sibling = store.retain(number);
    ASSERT_TRUE(first);
    ASSERT_TRUE(sibling);
    EXPECT_EQ(store.stats().live, 1U);
    EXPECT_EQ(store.stats().leases, 3U);
    auto nested = store.retain(first.value);
    ASSERT_TRUE(nested);
    owner.value.reset();
    write(store, number, 9);
    first.value.reset();
    sibling.value.reset();
    EXPECT_EQ(read(store, number), 9U);
    EXPECT_EQ(read(store, text), 91U);  // The whole lexical extent survives.
    EXPECT_EQ(store.stats().leases, 1U);
    EXPECT_EQ(store.stats().words, 6U);
    nested.value.reset();
    EXPECT_EQ(store.stats().live, 0U);
    EXPECT_EQ(store.stats().words, 0U);
    EXPECT_FALSE(store.retain(number));
}

TEST(Activation, ReentryCapturesOldBindingAndReusesOnlyReleasedRecords) {
    k::ActivationStore store;
    const auto id = layout(store, {{Kind::Two, 64, {}}});
    auto old = store.create(id);
    ASSERT_TRUE(old);
    const auto previous = bind(store, old.value);
    write(store, previous, 7);
    auto child = store.retain(previous);
    ASSERT_TRUE(child);
    old.value.reset();
    auto current = store.create(id);
    ASSERT_TRUE(current);
    const auto fresh = bind(store, current.value);
    write(store, fresh, 19);
    EXPECT_NE(previous.token(), fresh.token());
    EXPECT_EQ(read(store, previous), 7U);
    EXPECT_EQ(read(store, fresh), 19U);
    child.value.reset();
    auto reused = store.create(id);
    ASSERT_TRUE(reused);
    EXPECT_NE(reused.value.token(), previous.token());
    EXPECT_EQ(read(store, bind(store, reused.value)), 0U);
    EXPECT_FALSE(store.retain(previous));
    EXPECT_EQ(store.stats().entries, 2U);
}

TEST(Activation, RightsAndContextIdentitySurviveRetainAndMoves) {
    k::Context a, b;
    const auto ia = layout(a.activations(), {{Kind::Two, 64, {}}});
    const auto ib = layout(b.activations(), {{Kind::Two, 64, {}}});
    auto first = a.activations().create(ia), second = b.activations().create(ib);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto source = bind(a.activations(), first.value);
    const auto other = bind(b.activations(), second.value);
    // Raw numbers are Context-local, while C++ bindings carry store identity.
    EXPECT_EQ(source.token(), other.token());
    EXPECT_FALSE(b.activations().retain(source));
    EXPECT_FALSE(b.activations().bind(first.value, 0));
    auto readonly = a.activations().readOnly(source);
    ASSERT_TRUE(readonly);
    auto child = a.activations().retain(readonly.value);
    ASSERT_TRUE(child);
    auto moved = std::move(child.value);
    EXPECT_FALSE(child.value);
    const auto restricted = bind(a.activations(), moved);
    ASSERT_TRUE(restricted.readonly());
    const std::array<k::W4, 1> value{{{9, 0}}};
    EXPECT_EQ(a.activations().write(restricted, 0, value), Error::ReadOnly);
    write(a.activations(), source, 5);
    write(b.activations(), other, 11);
    EXPECT_EQ(read(a.activations(), restricted), 5U);
    EXPECT_EQ(read(b.activations(), other), 11U);
}

TEST(Activation, ClearInvalidatesOwnersWithoutReusingTheirGeneration) {
    k::ActivationStore store;
    const auto id = layout(store, {{Kind::Two, 64, {}}});
    auto old = store.create(id);
    ASSERT_TRUE(old);
    const auto stale = bind(store, old.value);
    store.clear();
    EXPECT_FALSE(old.value);
    auto current = store.create(id);
    ASSERT_TRUE(current);
    const auto fresh = bind(store, current.value);
    EXPECT_NE(stale.token(), fresh.token());
    old.value.reset();  // Cannot release the new allocation.
    EXPECT_EQ(store.stats().leases, 1U);
    EXPECT_FALSE(store.retain(stale));
    write(store, fresh, 19);
    EXPECT_EQ(read(store, fresh), 19U);
}

TEST(Activation, TypedChunksAndPackedSelectionsMatchIndependentBits) {
    for (const auto kind : {Kind::Two, Kind::Four})
        for (const std::uint32_t width : {1U, 31U, 32U, 63U, 64U, 65U, 127U, 128U, 129U, 257U})
            for (const auto& dims :
                 {std::vector<k::ActivationDim>{}, std::vector<k::ActivationDim>{{4, 5}},
                  std::vector<k::ActivationDim>{{7, 6}},
                  std::vector<k::ActivationDim>{{2, 1}, {-3, -1}}}) {
                SCOPED_TRACE(width);
                SCOPED_TRACE(static_cast<unsigned>(kind));
                SCOPED_TRACE(dims.size());
                k::ActivationStore store;
                const auto id = layout(store, {{kind, width, dims}});
                auto owner = store.create(id);
                ASSERT_TRUE(owner);
                const auto at = bind(store, owner.value);
                const auto n = (width + 63) / 64;
                const auto elements = dims.empty() ? 1U : dims.size() == 1 ? 2U : 6U;
                std::vector<k::W4> output(n);
                std::vector<std::vector<k::W4>> input(elements, std::vector<k::W4>(n));
                for (std::uint32_t e = 0; e < elements; ++e) {
                    for (std::uint32_t c = 0; c < n; ++c)
                        input[e][c] = {0xfedcba9876543210ULL ^
                                           (std::uint64_t{c} * 0x1111111111111111ULL) ^ e,
                                       0xa55aa55aa55aa55aULL ^ (std::uint64_t{e} << 1)};
                    ASSERT_EQ(store.write(at, e, input[e]), Error::None);
                }
                for (std::uint32_t e = 0; e < elements; ++e) {
                    ASSERT_EQ(store.read(at, e, output), Error::None);
                    for (std::uint32_t bit = 0; bit < n * 64; ++bit) {
                        const bool a = ((input[e][bit / 64].a >> (bit % 64)) & 1U) != 0;
                        const bool b = ((input[e][bit / 64].b >> (bit % 64)) & 1U) != 0;
                        EXPECT_EQ((output[bit / 64].a >> (bit % 64)) & 1U,
                                  bit < width && (kind == Kind::Four ? a : a && !b));
                        EXPECT_EQ((output[bit / 64].b >> (bit % 64)) & 1U,
                                  bit < width && kind == Kind::Four && b);
                    }
                }
                if (width == 1) {
                    for (std::uint64_t state = 0; state < 4; ++state) {
                        const std::array<k::W4, 1> bit{{{state & 1, state >> 1}}};
                        ASSERT_EQ(store.write(at, 0, bit), Error::None);
                        ASSERT_EQ(store.read(at, 0, output), Error::None);
                        EXPECT_EQ(output[0].a,
                                  kind == Kind::Four ? state & 1 : (state & 1) & !(state >> 1));
                        EXPECT_EQ(output[0].b, kind == Kind::Four ? state >> 1 : 0);
                    }
                }
                if (width > 64) {
                    const std::array<k::W4, 2> part{
                        {{0x0123456789abcdefULL, 0xfedcba9876543210ULL}, {1, 1}}};
                    const auto count = std::min(65U, width - 63);
                    const auto parts = (count + 63) / 64;
                    ASSERT_EQ(store.write(at, 0, std::span(part).first(parts), 63, count),
                              Error::None);
                    ASSERT_EQ(store.read(at, 0, output), Error::None);
                    for (std::uint32_t bit = 0; bit < n * 64; ++bit) {
                        const bool selected = bit >= 63 && bit < 63 + count;
                        const auto i = selected ? bit - 63 : bit;
                        const auto word = selected ? part[i / 64] : input[0][i / 64];
                        const bool a = ((word.a >> (i % 64)) & 1U) != 0;
                        const bool b = ((word.b >> (i % 64)) & 1U) != 0;
                        EXPECT_EQ((output[bit / 64].a >> (bit % 64)) & 1U,
                                  bit < width && (kind == Kind::Four ? a : a && !b));
                        EXPECT_EQ((output[bit / 64].b >> (bit % 64)) & 1U,
                                  bit < width && kind == Kind::Four && b);
                    }
                }
            }
}

struct Observer {
    k::ActivationStore* store = nullptr;
    k::ActivationBinding binding;
    unsigned reads = 0, changes = 0;
    bool complete = true;
    k::ActivationAccess last{};
    static void read(void* frame, const k::ActivationAccess&) noexcept {
        ++static_cast<Observer*>(frame)->reads;
    }
    static void change(void* frame, const k::ActivationAccess& access) noexcept {
        auto& self = *static_cast<Observer*>(frame);
        ++self.changes;
        self.last = access;
        std::array<k::W4, 3> value{};
        self.complete = self.complete && self.store->read(self.binding, 0, value) == Error::None &&
                        value[0].a == UINT64_MAX && value[1].a == UINT64_MAX && value[2].a == 1;
    }
};
TEST(Activation, NotificationsSeeCompleteNormalizedValuesAndNoFailedWrites) {
    k::ActivationStore store;
    const auto id = layout(store, {{Kind::Four, 129, {}}});
    auto owner = store.create(id);
    ASSERT_TRUE(owner);
    const auto at = bind(store, owner.value);
    Observer observer{&store, at};
    store.setReadHook({Observer::read, &observer});
    store.setChangeHook({Observer::change, &observer});
    const std::array<k::W4, 3> ones{{{UINT64_MAX, 0}, {UINT64_MAX, 0}, {UINT64_MAX, 0}}};
    ASSERT_EQ(store.write(at, 0, ones), Error::None);
    ASSERT_EQ(store.write(at, 0, ones), Error::None);
    EXPECT_EQ(observer.changes, 1U);
    EXPECT_TRUE(observer.complete);
    EXPECT_EQ(observer.last.width, 129U);
    const auto readonly = store.readOnly(at);
    ASSERT_TRUE(readonly);
    EXPECT_EQ(store.write(readonly.value, 0, ones), Error::ReadOnly);
    EXPECT_EQ(store.write(at, 1, ones), Error::OutOfRange);
    EXPECT_EQ(store.write(at, 0, std::span(ones).first(1)), Error::InvalidValue);
    EXPECT_EQ(observer.changes, 1U);
    EXPECT_EQ(observer.reads, 1U);
    std::array<k::W4, 1> canary{{{11, 13}}};
    EXPECT_EQ(store.read(at, 1, canary), Error::OutOfRange);
    EXPECT_EQ(canary[0].a, 11U);
    EXPECT_EQ(canary[0].b, 13U);
}

struct RootList {
    std::array<k::HeapRoot, 4> values{};
    std::size_t count = 0;
    static void append(void* frame, k::HeapRoot root) noexcept {
        auto& self = *static_cast<RootList*>(frame);
        self.values[self.count++] = root;
    }
    std::size_t size() const noexcept { return count; }
    bool empty() const noexcept { return count == 0; }
    void clear() noexcept { count = 0; }
    std::span<const k::HeapRoot> view() const noexcept { return std::span(values).first(count); }
    const k::HeapRoot& operator[](std::size_t index) const noexcept { return values[index]; }
};

TEST(Activation, TypedRootsKeepContainersButIgnoreEqualIntegralPatterns) {
    k::Context context;
    auto& heap = context.heap();
    auto& store = context.activations();
    const std::array<k::HeapWord, 1> words{};
    const auto shape = heap.addLayout(words);
    ASSERT_TRUE(shape);
    const auto type = heap.addType({k::ContainerKind::DynamicArray, shape.value, 0});
    ASSERT_TRUE(type);
    const auto value = heap.construct(type.value, 2);
    ASSERT_TRUE(value);
    const auto other = heap.construct(type.value, 3);
    ASSERT_TRUE(other);
    const auto id =
        layout(store, {{Kind::Container, 64, {}}, {Kind::Two, 64, {}}, {Kind::String, 64, {}}});
    auto owner = store.create(id);
    ASSERT_TRUE(owner);
    const auto container = bind(store, owner.value);
    write(store, container, value.value.bits);
    write(store, bind(store, owner.value, 1), other.value.bits);
    write(store, bind(store, owner.value, 2), other.value.bits);
    auto child = store.retain(container);
    ASSERT_TRUE(child);
    owner.value.reset();
    RootList roots;
    const auto visit = &RootList::append;
    store.visitRoots(visit, &roots);
    ASSERT_EQ(roots.size(), 1U);
    ASSERT_TRUE(heap.collect(roots.view()));
    EXPECT_TRUE(heap.actualType(value.value));
    EXPECT_FALSE(heap.actualType(other.value));
    store.acknowledgeRoots();
    child.value.reset();
    EXPECT_TRUE(store.rootsChanged());
    roots.clear();
    store.visitRoots(visit, &roots);
    EXPECT_TRUE(roots.empty());
    ASSERT_TRUE(heap.collect(roots.view()));
    EXPECT_FALSE(heap.actualType(value.value));
}

class FailingMemory final : public std::pmr::memory_resource {
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
    void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(Activation, AllocationFailuresAreAtomicAndReleaseNeverAllocates) {
    unsigned layoutFailures = 0, createFailures = 0;
    for (std::size_t budget = 0; budget < 8; ++budget) {
        FailingMemory memory;
        k::ActivationStore store(&memory);
        const std::vector<k::ActivationField> fields{{Kind::Four, 129, {{7, 6}, {-3, -1}}},
                                                     {Kind::Container, 64, {}}};
        memory.remaining = budget;
        const auto registered = store.addLayout(fields);
        if (!registered) {
            EXPECT_EQ(registered.error, Error::ResourceLimit);
            ++layoutFailures;
            continue;
        }
        memory.remaining.reset();
        auto live = store.create(registered.value);
        ASSERT_TRUE(live);
        const auto binding = bind(store, live.value, 1);
        write(store, binding, 51);
        const auto before = store.stats();
        memory.remaining = 0;
        auto failed = store.create(registered.value);
        EXPECT_FALSE(failed);
        EXPECT_EQ(failed.error, Error::ResourceLimit);
        ++createFailures;
        EXPECT_EQ(store.stats().live, before.live);
        EXPECT_EQ(store.stats().leases, before.leases);
        EXPECT_EQ(read(store, binding), 51U);
        live.value.reset();
        EXPECT_EQ(store.stats().live, 0U);
        EXPECT_EQ(store.stats().words, 0U);
    }
    EXPECT_GT(layoutFailures, 0U);
    EXPECT_GT(createFailures, 0U);
}

TEST(Activation, RealsHandlesAndTwoStateWritesNormalizeBeforeNotification) {
    k::ActivationStore store;
    const auto id = layout(store, {{Kind::Real, 64, {}},
                                   {Kind::ShortReal, 32, {}},
                                   {Kind::String, 64, {}},
                                   {Kind::Chandle, 64, {}},
                                   {Kind::Two, 65, {}}});
    auto owner = store.create(id);
    ASSERT_TRUE(owner);
    for (std::uint32_t field = 0; field < 4; ++field) {
        const auto at = bind(store, owner.value, field);
        const std::array<k::W4, 1> invalid{{{9, 1}}};
        EXPECT_EQ(store.write(at, 0, invalid), Error::InvalidValue);
        write(store, at, UINT64_MAX);
        EXPECT_EQ(read(store, at), field == 1 ? 0xffffffffULL : UINT64_MAX);
        EXPECT_EQ(store.write(at, 0, invalid, 1, 1), Error::OutOfRange);
    }
    unsigned changes = 0;
    store.setChangeHook(
        {[](void* frame, const k::ActivationAccess&) noexcept { ++*static_cast<unsigned*>(frame); },
         &changes});
    const auto at = bind(store, owner.value, 4);
    const std::array<k::W4, 2> unknown{{{UINT64_MAX, UINT64_MAX}, {UINT64_MAX, UINT64_MAX}}};
    EXPECT_EQ(store.write(at, 0, unknown), Error::None);
    EXPECT_EQ(changes, 0U);
    const std::array<k::W4, 2> one{{{1, 0}, {0, 0}}};
    EXPECT_EQ(store.write(at, 0, one), Error::None);
    EXPECT_EQ(changes, 1U);
}

TEST(Activation, DetachedCellRootsAndRootOnlyReplacementReleaseOldValues) {
    k::Context context;
    auto& heap = context.heap();
    auto& store = context.activations();
    const std::array<k::HeapWord, 1> words{};
    const auto shape = heap.addLayout(words);
    ASSERT_TRUE(shape);
    const auto type = heap.addType({k::ContainerKind::DynamicArray, shape.value, 0});
    ASSERT_TRUE(type);
    auto array = heap.construct(type.value, 2);
    ASSERT_TRUE(array);
    const auto cell = heap.cell(type.value, array.value, 1);
    ASSERT_TRUE(cell);
    const auto id = layout(store, {{Kind::Cell, 64, {}}});
    auto owner = store.create(id);
    ASSERT_TRUE(owner);
    const auto at = bind(store, owner.value);
    write(store, at, cell.value.bits);
    RootList roots;
    const auto visit = &RootList::append;
    store.visitRoots(visit, &roots);
    ASSERT_EQ(roots.size(), 1U);
    EXPECT_EQ(roots[0].kind, k::HeapRoot::Kind::Cell);
    ASSERT_TRUE(heap.collect(roots.view()));
    EXPECT_FALSE(heap.actualType(array.value));
    std::array<k::W4, 1> value{};
    EXPECT_TRUE(heap.readCell(shape.value, cell.value, value));
    store.acknowledgeRoots();
    write(store, at, 0);
    EXPECT_TRUE(store.rootsChanged());
    roots.clear();
    store.visitRoots(visit, &roots);
    ASSERT_TRUE(heap.collect(roots.view()));
    EXPECT_FALSE(heap.readCell(shape.value, cell.value, value));
}

TEST(Activation, EveryAllocationPointPreservesExistingBindingsAndFreeEntries) {
    unsigned failed = 0, passed = 0;
    for (const bool reusable : {false, true})
        for (std::size_t budget = 0; budget < 4; ++budget) {
            FailingMemory memory;
            k::ActivationStore store(&memory);
            const auto id = layout(store, {{Kind::Four, 129, {{7, 6}}}, {Kind::Two, 64, {}}});
            auto parent = store.create(id);
            ASSERT_TRUE(parent);
            const auto at = bind(store, parent.value, 1);
            write(store, at, 51);
            if (reusable) {
                auto free = store.create(id);
                ASSERT_TRUE(free);
            }
            const auto before = store.stats();
            store.acknowledgeRoots();
            memory.remaining = budget;
            auto result = store.create(id);
            if (result) {
                ++passed;
                memory.remaining = 0;
                result.value.reset();
            } else {
                ++failed;
                EXPECT_EQ(result.error, Error::ResourceLimit);
                EXPECT_FALSE(store.rootsChanged());
                EXPECT_EQ(store.stats().entries, before.entries);
            }
            EXPECT_EQ(store.stats().live, before.live);
            EXPECT_EQ(store.stats().words, before.words);
            EXPECT_EQ(store.stats().leases, before.leases);
            EXPECT_EQ(read(store, at), 51U);
        }
    EXPECT_GT(failed, 0U);
    EXPECT_GT(passed, 0U);
    FailingMemory memory;
    k::ActivationStore store(&memory);
    memory.remaining = 0;
    const std::array<k::ActivationField, 1> invalid{{{Kind::Four, 0, {}}}};
    EXPECT_EQ(store.addLayout(invalid).error, Error::InvalidLayout);
    for (std::size_t budget = 0; budget < 5; ++budget) {
        FailingMemory source;
        k::ActivationStore existing(&source);
        const auto original = layout(existing, {{Kind::Two, 64, {}}});
        auto owner = existing.create(original);
        ASSERT_TRUE(owner);
        const auto at = bind(existing, owner.value);
        write(existing, at, 71);
        const std::vector<k::ActivationField> fields{{Kind::Four, 129, {{7, 6}}}};
        source.remaining = budget;
        const auto added = existing.addLayout(fields);
        if (!added) EXPECT_EQ(added.error, Error::ResourceLimit);
        EXPECT_EQ(read(existing, at), 71U);
        EXPECT_EQ(existing.stats().live, 1U);
        EXPECT_EQ(existing.stats().words, 1U);
    }
}

kirimasim::sir::Design collectingDesign(bool wait = false) {
    namespace s = kirimasim::sir;
    s::Design d;
    d.sources = {"activation.sv"};
    d.instances = {{"top", "top", s::kNone}};
    s::TypeEntry array;
    array.name = "array";
    array.kind = s::TypeEntryKind::DynArray;
    array.value = s::u2(64);
    d.types.push_back(array);
    const auto container = s::dynarrayType(s::TypeId{0});
    d.constants = {{s::s4(64), {1}, {0}}, {container, {0}, {}}};
    s::Process p;
    p.name = "top.initial";
    p.loc = {0, 1, 1};
    p.numTemps = 1;
    p.blocks.resize(1);
    p.blocks[0].instrs = {
        {s::Op::ContainerNew,
         container,
         s::TempId{0},
         {s::Operand::constant(s::ConstId{0}), s::Operand::constant(s::ConstId{1})},
         p.loc},
        {s::Op::Ret, s::voidType(), s::kNone, {}, p.loc}};
    d.processes.push_back(p);
    if (wait) {
        d.constants.push_back({s::s4(64), {5}, {0}});
        auto& process = d.processes[0];
        process.blocks.resize(2);
        process.blocks[0].instrs.back() = {
            s::Op::WaitDelay,
            s::voidType(),
            s::kNone,
            {s::Operand::constant(s::ConstId{2}), s::Operand::block(s::BlockId{1})},
            process.loc};
        process.blocks[1].instrs = {{s::Op::Ret, s::voidType(), s::kNone, {}, process.loc}};
    }
    return d;
}

TEST(Activation, VmSafePointsIncludeRetainedKernelStorageRoots) {
    namespace s = kirimasim::sir;
    k::Context context;
    auto& heap = context.heap();
    auto& store = context.activations();
    const std::array<k::HeapWord, 1> word{};
    const auto element = heap.addLayout(word);
    ASSERT_TRUE(element);
    const auto type = heap.addType({k::ContainerKind::DynamicArray, element.value, 0});
    ASSERT_TRUE(type);
    const auto value = heap.construct(type.value, 2);
    ASSERT_TRUE(value);
    const auto scope = layout(store, {{Kind::Container, 64, {}}});
    auto owner = store.create(scope);
    ASSERT_TRUE(owner);
    write(store, bind(store, owner.value), value.value.bits);
    auto d = collectingDesign();
    const auto errors = s::verify(d);
    for (const auto& error : errors)
        ADD_FAILURE() << error.message;
    ASSERT_TRUE(errors.empty());
    const auto result = kirimasim::vm::run(d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_TRUE(heap.actualType(value.value));
    EXPECT_EQ(heap.stats().containers, 1U);
    EXPECT_EQ(heap.stats().cells, 2U);
}

struct ScopeRelease {
    k::ProcessId process{0};
    k::ActivationLease* owner = nullptr;
    k::ContainerToken container;
    unsigned step = 0;
    bool before = false, noMutation = false, after = false;
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& self = *static_cast<ScopeRelease*>(frame);
        auto& ctx = k::fromC(context);
        if (self.step++ == 0) {
            ctx.scheduler().waitDelay(self.process, 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        if (self.step == 2) {
            self.before = static_cast<bool>(ctx.heap().actualType(self.container));
            self.noMutation = !ctx.heap().needsCollection();
            self.owner->reset();
            self.noMutation = self.noMutation && !ctx.heap().needsCollection();
            ctx.scheduler().waitDelay(self.process, 0);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        self.after = !ctx.heap().actualType(self.container);
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(Activation, VmSafePointsCollectAfterRootOnlyLeaseRelease) {
    k::Context context;
    auto& heap = context.heap();
    auto& store = context.activations();
    const std::array<k::HeapWord, 1> word{};
    const auto element = heap.addLayout(word);
    ASSERT_TRUE(element);
    const auto type = heap.addType({k::ContainerKind::DynamicArray, element.value, 0});
    ASSERT_TRUE(type);
    const auto value = heap.construct(type.value, 2);
    ASSERT_TRUE(value);
    const auto scope = layout(store, {{Kind::Container, 64, {}}});
    auto owner = store.create(scope);
    ASSERT_TRUE(owner);
    write(store, bind(store, owner.value), value.value.bits);
    ScopeRelease observer;
    observer.owner = &owner.value;
    observer.container = value.value;
    observer.process =
        context.scheduler().addProcess(&ScopeRelease::resume, &observer, k::Group::Active);
    context.scheduler().start(observer.process);
    const auto d = collectingDesign(true);
    ASSERT_TRUE(kirimasim::sir::verify(d).empty());
    const auto result = kirimasim::vm::run(d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_TRUE(observer.before);
    EXPECT_TRUE(observer.noMutation);
    EXPECT_TRUE(observer.after);
    EXPECT_EQ(heap.stats().containers, 0U);
    EXPECT_EQ(heap.stats().cells, 0U);
}

TEST(Activation, InvalidLayoutsFailAndRepeatedCompletedScopesRemainBounded) {
    k::ActivationStore store;
    const std::vector<k::ActivationField> invalid[]{
        {{Kind::Four, 0, {}}},
        {{Kind::Real, 32, {}}},
        {{Kind::ShortReal, 64, {}}},
        {{Kind::String, 1, {}}},
        {{static_cast<Kind>(255), 64, {}}},
        {{Kind::Four,
          1,
          {{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}}}},
        {{Kind::Four, 1, {{0, 0xffffffffLL}, {0, 0xffffffffLL}}}}};
    for (const auto& fields : invalid)
        EXPECT_EQ(store.addLayout(fields).error, Error::InvalidLayout);
    const auto id = layout(store, {{Kind::Two, 64, {}}});
    for (unsigned i = 0; i < 100000; ++i) {
        auto owner = store.create(id);
        ASSERT_TRUE(owner);
        auto child = store.retain(owner.value);
        ASSERT_TRUE(child);
    }
    EXPECT_EQ(store.stats().live, 0U);
    EXPECT_EQ(store.stats().leases, 0U);
    EXPECT_EQ(store.stats().entries, 1U);
    const auto empty = layout(store, {});
    auto scope = store.create(empty);
    ASSERT_TRUE(scope);
    EXPECT_FALSE(store.bind(scope.value, 0));
    auto child = store.retain(scope.value);
    ASSERT_TRUE(child);
    scope.value.reset();
    EXPECT_EQ(store.stats().live, 1U);
    EXPECT_EQ(store.stats().words, 0U);
    child.value.reset();
    auto reused = store.create(id);
    ASSERT_TRUE(reused);
    EXPECT_EQ(read(store, bind(store, reused.value)), 0U);
}
}  // namespace
