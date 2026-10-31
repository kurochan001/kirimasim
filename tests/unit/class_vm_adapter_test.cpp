// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original matrices for the approved class contract 1, 3-6, 8 and 10.
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "activation_environment.h"
#include "frame_value.h"
#include "heap_plan.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/classes.h"
#include "object_values.h"
#include "gtest/gtest.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
namespace v = kirimasim::vm::detail;
using E = k::ObjectError;
using W = k::W4;

s::Instr effect(s::Op op, std::vector<s::Operand> operands = {}) {
    return {op, s::voidType(), s::kNone, std::move(operands), {0, 1, 1}};
}
struct Fixture {
    s::Design d;
    explicit Fixture(s::Type scalar = s::u4(129), bool initialized = false) {
        d.sources = {"objects.sv"};
        d.instances = {{"top", "top", s::kNone}};
        s::TypeEntry queue;
        queue.kind = s::TypeEntryKind::Queue;
        queue.value = scalar;
        queue.maxElements = 2;
        s::TypeEntry array;
        array.kind = s::TypeEntryKind::DynArray;
        array.value = s::classType(s::ClassId{1});
        s::TypeEntry nested;
        nested.kind = s::TypeEntryKind::Queue;
        nested.value = s::dynarrayType(s::TypeId{1});
        d.types = {queue, array, nested};
        d.classes.resize(5);
        d.classes[0].name = "Derived";
        d.classes[0].base = s::ClassId{1};
        d.classes[0].interfaces = {s::ClassId{2}};
        d.classes[0].fields = {{"value", s::u2(8)}};
        d.classes[1].name = "Base";
        d.classes[1].fields = {{"value", scalar, {{1, -1}}, s::kNone, initialized, initialized},
                               {"next", s::classType(s::ClassId{0})},
                               {"objects", s::classType(s::ClassId{1}), {{2, 0}}},
                               {"nested", s::queueType(s::TypeId{2})},
                               {"text", {s::TypeKind::String, 0}},
                               {"opaque", {s::TypeKind::Chandle, 0}}};
        d.classes[2].name = "Interface";
        d.classes[2].interfaceClass = true;
        d.classes[3].name = "Other";
        d.classes[4].name = "Weak";
        d.classes[4].weakTarget = s::ClassId{1};
        for (std::uint32_t i = 0; i < d.classes.size(); ++i) {
            auto& type = d.classes[i];
            type.virtualMethods = {s::SubroutineId{s::kNone}};
            if (type.interfaceClass) continue;
            auto sub = method(s::ClassId{i});
            sub.classRole = s::ClassRole::Constructor;
            auto& code = sub.blocks[0].instrs;
            if (type.base != s::kNone) code.push_back(effect(s::Op::ObjectBaseInit));
            code.push_back(effect(s::Op::ObjectStageBegin));
            for (std::uint32_t f = 0; f < type.fields.size(); ++f) {
                const auto& field = type.fields[f];
                if (!field.hasInitializer) continue;
                std::vector<s::Operand> operands{s::Operand::classField({s::ClassId{i}, f})};
                s::Constant c;
                c.type = field.type;
                const auto count = s::isIntegral(c.type) ? s::integralWords(c.type.width) : 1;
                c.aWords.resize(count, 0);
                if (s::isFourState(c.type)) c.bWords.resize(count, 0);
                const s::ConstId id{static_cast<std::uint32_t>(d.constants.size())};
                d.constants.push_back(std::move(c));
                for (std::uint64_t e = 0; e < s::elementCount(field); ++e)
                    operands.push_back(s::Operand::constant(id));
                code.push_back(effect(s::Op::ObjectInit, std::move(operands)));
            }
            code.push_back(effect(s::Op::ObjectStageBody));
            code.push_back(effect(s::Op::ObjectStageEnd));
            code.push_back(effect(s::Op::Ret));
            type.constructor = s::SubroutineId{static_cast<std::uint32_t>(d.subroutines.size())};
            d.subroutines.push_back(std::move(sub));
        }
        d.virtualSlots = {{"get", s::ClassId{1}, s::SubroutineKind::Function, s::voidType()}};
        for (const auto owner : {0U, 1U}) {
            auto sub = method(s::ClassId{owner});
            sub.blocks[0].instrs = {effect(s::Op::Ret)};
            d.classes[owner].virtualMethods[0] =
                s::SubroutineId{static_cast<std::uint32_t>(d.subroutines.size())};
            d.subroutines.push_back(std::move(sub));
        }
    }
    static s::Subroutine method(s::ClassId owner) {
        s::Subroutine sub;
        sub.name = "top.method";
        sub.automatic = true;
        sub.classRole = s::ClassRole::Method;
        sub.classOwner = owner;
        sub.loc = {0, 1, 1};
        sub.params = {
            {"this", s::Direction::Input, s::classType(owner), s::kNone, s::FrameVarId{0}}};
        sub.frame = {{"this", s::classType(owner)}};
        sub.blocks.resize(1);
        return sub;
    }
};
k::ObjectToken token(const k::ObjectResult<k::ObjectPin>& pin) { return pin.value.token(); }
W raw(k::Heap& heap, k::ObjectToken object, unsigned view = 1) {
    const auto result = v::objectValue(heap, s::classType(s::ClassId{view}), object);
    EXPECT_TRUE(result);
    return result.value;
}
std::vector<k::HeapRoot> roots(k::ActivationStore& store) {
    struct Buffer {
        std::array<k::HeapRoot, 32> values;
        std::size_t count = 0;
        bool overflow = false;
    } buffer{};
    store.visitRoots(
        [](void* raw, k::HeapRoot root) noexcept {
            auto& buffer = *static_cast<Buffer*>(raw);
            if (buffer.count == buffer.values.size())
                buffer.overflow = true;
            else
                buffer.values[buffer.count++] = root;
        },
        &buffer);
    EXPECT_FALSE(buffer.overflow);
    return {buffer.values.begin(),
            buffer.values.begin() + static_cast<std::ptrdiff_t>(buffer.count)};
}
k::HeapStatus collect(k::Heap& heap, std::span<const k::HeapRoot> roots = {}) {
    // The fixture's string fields contain typed empty strings. The heap
    // requires an explicit string edge visitor even for their null token.
    return heap.collect(roots,
                        {[](void*, std::uint64_t value) noexcept { return value == 0; }, nullptr});
}
struct Memory : std::pmr::memory_resource {
    bool fail = false;
    int remaining = -1;
    unsigned blocks = 0;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (fail || remaining == 0) throw std::bad_alloc{};
        if (remaining > 0) --remaining;
        void* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++blocks;
        return result;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
        --blocks;
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ClassVmAdapter, ForwardClassIdsAndIndependentContainerIdsPreserveLayoutsAndDispatch) {
    Fixture f;
    ASSERT_TRUE(s::verify(f.d).empty());
    k::Heap heap;
    const k::HeapWord word{};
    const auto oldLayout = heap.addLayout({&word, 1});
    ASSERT_TRUE(oldLayout);
    ASSERT_TRUE(heap.addType({k::ContainerKind::Queue, oldLayout.value}));
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan.value.containers()[0].index(), 1U);
    EXPECT_EQ(plan.value.containers()[1].index(), 2U);
    const auto shape = plan.value.fieldLayout({s::ClassId{1}, 0});
    ASSERT_TRUE(shape);
    EXPECT_EQ(heap.words(shape.value), 9U);
    const auto tail = heap.describeWord(shape.value, 2);
    ASSERT_TRUE(tail);
    EXPECT_EQ(tail.value.mask, 1U);
    EXPECT_TRUE(tail.value.fourState);
    k::Rng rng{17};
    auto object = heap.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(object);
    const auto selected = heap.virtualMethod(token(object), k::ObjectClassId{1}, 0);
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected.value, f.d.classes[0].virtualMethods[0].index());
    EXPECT_EQ(heap.objectStats().live, 1U);
    auto moved = std::move(plan.value);
    EXPECT_FALSE(plan.value.valid());
    EXPECT_TRUE(moved.valid());
    EXPECT_FALSE(plan.value.fieldLayout({s::ClassId{1}, 0}));
}

TEST(ClassVmAdapter, PropertyDefaultsAndSelectionsKeepWideTwoAndFourStateElementsSeparate) {
    for (const auto width : {1U, 32U, 65U, 129U, 257U}) {
        for (const bool four : {false, true}) {
            Fixture f(four ? s::u4(width) : s::u2(width));
            k::Heap heap;
            auto plan = v::HeapPlan::compile(heap, f.d);
            ASSERT_TRUE(plan) << width << ' ' << four;
            k::Rng rng;
            auto object = heap.newObject(k::ObjectClassId{0}, rng);
            ASSERT_TRUE(object);
            auto property = plan.value.property(token(object), {s::ClassId{1}, 0});
            ASSERT_TRUE(property);
            const auto count = s::integralWords(width);
            std::vector<W> values(std::size_t{3} * count);
            ASSERT_EQ(property.value.read(values), E::None);
            for (std::size_t i = 0; i < values.size(); ++i) {
                const auto bits = std::min(64U, width - static_cast<unsigned>(i % count) * 64U);
                const auto mask = bits == 64 ? UINT64_MAX : (std::uint64_t{1} << bits) - 1;
                EXPECT_EQ(values[i].a, four ? mask : 0);
                EXPECT_EQ(values[i].b, four ? mask : 0);
            }
            auto element = property.value.element(1);
            ASSERT_TRUE(element);
            std::vector<W> input(count, {UINT64_MAX, 0xaaaaaaaaaaaaaaaaULL}), output(count);
            ASSERT_EQ(element.value.write(input), E::None);
            ASSERT_EQ(element.value.read(output), E::None);
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto bits = std::min(64U, width - i * 64U);
                const auto mask = bits == 64 ? UINT64_MAX : (std::uint64_t{1} << bits) - 1;
                EXPECT_EQ(output[i].a, (four ? UINT64_MAX : 0x5555555555555555ULL) & mask);
                EXPECT_EQ(output[i].b, four ? (0xaaaaaaaaaaaaaaaaULL & mask) : 0);
            }
            auto first = property.value.element(0);
            ASSERT_TRUE(first);
            std::vector<W> unchanged(count);
            ASSERT_EQ(first.value.read(unchanged), E::None);
            EXPECT_EQ(unchanged[0].a, values[0].a);
            EXPECT_FALSE(property.value.element(3));
        }
    }
}

TEST(ClassVmAdapter, ClassArraysTranslateOwnerTagsAndTraceRecursiveEdgesAfterSourcePinsDrop) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto owner = heap.newObject(k::ObjectClassId{1}, rng);
    auto first = heap.newObject(k::ObjectClassId{1}, rng);
    auto second = heap.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(owner);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    auto property = plan.value.property(token(owner), {s::ClassId{1}, 2});
    ASSERT_TRUE(property);
    const std::array values{raw(heap, token(first)), raw(heap, token(second)), W{}};
    ASSERT_EQ(property.value.write(values), E::None);
    std::array<W, 3> read{};
    ASSERT_EQ(property.value.read(read), E::None);
    for (unsigned i = 0; i < 3; ++i) {
        EXPECT_EQ(read[i].a, values[i].a);
        EXPECT_EQ(read[i].b, 0U);
    }
    const auto receiver = token(owner);
    owner.value.reset();
    first.value.reset();
    second.value.reset();
    ASSERT_TRUE(collect(heap));
    EXPECT_EQ(heap.objectStats().live, 3U);
    EXPECT_TRUE(heap.objectInfo(receiver));
    property.value = {};
    ASSERT_TRUE(collect(heap));
    EXPECT_EQ(heap.objectStats().live, 0U);
}

TEST(ClassVmAdapter, RetainedPropertyAndItsMovesKeepTheOriginallySelectedObject) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto a = heap.newObject(k::ObjectClassId{0}, rng), b = heap.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    const auto original = token(a);
    auto selected = plan.value.property(original, {s::ClassId{0}, 0});
    ASSERT_TRUE(selected);
    a.value.reset();
    auto moved = std::move(selected.value);
    W value{13, 0}, output{};
    EXPECT_EQ(selected.value.read({&output, 1}), E::InvalidField);
    EXPECT_EQ(selected.value.words(), 0U);
    ASSERT_TRUE(collect(heap));
    EXPECT_TRUE(heap.objectInfo(original));
    EXPECT_EQ(moved.owner(), original);
    ASSERT_EQ(moved.write({&value, 1}), E::None);
    ASSERT_EQ(moved.read({&output, 1}), E::None);
    EXPECT_EQ(output.a, 13U);
    auto other = plan.value.property(token(b), {s::ClassId{0}, 0});
    ASSERT_TRUE(other);
    ASSERT_EQ(other.value.read({&output, 1}), E::None);
    EXPECT_EQ(output.a, 0U);
    moved = std::move(other.value);
    ASSERT_TRUE(collect(heap));
    EXPECT_FALSE(heap.objectInfo(original));
}

TEST(ClassVmAdapter, TypedImportRejectsForeignOwnersWrongViewsStaleTokensAndNonzeroBWords) {
    Fixture f;
    k::Heap left, right;
    auto aPlan = v::HeapPlan::compile(left, f.d), bPlan = v::HeapPlan::compile(right, f.d);
    ASSERT_TRUE(aPlan);
    ASSERT_TRUE(bPlan);
    k::Rng rng;
    auto object = left.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(object);
    const auto a = token(object);
    auto rightObject = right.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(rightObject);
    EXPECT_FALSE(
        v::objectValue(right, s::classType(s::ClassId{1}), {token(rightObject).bits, &left}));
    EXPECT_FALSE(v::objectValue(right, s::classType(s::ClassId{1}), a));
    EXPECT_FALSE(v::objectValue(left, s::classType(s::ClassId{3}), a));
    EXPECT_FALSE(v::localObject(left, s::classType(s::ClassId{1}), left.objectWord(a)));
    EXPECT_FALSE(v::localObject(left, s::u2(64), {a.bits, 0}));
    EXPECT_FALSE(v::localObject(left, s::classType(s::ClassId{99}), {}));
    EXPECT_TRUE(v::localObject(left, s::classType(s::ClassId{1}), {}));
    object.value.reset();
    ASSERT_TRUE(collect(left));
    EXPECT_FALSE(v::localObject(left, s::classType(s::ClassId{1}), {a.bits, 0}));
}

TEST(ClassVmAdapter, MalformedLateDescriptorsAndManagedEventFieldsFailBeforeSchemaRegistration) {
    for (unsigned fault = 0; fault < 4; ++fault) {
        Fixture f;
        if (fault == 0) f.d.classes[4].fields = {{"bad", s::classType(s::ClassId{99})}};
        if (fault == 1) f.d.classes[4].fields = {{"event", {s::TypeKind::Event, 0}}};
        if (fault == 2) f.d.types[2].value = s::queueType(s::TypeId{2});
        if (fault == 3) f.d.classes[1].base = s::ClassId{0};
        k::Heap heap;
        auto plan = v::HeapPlan::compile(heap, f.d);
        EXPECT_FALSE(plan) << fault;
        const k::HeapWord word{};
        const auto first = heap.addLayout({&word, 1});
        ASSERT_TRUE(first);
        EXPECT_EQ(first.value.index(), 0U);
        EXPECT_EQ(heap.objectStats().entries, 0U);
    }
}

TEST(ClassVmAdapter, FailedClassArrayWritePublishesNoPrefixAndNoChangeNotification) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto object = heap.newObject(k::ObjectClassId{1}, rng);
    ASSERT_TRUE(object);
    auto property = plan.value.property(token(object), {s::ClassId{1}, 2});
    ASSERT_TRUE(property);
    unsigned changes = 0;
    heap.setChangeHook(
        {[](void* raw, k::HeapRoot) noexcept { ++*static_cast<unsigned*>(raw); }, &changes});
    std::array input{raw(heap, token(object)), W{}, W{123, 1}};
    EXPECT_EQ(property.value.write(input), E::InvalidValue);
    std::array<W, 3> output{};
    ASSERT_EQ(property.value.read(output), E::None);
    EXPECT_EQ(output[0].a, 0U);
    EXPECT_EQ(changes, 0U);
}

TEST(ClassVmAdapter, ConstructorConstPermissionsAndReadonlyTargetsStayInTheKernel) {
    Fixture f(s::u2(32), true);
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto object = heap.newObject(k::ObjectClassId{1}, rng);
    ASSERT_TRUE(object);
    auto property = plan.value.property(token(object), {s::ClassId{1}, 0});
    auto readonly = plan.value.property(token(object), {s::ClassId{1}, 0}, true);
    ASSERT_TRUE(property);
    ASSERT_TRUE(readonly);
    const std::array input{W{1, 0}, W{2, 0}, W{3, 0}};
    EXPECT_EQ(property.value.write(input), E::ConstWrite);
    auto stage = heap.beginObjectStage(token(object), k::ObjectClassId{1});
    ASSERT_TRUE(stage);
    EXPECT_EQ(readonly.value.write(input, &stage.value), E::ConstWrite);
    EXPECT_EQ(property.value.write(input, &stage.value), E::None);
    EXPECT_EQ(property.value.write(input, &stage.value), E::ConstWrite);
    EXPECT_EQ(heap.beginObjectBody(stage.value), E::None);
    EXPECT_EQ(heap.finishObjectStage(stage.value), E::None);
    EXPECT_TRUE(heap.objectInfo(token(object)).value.constructed);
}

TEST(ClassVmAdapter, ArgumentTransfersPinOnlyClassValuesAndRollBackFailedGroups) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto first = heap.newObject(k::ObjectClassId{0}, rng),
         second = heap.newObject(k::ObjectClassId{1}, rng);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto a = token(first), b = token(second);
    v::ObjectTransfers transfers(heap);
    const std::array initial{raw(heap, a)};
    ASSERT_EQ(transfers.retain(s::classType(s::ClassId{1}), initial), E::None);
    const std::array<s::Param, 4> params{
        {{"value", s::Direction::Input, s::classType(s::ClassId{1})},
         {"reference", s::Direction::Ref, s::classType(s::ClassId{1})},
         {"opaque", s::Direction::Input, {s::TypeKind::Chandle, 0}},
         {"invalid", s::Direction::Input, s::classType(s::ClassId{1})}}};
    std::array<std::vector<W>, 4> args{
        {{raw(heap, b)}, {{a.bits, UINT64_MAX}}, {{a.bits, 0}}, {{b.bits, 1}}}};
    EXPECT_EQ(transfers.retainArguments(params, args), E::InvalidValue);
    EXPECT_EQ(transfers.size(), 1U);
    first.value.reset();
    second.value.reset();
    ASSERT_TRUE(collect(heap));
    EXPECT_TRUE(heap.objectInfo(a));
    EXPECT_FALSE(heap.objectInfo(b));
    transfers.clear();
    ASSERT_TRUE(collect(heap));
    EXPECT_FALSE(heap.objectInfo(a));
}

TEST(ClassVmAdapter, NullTransfersAndWrongArgumentCountsDoNotAcquireRoots) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    v::ObjectTransfers transfers(heap);
    const std::array<W, 3> nulls{};
    EXPECT_EQ(transfers.retain(s::classType(s::ClassId{1}), nulls), E::None);
    EXPECT_EQ(transfers.size(), 0U);
    const std::array<s::Param, 1> params{
        {{"object", s::Direction::Input, s::classType(s::ClassId{1})}}};
    EXPECT_EQ(transfers.retainArguments(params, {}), E::InvalidValue);
}

TEST(ClassVmAdapter, TransferAndPropertyAllocationFailuresReleaseUnpublishedPinsAndPreserveValues) {
    Fixture f;
    Memory memory;
    k::Heap heap(&memory);
    auto plan = v::HeapPlan::compile(heap, f.d, &memory);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto object = heap.newObject(k::ObjectClassId{1}, rng);
    ASSERT_TRUE(object);
    auto property = plan.value.property(token(object), {s::ClassId{1}, 2});
    ASSERT_TRUE(property);
    v::ObjectTransfers transfers(heap);
    const std::array input{raw(heap, token(object))};
    memory.fail = true;
    EXPECT_EQ(transfers.retain(s::classType(s::ClassId{1}), input), E::ResourceLimit);
    EXPECT_EQ(transfers.size(), 0U);
    std::array<W, 3> out{{{91, 0}, {92, 0}, {93, 0}}};
    EXPECT_EQ(property.value.read(out), E::ResourceLimit);
    EXPECT_EQ(out[0].a, 91U);
    EXPECT_EQ(property.value.write(out), E::ResourceLimit);
    memory.fail = false;
    ASSERT_EQ(property.value.read(out), E::None);
    EXPECT_EQ(out[0].a, 0U);
    property.value = {};
    object.value.reset();
    ASSERT_TRUE(collect(heap));
    EXPECT_EQ(heap.objectStats().live, 0U);
}

TEST(ClassVmAdapter, InitialPlanAllocationFailureCreatesNoObjectsOrClassRegistration) {
    Fixture f;
    Memory memory;
    k::Heap heap(&memory);
    memory.fail = true;
    auto plan = v::HeapPlan::compile(heap, f.d, &memory);
    EXPECT_FALSE(plan);
    EXPECT_EQ(plan.status.error, k::HeapError::ResourceLimit);
    memory.fail = false;
    const k::HeapWord word{};
    const auto first = heap.addLayout({&word, 1});
    ASSERT_TRUE(first);
    EXPECT_EQ(first.value.index(), 0U);
    EXPECT_EQ(heap.addClasses({}), E::None);
}

TEST(ClassVmAdapter, ScopedAndCapturedObjectFieldsRetainTheirViewAndOriginalActivation) {
    for (const auto join : {v::CaptureJoin::All, v::CaptureJoin::Any, v::CaptureJoin::None}) {
        Fixture f;
        k::Context context;
        auto& heap = context.heap();
        auto plan = v::HeapPlan::compile(heap, f.d);
        ASSERT_TRUE(plan);
        s::FrameVar row;
        row.type = s::classType(s::ClassId{1});
        row.dims = {{2, 0}};
        row.home = s::FrameHome::Scoped;
        row.activationScope = s::ActivationScopeId{0};
        const std::vector parentFrame{row};
        s::ActivationInfo info;
        info.scopes = {{s::kNone, {s::FrameVarId{0}}}};
        info.root = s::ActivationScopeId{0};
        auto parentPlan = v::ActivationPlan::compile(context.activations(), parentFrame, info);
        ASSERT_TRUE(parentPlan);
        auto parent = v::ActivationEnvironment::root(context.activations(), parentPlan.value);
        ASSERT_TRUE(parent);
        const auto binding = parent.value->binding(s::FrameVarId{0});
        ASSERT_TRUE(binding);
        k::Rng rng;
        auto object = heap.newObject(k::ObjectClassId{0}, rng);
        auto wrong = heap.newObject(k::ObjectClassId{3}, rng);
        ASSERT_TRUE(object);
        ASSERT_TRUE(wrong);
        const auto original = token(object);
        auto baseObject = heap.newObject(k::ObjectClassId{1}, rng);
        ASSERT_TRUE(baseObject);
        auto baseValue = raw(heap, token(baseObject));
        EXPECT_EQ(context.activations().write(binding.value, 0, {&baseValue, 1}),
                  k::ActivationError::None);
        const W nullValue{};
        EXPECT_EQ(context.activations().write(binding.value, 0, {&nullValue, 1}),
                  k::ActivationError::None);
        baseObject.value.reset();
        auto value = raw(heap, original);
        EXPECT_EQ(context.activations().write(binding.value, 2, {&value, 1}),
                  k::ActivationError::None);
        const W invalid{token(wrong).bits, 0};
        EXPECT_EQ(context.activations().write(binding.value, 2, {&invalid, 1}),
                  k::ActivationError::InvalidValue);
        row.home = s::FrameHome::Capture;
        row.activationScope = s::kNone;
        row.captureSource = s::FrameVarId{0};
        const std::vector childFrame{row};
        s::ActivationInfo childInfo;
        childInfo.extents = {s::ActivationScopeId{0}};
        auto childPlan = v::ActivationPlan::compile(context.activations(), childFrame, childInfo);
        ASSERT_TRUE(childPlan);
        const std::array plans{&childPlan.value};
        auto children = v::ActivationEnvironment::children(context.activations(), parentPlan.value,
                                                           parent.value.get(), plans, join);
        ASSERT_TRUE(children);
        parent.value->finish();
        object.value.reset();
        wrong.value.reset();
        ASSERT_TRUE(collect(heap, roots(context.activations())));
        EXPECT_TRUE(heap.objectInfo(original));
        const auto selected = children.value[0]->binding(s::FrameVarId{0});
        ASSERT_TRUE(selected);
        W observed{};
        EXPECT_EQ(context.activations().read(selected.value, 2, {&observed, 1}),
                  k::ActivationError::None);
        EXPECT_EQ(observed.a, original.bits);
        EXPECT_EQ(observed.b, 0U);
        children.value[0]->finish();
        ASSERT_TRUE(collect(heap, roots(context.activations())));
        EXPECT_FALSE(heap.objectInfo(original));
    }
}

TEST(ClassVmAdapter, NestedClassContainersKeepObjectAliasesWhileCopyingContainerValues) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto object = heap.newObject(k::ObjectClassId{1}, rng),
         child = heap.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(object);
    ASSERT_TRUE(child);
    const auto childToken = token(child);
    const auto word = heap.objectWord(childToken);
    const auto array = heap.fromValues(plan.value.containers()[1], {&word, 1});
    ASSERT_TRUE(array);
    const W nested{array.value.bits, 0};
    const auto queue = heap.fromValues(plan.value.containers()[2], {&nested, 1});
    ASSERT_TRUE(queue);
    const W input{queue.value.bits, 0};
    auto property = plan.value.property(token(object), {s::ClassId{1}, 3});
    ASSERT_TRUE(property);
    ASSERT_EQ(property.value.write({&input, 1}), E::None);
    W copiedQueue{};
    ASSERT_EQ(property.value.read({&copiedQueue, 1}), E::None);
    EXPECT_NE(copiedQueue.a, input.a);
    W copiedArray{}, copiedObject{};
    ASSERT_TRUE(heap.read(plan.value.containers()[2], {copiedQueue.a}, 0, {&copiedArray, 1}));
    EXPECT_NE(copiedArray.a, array.value.bits);
    ASSERT_TRUE(heap.read(plan.value.containers()[1], {copiedArray.a}, 0, {&copiedObject, 1}));
    EXPECT_EQ(heap.wordObject(copiedObject, k::ObjectClassId{1}).value, childToken);
    object.value.reset();
    child.value.reset();
    ASSERT_TRUE(collect(heap));
    EXPECT_TRUE(heap.objectInfo(childToken));
    EXPECT_EQ(heap.stats().containers, 2U);
    property.value = {};
    ASSERT_TRUE(collect(heap));
    EXPECT_EQ(heap.objectStats().live, 0U);
}
TEST(ClassVmAdapter, SuccessfulTransfersDoNotTreatOpaqueWordsOrBorrowedRefsAsObjects) {
    Fixture f;
    k::Heap heap;
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto a = heap.newObject(k::ObjectClassId{0}, rng), b = heap.newObject(k::ObjectClassId{1}, rng);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    const auto first = token(a), second = token(b);
    const std::array<s::Param, 3> parameters{
        {{"objects", s::Direction::Input, s::classType(s::ClassId{1})},
         {"ref", s::Direction::ConstRef, s::classType(s::ClassId{1})},
         {"opaque", s::Direction::Input, {s::TypeKind::Chandle, 0}}}};
    const std::array<std::vector<W>, 3> args{
        {{raw(heap, first), W{}}, {{second.bits, 0}}, {{second.bits, 0}}}};
    v::ObjectTransfers transfers(heap);
    ASSERT_EQ(transfers.retainArguments(parameters, args), E::None);
    EXPECT_EQ(transfers.size(), 1U);
    a.value.reset();
    b.value.reset();
    ASSERT_TRUE(collect(heap));
    EXPECT_TRUE(heap.objectInfo(first));
    EXPECT_FALSE(heap.objectInfo(second));
    transfers.clear();
    ASSERT_TRUE(collect(heap));
    EXPECT_FALSE(heap.objectInfo(first));
    EXPECT_EQ(transfers.retain(s::u2(64), {}), E::InvalidClass);
    EXPECT_EQ(transfers.retain(s::classType(s::ClassId{99}), {}), E::InvalidClass);
}

TEST(ClassVmAdapter, StringFieldsPublishTypedEdgesAndOpaqueFieldsDoNotRetainObjects) {
    Fixture f;
    k::Context context;
    auto& heap = context.heap();
    auto plan = v::HeapPlan::compile(heap, f.d);
    ASSERT_TRUE(plan);
    k::Rng rng;
    auto object = heap.newObject(k::ObjectClassId{1}, rng),
         opaque = heap.newObject(k::ObjectClassId{0}, rng);
    ASSERT_TRUE(object);
    ASSERT_TRUE(opaque);
    const auto unrooted = token(opaque);
    auto text = plan.value.property(token(object), {s::ClassId{1}, 4});
    auto handle = plan.value.property(token(object), {s::ClassId{1}, 5});
    ASSERT_TRUE(text);
    ASSERT_TRUE(handle);
    const auto string = context.strings().intern("kept by an object property");
    const W value{string, 0}, bits{unrooted.bits, 0};
    ASSERT_EQ(text.value.write({&value, 1}), E::None);
    ASSERT_EQ(handle.value.write({&bits, 1}), E::None);
    opaque.value.reset();
    const k::HeapStringRoots strings{[](void* raw, std::uint64_t token) noexcept {
                                         return static_cast<k::Strings*>(raw)->mark(token) ==
                                                k::StringError::None;
                                     },
                                     &context.strings()};
    ASSERT_EQ(context.strings().beginCollection({}), k::StringError::None);
    ASSERT_TRUE(heap.collect({}, strings));
    context.strings().finishCollection();
    EXPECT_FALSE(heap.objectInfo(unrooted));
    EXPECT_EQ(context.strings().get(string), "kept by an object property");
    text.value = {};
    handle.value = {};
    object.value.reset();
    ASSERT_EQ(context.strings().beginCollection({}), k::StringError::None);
    ASSERT_TRUE(heap.collect({}, strings));
    context.strings().finishCollection();
    EXPECT_TRUE(context.strings().get(string).empty());
}

TEST(ClassVmAdapter, EmptyAndContainerOnlyPlansPreserveExistingHeapRegistration) {
    for (const bool containers : {false, true}) {
        s::Design d;
        if (containers) {
            s::TypeEntry type;
            type.kind = s::TypeEntryKind::Queue;
            type.value = s::u2(32);
            d.types.push_back(type);
        }
        k::Heap heap;
        auto plan = v::HeapPlan::compile(heap, d);
        ASSERT_TRUE(plan);
        EXPECT_TRUE(plan.value.valid());
        EXPECT_EQ(plan.value.containers().size(), containers ? 1U : 0U);
        EXPECT_EQ(heap.objectStats().entries, 0U);
        // An ordinary-container plan does not install an empty class schema.
        EXPECT_EQ(heap.addClasses({}), E::None);
    }
}

TEST(ClassVmAdapter, EverySchemaAllocationFailureLeavesNoPublishedClassOrLiveSourceValue) {
    Fixture f;
    bool completed = false;
    unsigned failed = 0;
    for (int budget = 0; budget < 512 && !completed; ++budget) {
        Memory memory;
        {
            k::Heap heap(&memory);
            memory.remaining = budget;
            auto plan = v::HeapPlan::compile(heap, f.d, &memory);
            memory.remaining = -1;
            EXPECT_EQ(heap.objectStats().live, 0U);
            EXPECT_EQ(heap.stats().cells, 0U);
            EXPECT_EQ(heap.stats().containers, 0U);
            if (plan)
                completed = true;
            else {
                ++failed;
                EXPECT_EQ(plan.status.error, k::HeapError::ResourceLimit) << budget;
                EXPECT_FALSE(plan.value.valid());
                // A failed plan may leave unused forward-referencing layout
                // rows, so retry the same complete schema. Success also proves
                // that the failed attempt never installed a class table.
                auto retry = v::HeapPlan::compile(heap, f.d, &memory);
                EXPECT_TRUE(retry) << budget;
            }
        }
        EXPECT_EQ(memory.blocks, 0U) << budget;
    }
    EXPECT_TRUE(completed);
    EXPECT_GT(failed, 20U);
}
}  // namespace
