// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Value copies across equivalent nested queue types (6.22.2, 7.5, 7.10.5;
// SIR_SPEC 5.8, RUNTIME_ABI 6). These original inputs use kernel APIs only.
#include "kirimasim/kernel/heap.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

namespace {
using namespace kirimasim::kernel;

HeapLayoutId layout(Heap& h, std::span<const HeapWord> words) {
    const auto r = h.addLayout(words);
    EXPECT_TRUE(r);
    return r.value;
}
HeapTypeId type(Heap& h, ContainerKind kind, HeapLayoutId element, std::uint64_t capacity = 0) {
    const auto r = h.addType({kind, element, capacity});
    EXPECT_TRUE(r);
    return r.value;
}
HeapLayoutId nestedLayout(Heap& h, HeapTypeId child) {
    const std::array<HeapWord, 1> words{{{~std::uint64_t{0}, false, {0, 0}, child}}};
    return layout(h, words);
}
std::uint64_t length(Heap& h, HeapTypeId t, ContainerToken c) {
    const auto r = h.size(t, c);
    EXPECT_TRUE(r);
    return r.value;
}
ContainerToken child(Heap& h, HeapTypeId t, ContainerToken c, std::int64_t index) {
    std::array<W4, 1> out{};
    EXPECT_TRUE(h.read(t, c, index, out));
    EXPECT_EQ(out[0].b, 0U);
    return {out[0].a};
}
std::size_t kept(std::size_t size, std::uint64_t capacity) {
    return capacity == 0 ? size : std::min(size, static_cast<std::size_t>(capacity));
}

TEST(Heap, NestedBoundCopyMatrixPreservesValuesTypesAndIdentity) {
    for (unsigned width : {1U, 32U, 65U, 129U})
        for (bool four : {false, true})
            for (std::uint64_t sourceBound : {0U, 1U, 3U})
                for (std::uint64_t destinationBound : {0U, 1U, 3U})
                    for (auto sourceKind : {ContainerKind::DynamicArray, ContainerKind::Queue})
                        for (unsigned destination : {0U, 1U, 2U}) {
                            SCOPED_TRACE(::testing::Message()
                                         << width << '/' << four << '/' << sourceBound << '/'
                                         << destinationBound << '/' << static_cast<int>(sourceKind)
                                         << '/' << destination);
                            Heap h;
                            std::vector<HeapWord> words;
                            std::vector<W4> raw;
                            for (unsigned bit = 0; bit < width; bit += 64) {
                                const unsigned bits = std::min(64U, width - bit);
                                const std::uint64_t mask =
                                    bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1;
                                words.push_back(
                                    {mask, four, four ? W4{mask, mask} : W4{0, 0}, kNone});
                            }
                            for (unsigned element = 0; element < 3; ++element)
                                for (const auto& w : words)
                                    raw.push_back({(0xa5U + element) & w.mask,
                                                   four ? (0x12U + element) & w.mask : 0});
                            // Separate registrations exercise structural equivalence too.
                            const auto leafA = layout(h, words), leafB = layout(h, words);
                            const auto a = type(h, ContainerKind::Queue, leafA, sourceBound);
                            const auto b = type(h, ContainerKind::Queue, leafB, destinationBound);
                            const auto outerA = type(h, sourceKind, nestedLayout(h, a));
                            const auto outerB = type(h,
                                                     destination == 0 ? ContainerKind::DynamicArray
                                                                      : ContainerKind::Queue,
                                                     nestedLayout(h, b), destination == 2 ? 1 : 0);
                            const auto input = h.fromValues(a, raw);
                            ASSERT_TRUE(input);
                            const std::array<W4, 2> members{
                                {{input.value.bits, 0}, {input.value.bits, 0}}};
                            const auto original = h.fromValues(outerA, members);
                            ASSERT_TRUE(original);
                            const auto originalCell = h.cell(outerA, original.value, 0).value;
                            const auto originalChild = child(h, outerA, original.value, 0);
                            const auto copied = h.copy(outerB, original.value);
                            ASSERT_TRUE(copied) << static_cast<int>(copied.status.error);
                            const auto sourceCount = kept(3, sourceBound);
                            const auto targetCount = kept(sourceCount, destinationBound);
                            EXPECT_EQ(copied.status.warning,
                                      destination == 2 || targetCount != sourceCount
                                          ? HeapWarning::BoundExceeded
                                          : HeapWarning::None);
                            EXPECT_EQ(length(h, outerB, copied.value), destination == 2 ? 1U : 2U);
                            EXPECT_NE(copied.value, original.value);
                            EXPECT_NE(h.cell(outerB, copied.value, 0).value, originalCell);
                            const auto copiedChild = child(h, outerB, copied.value, 0);
                            EXPECT_NE(copiedChild, originalChild);
                            EXPECT_EQ(length(h, b, copiedChild), targetCount);
                            std::vector<W4> out(words.size());
                            for (std::size_t i = 0; i < targetCount; ++i) {
                                ASSERT_TRUE(
                                    h.read(b, copiedChild, static_cast<std::int64_t>(i), out));
                                for (std::size_t w = 0; w < words.size(); ++w) {
                                    EXPECT_EQ(out[w].a, raw[i * words.size() + w].a);
                                    EXPECT_EQ(out[w].b, raw[i * words.size() + w].b);
                                }
                            }
                            auto mutableOriginal = originalChild;
                            const std::vector<W4> zero(words.size(), {0, 0});
                            ASSERT_TRUE(h.write(a, mutableOriginal, 0, zero));
                            ASSERT_TRUE(h.read(b, copiedChild, 0, out));
                            EXPECT_EQ(out[0].a, raw[0].a);
                            const std::array<HeapRoot, 2> roots{
                                {HeapRoot::container(copied.value), HeapRoot::cell(originalCell)}};
                            ASSERT_TRUE(h.collect(roots));
                            EXPECT_EQ(length(h, a, originalChild), sourceCount);
                            EXPECT_EQ(length(h, b, copiedChild), targetCount);
                        }
}

TEST(Heap, NestedConversionRejectsDifferentElementShapesAndKinds) {
    Heap h;
    const std::array<HeapWord, 1> scalar{{{0xff, false, {0, 0}, kNone}}};
    const auto leaf = layout(h, scalar);
    const auto inner = type(h, ContainerKind::Queue, leaf);
    const auto outer = type(h, ContainerKind::Queue, nestedLayout(h, inner));
    const std::array<W4, 1> raw{{{7, 0}}};
    const auto first = h.fromValues(inner, raw);
    ASSERT_TRUE(first);
    const std::array<W4, 1> nested{{{first.value.bits, 0}}};
    const auto input = h.fromValues(outer, nested);
    ASSERT_TRUE(input);
    for (unsigned mismatch = 0; mismatch < 5; ++mismatch) {
        SCOPED_TRACE(mismatch);
        auto different = scalar;
        if (mismatch == 0) different[0].mask = 0xf;
        if (mismatch == 1) different[0].fourState = true;
        if (mismatch == 2) different[0].initial.a = 1;
        const auto l = layout(h, different);
        const auto t =
            type(h, mismatch == 3 ? ContainerKind::DynamicArray : ContainerKind::Queue, l);
        std::vector<HeapWord> fields{{~std::uint64_t{0}, false, {0, 0}, t}};
        if (mismatch == 4) fields.push_back({0xff, false, {0, 0}, kNone});
        const auto target = type(h, ContainerKind::Queue, layout(h, fields));
        const auto before = h.stats();
        EXPECT_EQ(h.copy(target, input.value).status.error, HeapError::InvalidHandle);
        if (mismatch < 4) {
            EXPECT_EQ(h.fromValues(target, nested).status.error, HeapError::InvalidHandle);
            ContainerToken empty;
            EXPECT_EQ(h.push(target, empty, false, nested).error, HeapError::InvalidHandle);
            EXPECT_EQ(empty.bits, 0U);
        }
        EXPECT_EQ(
            h.construct(type(h, ContainerKind::DynamicArray, layout(h, fields)), 1, input.value)
                .status.error,
            HeapError::InvalidHandle);
        EXPECT_EQ(h.stats().containers, before.containers);
        EXPECT_EQ(h.stats().cells, before.cells);
    }
}

struct Conversion {
    HeapLayoutId targetLayout;
    HeapTypeId sourceInner, targetInner, sourceOuter, targetOuter;
    ContainerToken source, target, input;
};
Conversion conversion(Heap& h, bool dynamic = false) {
    Conversion f;
    const std::array<HeapWord, 1> scalar{{{~std::uint64_t{0}, false, {0, 0}, kNone}}};
    const auto leaf = layout(h, scalar);
    f.sourceInner = type(h, ContainerKind::Queue, leaf);
    f.targetInner = type(h, ContainerKind::Queue, leaf, 2);
    f.sourceOuter = type(h, ContainerKind::Queue, nestedLayout(h, f.sourceInner));
    f.targetLayout = nestedLayout(h, f.targetInner);
    f.targetOuter =
        type(h, dynamic ? ContainerKind::DynamicArray : ContainerKind::Queue, f.targetLayout);
    const std::array<W4, 3> raw{{{7, 0}, {11, 0}, {13, 0}}};
    const auto inner = h.fromValues(f.sourceInner, raw);
    EXPECT_TRUE(inner);
    f.input = inner.value;
    const std::array<W4, 2> members{{{f.input.bits, 0}, {f.input.bits, 0}}};
    const auto outer = h.fromValues(f.sourceOuter, members);
    EXPECT_TRUE(outer);
    f.source = outer.value;
    const std::array<W4, 2> empty{};
    const auto target = h.fromValues(f.targetOuter, empty);
    EXPECT_TRUE(target);
    f.target = target.value;
    return f;
}
HeapStatus convert(unsigned operation, Heap& h, Conversion& f) {
    const std::array<W4, 1> item{{{f.input.bits, 0}}};
    if (operation <= 2) {
        const auto made = operation == 0   ? h.construct(f.targetOuter, 3, f.source)
                          : operation == 1 ? h.copy(f.targetOuter, f.source)
                                           : h.fromValues(f.targetOuter, item);
        if (made) f.target = made.value;
        return made.status;
    }
    switch (operation) {
        case 3: return h.writeCell(f.targetLayout, h.cell(f.targetOuter, f.target, 0).value, item);
        case 4: return h.write(f.targetOuter, f.target, 0, item);
        case 5: return h.write(f.targetOuter, f.target, 2, item);
        case 6: return h.push(f.targetOuter, f.target, true, item);
        case 7: return h.push(f.targetOuter, f.target, false, item);
        default: return h.insert(f.targetOuter, f.target, 1, item);
    }
}

TEST(Heap, NestedConversionAndWarningsUseEveryValueEntry) {
    for (unsigned operation = 0; operation < 9; ++operation) {
        SCOPED_TRACE(operation);
        Heap h;
        auto f = conversion(h, operation == 0);
        const auto oldCell = h.cell(f.targetOuter, f.target, 0).value;
        const auto status = convert(operation, h, f);
        ASSERT_TRUE(status);
        EXPECT_EQ(status.warning, HeapWarning::BoundExceeded);
        const auto index = operation == 5 || operation == 7 ? 2 : operation == 8 ? 1 : 0;
        auto copied = child(h, f.targetOuter, f.target, index);
        EXPECT_EQ(length(h, f.targetInner, copied), 2U);
        std::array<W4, 1> out{};
        ASSERT_TRUE(h.read(f.targetInner, copied, 1, out));
        EXPECT_EQ(out[0].a, 11U);
        const std::array<W4, 1> zero{};
        ASSERT_TRUE(h.write(f.sourceInner, f.input, 0, zero));
        ASSERT_TRUE(h.read(f.targetInner, copied, 0, out));
        EXPECT_EQ(out[0].a, 7U);
        if (operation == 0) {
            EXPECT_EQ(length(h, f.targetOuter, f.target), 3U);
            EXPECT_EQ(child(h, f.targetOuter, f.target, 2).bits, 0U);
        }
        if (operation == 3 || operation == 4)
            EXPECT_EQ(h.cell(f.targetOuter, f.target, 0).value, oldCell);
        if (operation == 6) EXPECT_EQ(h.cell(f.targetOuter, f.target, 1).value, oldCell);
        if (operation >= 7) EXPECT_EQ(h.cell(f.targetOuter, f.target, 0).value, oldCell);
        const std::array<HeapRoot, 1> roots{{HeapRoot::container(f.target)}};
        ASSERT_TRUE(h.collect(roots));
        EXPECT_EQ(length(h, f.targetInner, copied), 2U);
    }
}

TEST(Heap, NestedBranchConversionPreservesScalarWordsAndDefaults) {
    Heap h;
    const std::array<HeapWord, 1> scalar{{{0xff, true, {0xff, 0xff}, kNone}}};
    const auto leaf = layout(h, scalar);
    const auto a = type(h, ContainerKind::Queue, leaf);
    const auto b = type(h, ContainerKind::Queue, leaf, 2);
    const auto c = type(h, ContainerKind::Queue, leaf, 1);
    std::array<HeapWord, 5> fields{{{0xff, true, {0xff, 0xff}, kNone},
                                    {~std::uint64_t{0}, false, {0, 0}, a},
                                    {~std::uint64_t{0}, false, {0, 0}, kNone},
                                    {~std::uint64_t{0}, false, {0, 0}, a},
                                    {~std::uint64_t{0}, false, {0, 0}, a}}};
    const auto source = type(h, ContainerKind::Queue, layout(h, fields));
    fields[1].nested = b;
    fields[3].nested = c;
    fields[4].nested = b;
    const auto target = type(h, ContainerKind::DynamicArray, layout(h, fields));
    const std::array<W4, 3> data{{{0xab, 0x12}, {0xcd, 0x34}, {0xef, 0x56}}};
    const auto inner = h.fromValues(a, data);
    ASSERT_TRUE(inner);
    const std::array<W4, 5> raw{
        {{0x54, 0x12}, {inner.value.bits, 0}, {42, 0}, {inner.value.bits, 0}, {0, 0}}};
    const auto input = h.fromValues(source, raw);
    ASSERT_TRUE(input);
    const auto copied = h.construct(target, 2, input.value);
    ASSERT_TRUE(copied);
    EXPECT_EQ(copied.status.warning, HeapWarning::BoundExceeded);
    std::array<W4, 5> out{};
    ASSERT_TRUE(h.read(target, copied.value, 0, out));
    EXPECT_EQ(out[0].a, raw[0].a);
    EXPECT_EQ(out[0].b, raw[0].b);
    EXPECT_EQ(out[2].a, 42U);
    EXPECT_EQ(out[2].b, 0U);
    EXPECT_EQ(out[4].a | out[4].b, 0U);
    EXPECT_EQ(length(h, b, ContainerToken{out[1].a}), 2U);
    EXPECT_EQ(length(h, c, ContainerToken{out[3].a}), 1U);
    EXPECT_NE(out[1].a, out[3].a);
    ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(copied.value)}}));
    std::array<W4, 1> first{};
    ASSERT_TRUE(h.read(b, ContainerToken{out[1].a}, 1, first));
    EXPECT_EQ(first[0].a, data[1].a);
    EXPECT_EQ(first[0].b, data[1].b);
    ASSERT_TRUE(h.read(target, copied.value, 1, out));
    EXPECT_EQ(out[0].a, 255U);
    EXPECT_EQ(out[0].b, 255U);
    for (std::size_t i = 1; i < out.size(); ++i)
        EXPECT_EQ(out[i].a | out[i].b, 0U);
}

TEST(Heap, SnapshotRetainsActualSourceBoundsAndIndependentCells) {
    for (std::uint64_t bound : {0U, 1U, 3U}) {
        SCOPED_TRACE(bound);
        Heap h;
        const std::array<HeapWord, 1> scalar{{{0xff, false, {0, 0}, kNone}}};
        const auto l = layout(h, scalar);
        const auto actual = type(h, ContainerKind::Queue, l, bound);
        const auto outer = type(h, ContainerKind::Queue, nestedLayout(h, actual));
        const std::array<W4, 3> raw{{{7, 0}, {11, 0}, {13, 0}}};
        const auto inner = h.fromValues(actual, raw);
        ASSERT_TRUE(inner);
        const std::array<W4, 1> member{{{inner.value.bits, 0}}};
        const auto input = h.fromValues(outer, member);
        ASSERT_TRUE(input);
        const auto snapshot = h.snapshot(input.value);
        ASSERT_TRUE(snapshot);
        EXPECT_EQ(snapshot.status.warning, HeapWarning::None);
        const auto saved = child(h, outer, snapshot.value, 0);
        EXPECT_NE(saved, child(h, outer, input.value, 0));
        EXPECT_EQ(length(h, actual, saved), kept(3, bound));
        auto original = input.value;
        ASSERT_TRUE(h.clear(outer, original));
        auto mutableSaved = saved;
        const std::array<W4, 1> extra{{{17, 0}}};
        const auto pushed = h.push(actual, mutableSaved, false, extra);
        ASSERT_TRUE(pushed);
        EXPECT_EQ(pushed.warning, bound == 0 ? HeapWarning::None : HeapWarning::BoundExceeded);
        EXPECT_EQ(length(h, actual, mutableSaved), bound == 0 ? 4U : bound);
        ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(snapshot.value)}}));
        EXPECT_EQ(length(h, actual, mutableSaved), bound == 0 ? 4U : bound);
    }
    Heap h;
    EXPECT_TRUE(h.snapshot({}));
    EXPECT_EQ(h.snapshot({}).value.bits, 0U);
    EXPECT_EQ(h.snapshot(ContainerToken{1}).status.error, HeapError::InvalidHandle);
    EXPECT_EQ(h.stats().containers, 0U);
}

class ConversionMemory final : public std::pmr::memory_resource {
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

TEST(Heap, EquivalentLayoutRegistrationFailureIsAtomic) {
    bool completed = false;
    for (std::size_t point = 0; point < 10; ++point) {
        SCOPED_TRACE(point);
        ConversionMemory memory;
        Heap h(&memory);
        const std::array<HeapWord, 1> scalar{{{0xff, false, {0, 0}, kNone}}};
        const auto leaf = layout(h, scalar);
        const auto a = type(h, ContainerKind::Queue, leaf);
        const auto b = type(h, ContainerKind::Queue, leaf, 1);
        const auto outerA = type(h, ContainerKind::Queue, nestedLayout(h, a));
        const std::array<W4, 2> raw{{{7, 0}, {11, 0}}};
        const auto inner = h.fromValues(a, raw);
        ASSERT_TRUE(inner);
        const std::array<W4, 1> item{{{inner.value.bits, 0}}};
        const auto original = h.fromValues(outerA, item);
        ASSERT_TRUE(original);
        const auto before = h.stats();
        const std::array<HeapWord, 1> next{{{~std::uint64_t{0}, false, {0, 0}, b}}};
        memory.remaining = point;
        const auto attempted = h.addLayout(next);
        // Null snapshots need no memory, even while the allocator fails.
        EXPECT_TRUE(h.snapshot({}));
        memory.remaining.reset();
        const auto registered = attempted ? attempted : h.addLayout(next);
        ASSERT_TRUE(registered);
        EXPECT_EQ(registered.value.index(), 2U);
        if (!attempted) EXPECT_EQ(attempted.status.error, HeapError::ResourceLimit);
        const auto outerB = type(h, ContainerKind::Queue, registered.value);
        const auto copied = h.copy(outerB, original.value);
        ASSERT_TRUE(copied);
        EXPECT_EQ(copied.status.warning, HeapWarning::BoundExceeded);
        EXPECT_EQ(length(h, b, child(h, outerB, copied.value, 0)), 1U);
        EXPECT_EQ(length(h, a, child(h, outerA, original.value, 0)), 2U);
        ASSERT_TRUE(h.collect(std::array<HeapRoot, 2>{
            {HeapRoot::container(original.value), HeapRoot::container(inner.value)}}));
        EXPECT_EQ(h.stats().containers, before.containers);
        EXPECT_EQ(h.stats().cells, before.cells);
        EXPECT_EQ(h.snapshot(copied.value).status.error, HeapError::InvalidHandle);
        if (attempted) {
            completed = true;
            EXPECT_GT(point, 0U);
            break;
        }
    }
    EXPECT_TRUE(completed);
}

TEST(Heap, NestedConversionAllocationFailureLeavesEveryEntryUnchanged) {
    for (unsigned operation = 0; operation < 10; ++operation) {
        bool completed = false;
        for (std::size_t point = 0; point < 300; ++point) {
            SCOPED_TRACE(::testing::Message() << operation << '/' << point);
            ConversionMemory memory;
            Heap h(&memory);
            auto f = conversion(h, operation == 0);
            const auto before = h.stats();
            const auto oldTarget = f.target;
            const auto oldCell = h.cell(f.targetOuter, f.target, 0).value;
            const auto oldChild = child(h, f.sourceOuter, f.source, 0);
            memory.remaining = point;
            const auto status =
                operation == 9 ? h.snapshot(f.source).status : convert(operation, h, f);
            memory.remaining.reset();
            if (status) {
                completed = true;
                EXPECT_GT(point, 5U);
                break;
            }
            EXPECT_EQ(status.error, HeapError::ResourceLimit);
            EXPECT_EQ(status.warning, HeapWarning::None);
            EXPECT_EQ(f.target, oldTarget);
            EXPECT_EQ(h.cell(f.targetOuter, f.target, 0).value, oldCell);
            EXPECT_EQ(child(h, f.targetOuter, f.target, 0).bits, 0U);
            EXPECT_EQ(child(h, f.sourceOuter, f.source, 0), oldChild);
            EXPECT_EQ(length(h, f.sourceInner, oldChild), 3U);
            EXPECT_EQ(h.stats().containers, before.containers);
            EXPECT_EQ(h.stats().cells, before.cells);
            // Reuse rolled-back entries before any collection.
            const auto retry = h.snapshot(f.source);
            ASSERT_TRUE(retry);
            ASSERT_TRUE(h.collect(std::array<HeapRoot, 3>{{HeapRoot::container(f.source),
                                                           HeapRoot::container(f.target),
                                                           HeapRoot::container(f.input)}}));
            EXPECT_EQ(h.stats().containers, before.containers);
            EXPECT_EQ(h.stats().cells, before.cells);
        }
        EXPECT_TRUE(completed);
    }
}

TEST(Heap, DeepBoundConversionUsesNoNativeRecursion) {
    Heap h;
    const std::array<HeapWord, 1> scalar{{{0xff, false, {0, 0}, kNone}}};
    const auto leaf = layout(h, scalar);
    auto source = type(h, ContainerKind::Queue, leaf, 3);
    auto target = type(h, ContainerKind::Queue, leaf, 1);
    std::vector<HeapTypeId> targets{target};
    const std::array<W4, 3> raw{{{7, 0}, {11, 0}, {13, 0}}};
    auto value = h.fromValues(source, raw).value;
    constexpr unsigned depth = 2500;
    for (unsigned i = 0; i < depth; ++i) {
        source = type(h, ContainerKind::Queue, nestedLayout(h, source), 3);
        target = type(h, ContainerKind::Queue, nestedLayout(h, target), 1);
        targets.push_back(target);
        const std::array<W4, 1> item{{{value.bits, 0}}};
        const auto next = h.fromValues(source, item);
        ASSERT_TRUE(next);
        value = next.value;
        ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(value)}}));
    }
    const auto converted = h.copy(target, value);
    ASSERT_TRUE(converted);
    EXPECT_EQ(converted.status.warning, HeapWarning::BoundExceeded);
    ASSERT_TRUE(h.collect(std::array<HeapRoot, 1>{{HeapRoot::container(converted.value)}}));
    EXPECT_EQ(h.stats().containers, depth + 1U);
    EXPECT_EQ(h.stats().cells, depth + 1U);
    auto current = converted.value;
    for (std::size_t i = targets.size() - 1; i > 0; --i) {
        EXPECT_EQ(length(h, targets[i], current), 1U);
        current = child(h, targets[i], current, 0);
    }
    EXPECT_EQ(length(h, targets[0], current), 1U);
    std::array<W4, 1> out{};
    ASSERT_TRUE(h.read(targets[0], current, 0, out));
    EXPECT_EQ(out[0].a, 7U);
}

}  // namespace
