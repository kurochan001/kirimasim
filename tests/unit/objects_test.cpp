// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/heap.h"

#include <algorithm>
#include <array>
#include <memory_resource>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "kirimasim/valsem/objects.h"
#include "gtest/gtest.h"

namespace {
using namespace kirimasim::kernel;
namespace vs = kirimasim::valsem;

HeapLayoutId bits(Heap& heap, unsigned width = 32, bool four = false) {
    std::vector<HeapWord> words;
    for (unsigned offset = 0; offset < width; offset += 64) {
        const auto n = std::min(64U, width - offset);
        const auto mask = n == 64 ? UINT64_MAX : (std::uint64_t{1} << n) - 1;
        words.push_back({mask, four, four ? W4{mask, mask} : W4{}, kNone});
    }
    const auto layout = heap.addLayout(words);
    EXPECT_TRUE(layout);
    return layout.value;
}
HeapLayoutId handle(Heap& heap, ObjectClassId type) {
    HeapWord word;
    word.object = type;
    const auto layout = heap.addLayout({&word, 1});
    EXPECT_TRUE(layout);
    return layout.value;
}
HeapLayoutId nested(Heap& heap, HeapTypeId type) {
    HeapWord word;
    word.nested = type;
    const auto layout = heap.addLayout({&word, 1});
    EXPECT_TRUE(layout);
    return layout.value;
}
std::vector<W4> encode(const vs::Value& value) {
    std::vector<std::uint64_t> a, b;
    vs::toWords(value, a, b);
    std::vector<W4> result;
    result.reserve(a.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        result.push_back({a[i], b[i]});
    return result;
}
vs::Value decode(std::span<const W4> values, unsigned width) {
    std::vector<std::uint64_t> a, b;
    a.reserve(values.size());
    b.reserve(values.size());
    for (const auto value : values) {
        a.push_back(value.a);
        b.push_back(value.b);
    }
    return vs::fromWords(a, b, width, false);
}
ObjectRef ref(Heap& heap, ObjectToken value, unsigned declaring, unsigned field = 0) {
    const auto result = heap.objectRef(value, {ObjectClassId{declaring}, field});
    EXPECT_TRUE(result);
    return result.value;
}
std::vector<W4> read(Heap& heap, ObjectRef target) {
    std::vector<W4> output(target.count);
    EXPECT_EQ(heap.readObject(target, output), ObjectError::None);
    return output;
}
ObjectError write(Heap& heap, ObjectRef target, std::uint64_t value,
                  const ObjectStage* stage = nullptr) {
    const W4 word{value, 0};
    return heap.writeObject(target, {&word, 1}, stage);
}
void simple(Heap& heap, HeapLayoutId layout) {
    const ObjectField field{layout};
    ObjectClass type;
    type.name = "Cell";
    type.fields = {&field, 1};
    ASSERT_EQ(heap.addClasses({&type, 1}), ObjectError::None);
}
vs::ObjectValue modelBits(vs::Value value) {
    vs::ObjectValue result;
    result.bits = std::move(value);
    return result;
}
vs::ObjectValue modelHandle(std::uint64_t id) {
    vs::ObjectValue result;
    result.kind = vs::ObjectValue::Kind::Handle;
    result.handle = id;
    return result;
}
vs::ObjectValue modelSequence(std::vector<vs::ObjectValue> values) {
    vs::ObjectValue result;
    result.kind = vs::ObjectValue::Kind::Sequence;
    result.elements = std::move(values);
    return result;
}

TEST(Objects, DefaultsNormalizationIdentityDispatchAndCopyAcrossWidthsAndStates) {
    for (const unsigned width : {1U, 32U, 65U, 129U, 257U}) {
        for (const bool four : {false, true}) {
            SCOPED_TRACE(::testing::Message() << width << ' ' << four);
            Heap heap;
            const auto layout = bits(heap, width, four);
            const ObjectField field{layout};
            const std::array<std::uint32_t, 1> baseMethods{7}, derivedMethods{9};
            std::array<ObjectClass, 2> types{};
            types[0].name = "Derived";
            types[0].base = ObjectClassId{1};
            types[0].fields = {&field, 1};
            types[0].virtualMethods = derivedMethods;
            types[1].name = "Base";
            types[1].fields = {&field, 1};
            types[1].virtualMethods = baseMethods;
            ASSERT_EQ(heap.addClasses(types), ObjectError::None);
            Rng parent{13}, expected = parent;
            const auto seed = expected.next();
            auto made = heap.newObject(ObjectClassId{0}, parent);
            ASSERT_TRUE(made);
            const auto object = made.value.token();
            EXPECT_EQ(parent.state, expected.state);
            EXPECT_EQ(heap.objectInfo(object).value.randomState, seed);
            EXPECT_EQ(heap.objectId({}).value, 0U);
            EXPECT_EQ(heap.virtualMethod(object, ObjectClassId{1}, 0).value, 9U);
            EXPECT_EQ(heap.virtualMethod(object, ObjectClassId{1}, 1).error,
                      ObjectError::InvalidMethod);
            const auto initial = vs::filled(four ? vs::Logic::X : vs::Logic::Zero, width, false);
            EXPECT_EQ(decode(read(heap, ref(heap, object, 0)), width), initial);
            EXPECT_EQ(decode(read(heap, ref(heap, object, 1)), width), initial);
            auto source = vs::filled(vs::Logic::Z, width, false);
            for (unsigned i = 0; i < width; i += 3)
                source.bits[i] = vs::Logic::One;
            ASSERT_EQ(heap.writeObject(ref(heap, object, 0), encode(source)), ObjectError::None);
            EXPECT_EQ(decode(read(heap, ref(heap, object, 0)), width),
                      four ? source : vs::to2(source));
            EXPECT_EQ(decode(read(heap, ref(heap, object, 1)), width), initial);
            auto copy = heap.copyObject(object);
            ASSERT_TRUE(copy);
            EXPECT_NE(copy.value.token(), object);
            EXPECT_EQ(heap.objectInfo(copy.value.token()).value.dynamicClass, ObjectClassId{0});
            EXPECT_EQ(heap.objectInfo(copy.value.token()).value.randomState, seed);
            EXPECT_EQ(heap.objectInfo(copy.value.token()).value.ordinal, 2U);
            EXPECT_EQ(heap.objectId(copy.value.token()).value, 2U);
            EXPECT_EQ(parent.state, expected.state);
            EXPECT_TRUE(heap.objectInfo(copy.value.token()).value.constructed);
            ASSERT_EQ(heap.writeObject(ref(heap, object, 0), encode(initial)), ObjectError::None);
            EXPECT_EQ(decode(read(heap, ref(heap, copy.value.token(), 0)), width),
                      four ? source : vs::to2(source));
        }
    }
}

TEST(Objects, ShallowCopiesAliasObjectFieldsAndDeepCopyContainersInOneCollectibleGraph) {
    Heap heap;
    const auto scalar = bits(heap);
    const auto objectLayout = handle(heap, ObjectClassId{0});
    const auto queue = heap.addType({ContainerKind::Queue, objectLayout, 0});
    ASSERT_TRUE(queue);
    const auto queueLayout = nested(heap, queue.value);
    const std::array<ObjectField, 3> fields{{{scalar}, {objectLayout}, {queueLayout}}};
    std::array<ObjectClass, 2> types{};
    types[0].name = "Node";
    types[0].fields = fields;
    types[1].name = "WeakNode";
    types[1].weakTarget = ObjectClassId{0};
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    Rng rng;
    auto a = heap.newObject(ObjectClassId{0}, rng), b = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    const auto at = a.value.token(), bt = b.value.token();
    vs::ObjectModel model;
    const auto initial = [&] {
        return std::vector<vs::ObjectValue>{modelBits(vs::fromUint(0, 32, false)), modelHandle(0),
                                            modelSequence({})};
    };
    const auto ma = model.create(0, initial()), mb = model.create(0, initial());
    const auto aw = heap.objectWord(at), bw = heap.objectWord(bt);
    ASSERT_EQ(heap.writeObject(ref(heap, at, 0, 1), {&bw, 1}), ObjectError::None);
    ASSERT_EQ(heap.writeObject(ref(heap, bt, 0, 1), {&aw, 1}), ObjectError::None);
    ASSERT_TRUE(model.write(ma, 1, modelHandle(mb)));
    ASSERT_TRUE(model.write(mb, 1, modelHandle(ma)));
    const auto q = heap.fromValues(queue.value, {&bw, 1});
    ASSERT_TRUE(q);
    ASSERT_EQ(write(heap, ref(heap, at, 0, 2), q.value.bits), ObjectError::None);
    ASSERT_TRUE(model.write(ma, 2, modelSequence({modelHandle(mb)})));
    const auto parentState = rng.state;
    auto copy = heap.copyObject(at);
    ASSERT_TRUE(copy);
    const auto ct = copy.value.token();
    const auto mc = model.copy(ma);
    EXPECT_EQ(rng.state, parentState);
    EXPECT_EQ(heap.objectId(ct).value, mc);
    auto copiedQueue = ContainerToken{read(heap, ref(heap, ct, 0, 2))[0].a};
    EXPECT_NE(copiedQueue.bits, read(heap, ref(heap, at, 0, 2))[0].a);
    W4 value;
    ASSERT_TRUE(heap.read(queue.value, copiedQueue, 0, {&value, 1}));
    EXPECT_EQ(heap.wordObject(value, ObjectClassId{0}).value, bt);
    ASSERT_EQ(write(heap, ref(heap, bt, 0), 19), ObjectError::None);
    ASSERT_TRUE(model.write(mb, 0, modelBits(vs::fromUint(19, 32, false))));
    const auto peer = heap.wordObject(read(heap, ref(heap, ct, 0, 1))[0], ObjectClassId{0});
    ASSERT_TRUE(peer);
    EXPECT_EQ(decode(read(heap, ref(heap, peer.value, 0)), 32), model.read(mb, 0).bits);
    auto weak = heap.newWeakObject(ObjectClassId{1}, at, rng);
    ASSERT_TRUE(weak);
    const auto mw = model.weak(1, ma);
    // Break the back edge; only the copied container and property keep b.
    const W4 null{};
    ASSERT_EQ(heap.writeObject(ref(heap, bt, 0, 1), {&null, 1}), ObjectError::None);
    ASSERT_TRUE(model.write(mb, 1, modelHandle(0)));
    a.value.reset();
    b.value.reset();
    copy.value.reset();
    const std::array roots{HeapRoot::object(ct), HeapRoot::object(weak.value.token())};
    const std::array modelRoots{mc, mw};
    ASSERT_TRUE(heap.collect(roots));
    model.collect(modelRoots);
    EXPECT_EQ(heap.objectInfo(at).error, ObjectError::InvalidToken);
    EXPECT_FALSE(model.live(ma));
    EXPECT_TRUE(model.live(mb));
    EXPECT_EQ(heap.objectStats().live, 3U);
    EXPECT_EQ(heap.lockWeakObject(weak.value.token()).value.token().bits, 0U);
    EXPECT_EQ(model.lock(mw), 0U);
    weak.value.reset();
    ASSERT_TRUE(heap.collect({}));
    model.collect({});
    EXPECT_EQ(heap.objectStats().live, 0U);
    EXPECT_EQ(heap.stats().containers, 0U);
    EXPECT_EQ(heap.stats().cells, 0U);
}

TEST(Objects, UnreachableObjectContainerCyclesAreCollectedWithoutGuessingIntegerRoots) {
    Heap heap;
    const auto objectLayout = handle(heap, ObjectClassId{0});
    const auto queue = heap.addType({ContainerKind::Queue, objectLayout, 0});
    ASSERT_TRUE(queue);
    const auto queueLayout = nested(heap, queue.value);
    const std::array<ObjectField, 2> fields{{{objectLayout}, {queueLayout}}};
    ObjectClass type;
    type.name = "Node";
    type.fields = fields;
    ASSERT_EQ(heap.addClasses({&type, 1}), ObjectError::None);
    Rng rng;
    auto a = heap.newObject(ObjectClassId{0}, rng), b = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    const auto at = a.value.token(), bt = b.value.token();
    const auto aw = heap.objectWord(at), bw = heap.objectWord(bt);
    auto values = heap.fromValues(queue.value, {&bw, 1});
    ASSERT_TRUE(values);
    ASSERT_EQ(write(heap, ref(heap, at, 0, 1), values.value.bits), ObjectError::None);
    ASSERT_EQ(heap.writeObject(ref(heap, bt, 0), {&aw, 1}), ObjectError::None);
    vs::ObjectModel model;
    const auto ma = model.create(0, {modelHandle(0), modelSequence({})});
    const auto mb = model.create(0, {modelHandle(ma), modelSequence({})});
    ASSERT_TRUE(model.write(ma, 1, modelSequence({modelHandle(mb)})));
    const auto scalar = bits(heap, 64);
    auto integers = heap.addType({ContainerKind::DynamicArray, scalar, 0});
    ASSERT_TRUE(integers);
    const W4 coincidence{at.bits, 0};
    auto raw = heap.fromValues(integers.value, {&coincidence, 1});
    ASSERT_TRUE(raw);
    a.value.reset();
    b.value.reset();
    const auto live = HeapRoot::object(at);
    ASSERT_TRUE(heap.collect({&live, 1}));
    EXPECT_EQ(heap.objectStats().live, 2U);
    // The first collection also drops the unrelated integer container.
    raw = heap.fromValues(integers.value, {&coincidence, 1});
    ASSERT_TRUE(raw);
    const auto root = HeapRoot::container(raw.value);
    ASSERT_TRUE(heap.collect({&root, 1}));
    model.collect({});
    EXPECT_EQ(heap.objectStats().live, 0U);
    EXPECT_FALSE(model.live(ma));
    EXPECT_FALSE(model.live(mb));
    EXPECT_EQ(heap.objectInfo(at).error, ObjectError::InvalidToken);
    EXPECT_EQ(heap.objectInfo(bt).error, ObjectError::InvalidToken);
}

TEST(Objects, ConstructorStagesGuardConstWritesAndPreserveSafeEscapedDerivedDefaults) {
    Heap heap;
    const auto integer = bits(heap), logic = bits(heap, 65, true);
    const std::array<ObjectField, 3> baseFields{
        {{integer}, {integer, true, true}, {integer, true, false}}};
    const ObjectField derivedField{logic, true, true};
    std::array<ObjectClass, 2> types{};
    types[0].name = "Base";
    types[0].fields = baseFields;
    types[1].name = "Derived";
    types[1].base = ObjectClassId{0};
    types[1].fields = {&derivedField, 1};
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    Rng rng;
    auto value = heap.newObject(ObjectClassId{1}, rng);
    ASSERT_TRUE(value);
    const auto token = value.value.token();
    EXPECT_EQ(heap.beginObjectStage(token, ObjectClassId{1}).error, ObjectError::InvalidStage);
    auto stage = heap.beginObjectStage(token, ObjectClassId{0});
    ASSERT_TRUE(stage);
    EXPECT_EQ(heap.beginObjectStage(token, ObjectClassId{0}).error, ObjectError::InvalidStage);
    EXPECT_EQ(heap.beginObjectBody(stage.value), ObjectError::InvalidStage);
    EXPECT_EQ(write(heap, ref(heap, token, 0, 2), 2, &stage.value), ObjectError::ConstWrite);
    ASSERT_EQ(write(heap, ref(heap, token, 0, 1), 7, &stage.value), ObjectError::None);
    EXPECT_EQ(write(heap, ref(heap, token, 0, 1), 7, &stage.value), ObjectError::ConstWrite);
    EXPECT_EQ(decode(read(heap, ref(heap, token, 1)), 65), vs::filled(vs::Logic::X, 65, false));
    ASSERT_EQ(heap.beginObjectBody(stage.value), ObjectError::None);
    ASSERT_EQ(write(heap, ref(heap, token, 0, 2), 9, &stage.value), ObjectError::None);
    EXPECT_EQ(write(heap, ref(heap, token, 0, 2), 10, &stage.value), ObjectError::ConstWrite);
    ASSERT_EQ(heap.finishObjectStage(stage.value), ObjectError::None);
    EXPECT_FALSE(heap.objectInfo(token).value.constructed);
    auto derived = heap.beginObjectStage(token, ObjectClassId{1});
    ASSERT_TRUE(derived);
    EXPECT_EQ(write(heap, ref(heap, token, 0, 2), 11, &derived.value), ObjectError::ConstWrite);
    const auto initialized = encode(vs::fromUint(13, 65, false));
    ASSERT_EQ(heap.writeObject(ref(heap, token, 1), initialized, &derived.value),
              ObjectError::None);
    ASSERT_EQ(heap.beginObjectBody(derived.value), ObjectError::None);
    auto readonly = heap.objectRef(token, {ObjectClassId{0}, 0}, true);
    ASSERT_TRUE(readonly);
    EXPECT_EQ(write(heap, readonly.value, 17, &derived.value), ObjectError::ConstWrite);
    ASSERT_EQ(write(heap, ref(heap, token, 0), 17), ObjectError::None);
    ASSERT_EQ(heap.finishObjectStage(derived.value), ObjectError::None);
    EXPECT_TRUE(heap.objectInfo(token).value.constructed);
    EXPECT_EQ(read(heap, ref(heap, token, 0))[0].a, 17U);
    EXPECT_EQ(write(heap, ref(heap, token, 0, 2), 20), ObjectError::ConstWrite);
}

TEST(Objects, AbandonedConstructionKeepsEscapedEffectsAndCannotReuseItsCapability) {
    Heap heap;
    simple(heap, bits(heap));
    Rng rng;
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    auto stage = heap.beginObjectStage(value.value.token(), ObjectClassId{0});
    ASSERT_TRUE(stage);
    ASSERT_EQ(heap.beginObjectBody(stage.value), ObjectError::None);
    ASSERT_EQ(write(heap, ref(heap, value.value.token(), 0), 23), ObjectError::None);
    const auto escaped = value.value.token();
    value.value.reset();
    ASSERT_TRUE(heap.collect({}));
    EXPECT_EQ(heap.objectStats().live, 1U);
    stage.value.reset();
    const std::array roots{HeapRoot::object(escaped)};
    ASSERT_TRUE(heap.collect(roots));
    EXPECT_EQ(read(heap, ref(heap, escaped, 0))[0].a, 23U);
    EXPECT_FALSE(heap.objectInfo(escaped).value.constructed);
    EXPECT_EQ(heap.beginObjectStage(escaped, ObjectClassId{0}).error, ObjectError::InvalidStage);
    auto copy = heap.copyObject(escaped);
    ASSERT_TRUE(copy);
    EXPECT_TRUE(heap.objectInfo(copy.value.token()).value.constructed);
    EXPECT_EQ(read(heap, ref(heap, copy.value.token(), 0))[0].a, 23U);
}

TEST(Objects, WeakPromotionPinsBeforeCollectionAndClearsBeforeGenerationReuse) {
    Heap heap;
    ObjectClass target;
    target.name = "Target";
    ObjectClass weak;
    weak.name = "Weak";
    weak.weakTarget = ObjectClassId{0};
    const std::array types{target, weak};
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    Rng rng;
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    const auto old = value.value.token();
    auto observer = heap.newWeakObject(ObjectClassId{1}, old, rng);
    ASSERT_TRUE(observer);
    auto held = heap.lockWeakObject(observer.value.token());
    ASSERT_TRUE(held);
    EXPECT_EQ(held.value.token(), old);
    value.value.reset();
    ASSERT_TRUE(heap.collect({}));
    EXPECT_TRUE(heap.objectInfo(old));
    held.value.reset();
    ASSERT_TRUE(heap.collect({}));
    EXPECT_EQ(heap.objectInfo(old).error, ObjectError::InvalidToken);
    auto now = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(now);
    EXPECT_EQ(static_cast<std::uint32_t>(now.value.token().bits),
              static_cast<std::uint32_t>(old.bits));
    EXPECT_NE(now.value.token(), old);
    auto expired = heap.lockWeakObject(observer.value.token());
    ASSERT_TRUE(expired);
    EXPECT_FALSE(expired.value);
    EXPECT_EQ(heap.objectId(now.value.token()).value, 3U);
    auto empty = heap.newWeakObject(ObjectClassId{1}, {}, rng);
    ASSERT_TRUE(empty);
    EXPECT_FALSE(heap.lockWeakObject(empty.value.token()).value);
}

TEST(Objects, PropertyCellRootsRetainTheWholeOwnerAndItsOtherFields) {
    Heap heap;
    const auto layout = bits(heap);
    const std::array<ObjectField, 2> fields{{{layout}, {layout}}};
    ObjectClass type;
    type.name = "Pair";
    type.fields = fields;
    ASSERT_EQ(heap.addClasses({&type, 1}), ObjectError::None);
    Rng rng;
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    const auto token = value.value.token();
    const auto target = ref(heap, token, 0);
    HeapRoot captured;
    heap.setReadHook(
        {[](void* state, HeapRoot root) noexcept { *static_cast<HeapRoot*>(state) = root; },
         &captured});
    read(heap, target);
    EXPECT_EQ(captured.kind, HeapRoot::Kind::Cell);
    heap.setReadHook({});
    value.value.reset();
    ASSERT_TRUE(heap.collect({&captured, 1}));
    EXPECT_TRUE(heap.objectInfo(token));
    EXPECT_EQ(heap.stats().cells, 2U);
    EXPECT_TRUE(heap.reaches({&captured, 1}, HeapRoot::object(token)).value);
    ASSERT_TRUE(heap.collect({}));
    W4 out;
    EXPECT_EQ(heap.readObject(target, {&out, 1}), ObjectError::InvalidToken);
    EXPECT_EQ(heap.objectStats().live, 0U);
}

TEST(Objects, PartialWordStoresPreserveUnselectedContainerCellsAndNormalizeBeforeNotification) {
    Heap heap;
    const auto scalar = bits(heap, 4, true);
    auto queue = heap.addType({ContainerKind::Queue, scalar, 0});
    ASSERT_TRUE(queue);
    const std::array<HeapWord, 2> words{{{15, false}, {UINT64_MAX, false, {}, queue.value}}};
    const auto layout = heap.addLayout(words);
    ASSERT_TRUE(layout);
    simple(heap, layout.value);
    Rng rng;
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    const W4 item{3, 0};
    auto q = heap.fromValues(queue.value, {&item, 1});
    ASSERT_TRUE(q);
    const std::array<W4, 2> initial{{{7, 0}, {q.value.bits, 0}}};
    const auto all = ref(heap, value.value.token(), 0);
    ASSERT_EQ(heap.writeObject(all, initial), ObjectError::None);
    const auto before = read(heap, all)[1].a;
    unsigned notifications = 0;
    heap.setChangeHook(
        {[](void* state, HeapRoot) noexcept { ++*static_cast<unsigned*>(state); }, &notifications});
    auto first = all;
    first.count = 1;
    ASSERT_EQ(write(heap, first, 0x107), ObjectError::None);
    EXPECT_EQ(notifications, 0U);
    ASSERT_EQ(write(heap, first, 9), ObjectError::None);
    EXPECT_EQ(notifications, 1U);
    EXPECT_EQ(read(heap, all)[1].a, before);
    auto invalid = all;
    invalid.first = SIZE_MAX;
    EXPECT_EQ(write(heap, invalid, 2), ObjectError::InvalidField);
    EXPECT_EQ(read(heap, all)[1].a, before);
}

TEST(Objects, ForeignContextsAndWrongViewsNeverResolveThroughMatchingPoolIndices) {
    Heap first, second;
    const ObjectClass left{"Left"}, right{"Right"};
    const std::array types{left, right};
    ASSERT_EQ(first.addClasses(types), ObjectError::None);
    ASSERT_EQ(second.addClasses(types), ObjectError::None);
    Rng rng;
    auto a = first.newObject(ObjectClassId{0}, rng), b = second.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(a.value.token().bits, b.value.token().bits);
    EXPECT_EQ(first.pinObject(b.value.token()).error, ObjectError::InvalidToken);
    EXPECT_EQ(first.pinObject({0, &first}).error, ObjectError::InvalidToken);
    const auto foreignNull = HeapRoot::object({0, &second});
    EXPECT_FALSE(first.collect({&foreignNull, 1}));
    EXPECT_FALSE(first.reaches({&foreignNull, 1}, HeapRoot::object(a.value.token())));
    EXPECT_EQ(first.wordObject(first.objectWord(b.value.token()), ObjectClassId{0}).error,
              ObjectError::InvalidToken);
    EXPECT_EQ(first.wordObject(first.objectWord(a.value.token()), ObjectClassId{1}).error,
              ObjectError::InvalidClass);
    EXPECT_EQ(first.objectId({}).value, 0U);
    EXPECT_EQ(first.copyObject({}).error, ObjectError::Null);
    const auto root = HeapRoot::object(b.value.token());
    EXPECT_EQ(first.collect({&root, 1}).error, HeapError::InvalidHandle);
    EXPECT_TRUE(first.objectInfo(a.value.token()));
}

TEST(Objects, InterfaceViewsAndHiddenFieldsRetainOneDynamicIdentity) {
    Heap heap;
    const auto layout = bits(heap);
    const ObjectField field{layout};
    const std::array<std::uint32_t, 1> parentMethod{11}, overrideMethod{29};
    const std::array<ObjectClassId, 1> interfaces{ObjectClassId{1}};
    std::array<ObjectClass, 4> types{};
    types[0].name = "Derived";
    types[0].base = ObjectClassId{2};
    types[0].fields = {&field, 1};
    types[0].virtualMethods = overrideMethod;
    types[0].interfaces = interfaces;
    types[1].name = "Interface";
    types[1].interfaceClass = true;
    types[2].name = "Base";
    types[2].fields = {&field, 1};
    types[2].virtualMethods = parentMethod;
    types[2].abstract = true;
    types[3].name = "Other";
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    Rng rng;
    EXPECT_EQ(heap.newObject(ObjectClassId{2}, rng).error, ObjectError::InvalidClass);
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    const auto token = value.value.token();
    EXPECT_EQ(heap.virtualMethod(token, ObjectClassId{1}, 0).value, 29U);
    EXPECT_EQ(heap.virtualMethod(token, ObjectClassId{2}, 0).value, 29U);
    EXPECT_EQ(heap.virtualMethod(token, ObjectClassId{3}, 0).error, ObjectError::InvalidClass);
    EXPECT_EQ(heap.wordObject(heap.objectWord(token), ObjectClassId{1}).value, token);
    ASSERT_EQ(write(heap, ref(heap, token, 2), 7), ObjectError::None);
    ASSERT_EQ(write(heap, ref(heap, token, 0), 9), ObjectError::None);
    EXPECT_EQ(read(heap, ref(heap, token, 2))[0].a, 7U);
    EXPECT_EQ(read(heap, ref(heap, token, 0))[0].a, 9U);
    auto stage = heap.beginObjectStage(token, ObjectClassId{2});
    ASSERT_TRUE(stage);
    ASSERT_EQ(heap.beginObjectBody(stage.value), ObjectError::None);
    ASSERT_EQ(heap.finishObjectStage(stage.value), ObjectError::None);
    EXPECT_EQ(write(heap, ref(heap, token, 0), 20, &stage.value), ObjectError::InvalidStage);
    EXPECT_EQ(read(heap, ref(heap, token, 0))[0].a, 9U);
}

TEST(Objects, MetadataValidationIsAtomicForCyclesBadFieldsAndInterfaceDiamonds) {
    Heap heap;
    std::array<ObjectClass, 4> types{};
    for (auto& type : types) {
        type.name = "View";
        type.interfaceClass = true;
    }
    const std::array<ObjectClassId, 2> both{ObjectClassId{1}, ObjectClassId{2}};
    const std::array<ObjectClassId, 1> root{ObjectClassId{3}};
    types[0].interfaces = both;
    types[1].interfaces = root;
    types[2].interfaces = root;
    types[3].base = ObjectClassId{0};
    EXPECT_EQ(heap.addClasses(types), ObjectError::InvalidClass);
    types[3].base = kNone;
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    EXPECT_TRUE(heap.assignable(ObjectClassId{0}, ObjectClassId{3}));
    EXPECT_FALSE(heap.assignable(ObjectClassId{3}, ObjectClassId{0}));
    Rng rng;
    EXPECT_EQ(heap.newObject(ObjectClassId{0}, rng).error, ObjectError::InvalidClass);
    EXPECT_EQ(rng.state, 0U);
    EXPECT_EQ(heap.addClasses(types), ObjectError::InvalidClass);
    Heap badField;
    const ObjectField field{HeapLayoutId{123}};
    ObjectClass type;
    type.fields = {&field, 1};
    EXPECT_EQ(badField.addClasses({&type, 1}), ObjectError::InvalidField);
    type.fields = {};
    EXPECT_EQ(badField.addClasses({&type, 1}), ObjectError::None);
    HeapWord invalidWord;
    invalidWord.object = ObjectClassId{1};
    EXPECT_FALSE(badField.addLayout({&invalidWord, 1}));
    Heap empty;
    EXPECT_EQ(empty.addClasses({}), ObjectError::None);
    EXPECT_EQ(empty.addClasses({&type, 1}), ObjectError::InvalidClass);
}

TEST(Objects, FailedMarkKeepsObjectsAndWeakEdgesUntilACompleteCollection) {
    Heap heap;
    HeapWord string;
    string.string = true;
    const auto layout = heap.addLayout({&string, 1});
    ASSERT_TRUE(layout);
    const ObjectField field{layout.value};
    std::array<ObjectClass, 2> types{};
    types[0].name = "Text";
    types[0].fields = {&field, 1};
    types[1].name = "WeakText";
    types[1].weakTarget = ObjectClassId{0};
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    Rng rng;
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    const auto token = value.value.token();
    auto weak = heap.newWeakObject(ObjectClassId{1}, token, rng);
    ASSERT_TRUE(weak);
    value.value.reset();
    const auto root = HeapRoot::object(token);
    EXPECT_EQ(
        heap.collect({&root, 1}, {[](void*, std::uint64_t) noexcept { return false; }, nullptr})
            .error,
        HeapError::InvalidHandle);
    EXPECT_TRUE(heap.objectInfo(token));
    {
        auto held = heap.lockWeakObject(weak.value.token());
        ASSERT_TRUE(held);
        EXPECT_EQ(held.value.token(), token);
    }
    ASSERT_TRUE(heap.collect({}));
    EXPECT_EQ(heap.objectInfo(token).error, ObjectError::InvalidToken);
    EXPECT_FALSE(heap.lockWeakObject(weak.value.token()).value);
}

class FailingObjectMemory final : public std::pmr::memory_resource {
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

TEST(Objects, AllocationFailureLeavesNoPublishedIdentitySeedDrawOrLiveStorage) {
    bool succeeded = false;
    unsigned failures = 0;
    for (std::size_t failAt = 0; failAt < 80 && !succeeded; ++failAt) {
        FailingObjectMemory memory;
        Heap heap(&memory);
        simple(heap, bits(heap, 257, true));
        Rng parent{77}, expected = parent;
        const auto seed = expected.next();
        memory.remaining = failAt;
        auto value = heap.newObject(ObjectClassId{0}, parent);
        memory.remaining.reset();
        if (!value) {
            ++failures;
            EXPECT_EQ(value.error, ObjectError::ResourceLimit);
            EXPECT_EQ(parent.state, 77U);
            EXPECT_EQ(heap.objectStats().live, 0U);
            EXPECT_EQ(heap.stats().cells, 0U);
            auto retry = heap.newObject(ObjectClassId{0}, parent);
            ASSERT_TRUE(retry);
            EXPECT_EQ(heap.objectId(retry.value.token()).value, 1U);
            EXPECT_EQ(heap.objectInfo(retry.value.token()).value.ordinal, 1U);
            EXPECT_EQ(heap.objectInfo(retry.value.token()).value.randomState, seed);
        } else {
            succeeded = true;
            EXPECT_EQ(parent.state, expected.state);
        }
    }
    EXPECT_TRUE(succeeded);
    EXPECT_GT(failures, 3U);
}

TEST(Objects, RepeatedLifetimesReuseStorageWithoutReusingSourceIdentities) {
    Heap heap;
    simple(heap, bits(heap));
    Rng rng;
    ObjectToken previous;
    for (std::uint64_t i = 1; i <= 1024; ++i) {
        auto value = heap.newObject(ObjectClassId{0}, rng);
        ASSERT_TRUE(value);
        const auto token = value.value.token();
        EXPECT_EQ(heap.objectId(token).value, i);
        if (previous.bits != 0) {
            EXPECT_EQ(static_cast<std::uint32_t>(token.bits),
                      static_cast<std::uint32_t>(previous.bits));
            EXPECT_NE(token, previous);
            EXPECT_EQ(heap.objectInfo(previous).error, ObjectError::InvalidToken);
        }
        value.value.reset();
        ASSERT_TRUE(heap.collect({}));
        previous = token;
    }
    EXPECT_EQ(heap.objectStats().entries, 1U);
    EXPECT_EQ(heap.objectStats().live, 0U);
    EXPECT_EQ(heap.stats().cellEntries, 1U);
}

TEST(Objects, DeepBaseChainsUseIterativeRegistrationAndOrderedConstructorCapabilities) {
    Heap heap;
    std::vector<ObjectClass> classes(512);
    for (std::uint32_t i = 0; i < classes.size(); ++i) {
        classes[i].name = "Stage";
        if (i + 1 < classes.size()) classes[i].base = ObjectClassId{i + 1};
    }
    ASSERT_EQ(heap.addClasses(classes), ObjectError::None);
    Rng rng;
    auto value = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(value);
    for (std::uint32_t i = static_cast<std::uint32_t>(classes.size()); i-- > 0;) {
        auto stage = heap.beginObjectStage(value.value.token(), ObjectClassId{i});
        ASSERT_TRUE(stage);
        ASSERT_EQ(heap.beginObjectBody(stage.value), ObjectError::None);
        ASSERT_EQ(heap.finishObjectStage(stage.value), ObjectError::None);
    }
    EXPECT_TRUE(heap.objectInfo(value.value.token()).value.constructed);
}

TEST(Objects, CapturedBitSelectionsPreserveNeighboursAcrossWidthsAndStateDomains) {
    for (const unsigned width : {1U, 32U, 65U, 129U, 257U}) {
        for (const bool four : {false, true}) {
            Heap heap;
            simple(heap, bits(heap, width, four));
            Rng rng;
            auto object = heap.newObject(ObjectClassId{0}, rng);
            ASSERT_TRUE(object);
            const auto all = ref(heap, object.value.token(), 0);
            const auto initial = vs::filled(four ? vs::Logic::X : vs::Logic::Zero, width, false);
            for (const unsigned start : {0U, width / 2, width - 1, std::min(63U, width - 1)}) {
                for (const unsigned size : {1U, std::min(7U, width - start),
                                            std::min(65U, width - start), width - start}) {
                    SCOPED_TRACE(::testing::Message()
                                 << width << ' ' << four << ' ' << start << ' ' << size);
                    ASSERT_EQ(heap.writeObject(all, encode(initial)), ObjectError::None);
                    auto selected = heap.selectObjectBits(all, start, size);
                    ASSERT_TRUE(selected);
                    auto source = vs::filled(vs::Logic::Zero, size, false);
                    const std::array states{vs::Logic::One, vs::Logic::X, vs::Logic::Z,
                                            vs::Logic::Zero};
                    for (unsigned i = 0; i < size; ++i)
                        source.bits[i] = states[i % states.size()];
                    auto expected = initial;
                    const auto normalized = four ? source : vs::to2(source);
                    for (unsigned i = 0; i < size; ++i)
                        expected.bits[start + i] = normalized.bits[i];
                    ASSERT_EQ(heap.writeObject(selected.value, encode(source)), ObjectError::None);
                    EXPECT_EQ(decode(read(heap, all), width), expected);
                    std::vector<W4> selectedWords((size - 1) / 64 + 1);
                    ASSERT_EQ(heap.readObject(selected.value, selectedWords), ObjectError::None);
                    EXPECT_EQ(decode(selectedWords, size), normalized);
                    const auto nested = heap.selectObjectBits(selected.value, size - 1, 1);
                    ASSERT_TRUE(nested);
                    W4 last;
                    ASSERT_EQ(heap.readObject(nested.value, {&last, 1}), ObjectError::None);
                    EXPECT_EQ(decode({&last, 1}, 1).bits[0], normalized.bits.back());
                }
            }
        }
    }
}

TEST(Objects, PropertySelectionsCannotEscapeParentsOrSelectTypedHandles) {
    Heap heap;
    const auto scalar = bits(heap, 129, true), objectLayout = handle(heap, ObjectClassId{0});
    const std::array<ObjectField, 2> fields{{{scalar}, {objectLayout}}};
    ObjectClass type;
    type.name = "Selected";
    type.fields = fields;
    ASSERT_EQ(heap.addClasses({&type, 1}), ObjectError::None);
    Rng rng;
    auto object = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(object);
    const auto all = ref(heap, object.value.token(), 0);
    auto part = heap.selectObjectBits(all, 60, 65);
    ASSERT_TRUE(part);
    EXPECT_EQ(heap.selectObjectBits(part.value, 64, 2).error, ObjectError::InvalidField);
    EXPECT_EQ(heap.selectObjectBits(all, UINT64_MAX, 1).error, ObjectError::InvalidField);
    EXPECT_EQ(heap.selectObjectBits(all, 128, 2).error, ObjectError::InvalidField);
    EXPECT_EQ(heap.selectObjectBits(all, 0, 0).error, ObjectError::InvalidField);
    EXPECT_EQ(heap.selectObjectBits(ref(heap, object.value.token(), 0, 1), 0, 1).error,
              ObjectError::InvalidField);
    const W4 sentinel{19, 23};
    W4 output = sentinel;
    EXPECT_EQ(heap.readObject(part.value, {&output, 1}), ObjectError::InvalidValue);
    EXPECT_EQ(output.a, sentinel.a);
    EXPECT_EQ(output.b, sentinel.b);
    const auto before = read(heap, all);
    part.value.field.readonly = true;
    const std::array<W4, 2> input{{{7, 0}, {1, 0}}};
    EXPECT_EQ(heap.writeObject(part.value, input), ObjectError::ConstWrite);
    const auto after = read(heap, all);
    EXPECT_EQ(decode(after, 129), decode(before, 129));
}

TEST(Objects, CastsDistinguishStaticCompatibilityDynamicViewsAndLiteralNull) {
    Heap heap;
    std::array<ObjectClass, 4> types{};
    types[0].name = "Base";
    const std::array interfaces{ObjectClassId{3}};
    types[1].name = "Derived";
    types[1].base = ObjectClassId{0};
    types[1].interfaces = interfaces;
    types[2].name = "Other";
    types[3].name = "Interface";
    types[3].interfaceClass = true;
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    Rng rng;
    std::array<ObjectPin, 3> objects;
    for (unsigned dynamic = 0; dynamic < objects.size(); ++dynamic) {
        auto made = heap.newObject(ObjectClassId{dynamic}, rng);
        ASSERT_TRUE(made);
        objects[dynamic] = std::move(made.value);
    }
    const auto inherits = [](unsigned from, unsigned to) {
        return from == to || (from == 1 && (to == 0 || to == 3));
    };
    for (unsigned source = 0; source < types.size(); ++source) {
        for (unsigned destination = 0; destination < types.size(); ++destination) {
            const ObjectClassId s{source}, d{destination};
            auto null = heap.castObject({}, s, d);
            ASSERT_TRUE(null);
            EXPECT_EQ(null.value, inherits(source, destination));
            auto literal = heap.castObject({}, kNone, d);
            ASSERT_TRUE(literal);
            EXPECT_TRUE(literal.value);
            for (unsigned dynamic = 0; dynamic < objects.size(); ++dynamic) {
                SCOPED_TRACE(::testing::Message()
                             << source << ' ' << destination << ' ' << dynamic);
                const auto result = heap.castObject(objects[dynamic].token(), s, d);
                if (!inherits(dynamic, source)) {
                    EXPECT_EQ(result.error, ObjectError::InvalidClass);
                } else {
                    ASSERT_TRUE(result);
                    EXPECT_EQ(result.value, inherits(dynamic, destination));
                }
            }
        }
    }
    EXPECT_EQ(heap.castObject(objects[0].token(), kNone, ObjectClassId{0}).error,
              ObjectError::InvalidClass);
    Heap foreign;
    EXPECT_EQ(
        heap.castObject({objects[0].token().bits, &foreign}, ObjectClassId{0}, ObjectClassId{0})
            .error,
        ObjectError::InvalidToken);
}

TEST(Objects, RetainedAutomaticFramesTraceTypedObjectsAndReleaseThemOnUnwind) {
    Context context;
    auto& heap = context.heap();
    simple(heap, bits(heap));
    auto& activations = context.activations();
    const std::array fields{ActivationField{ActivationKind::Object, 64, {{2, 0}}, ObjectClassId{0}},
                            ActivationField{ActivationKind::Two, 64, {}}};
    auto layout = activations.addLayout(fields);
    ASSERT_TRUE(layout);
    auto frame = activations.create(layout.value);
    ASSERT_TRUE(frame);
    auto binding = activations.bind(frame.value, 0);
    ASSERT_TRUE(binding);
    auto integer = activations.bind(frame.value, 1);
    ASSERT_TRUE(integer);
    Rng rng;
    auto object = heap.newObject(ObjectClassId{0}, rng);
    ASSERT_TRUE(object);
    const auto token = object.value.token();
    const W4 encoded{token.bits, 0};
    ASSERT_EQ(activations.write(binding.value, 2, {&encoded, 1}), ActivationError::None);
    ASSERT_EQ(activations.write(integer.value, 0, {&encoded, 1}), ActivationError::None);
    auto suspended = activations.retain(binding.value);
    ASSERT_TRUE(suspended);
    frame.value.reset();
    object.value.reset();
    const auto roots = [&] {
        std::vector<HeapRoot> result;
        activations.visitRoots(
            [](void* p, HeapRoot root) noexcept {
                static_cast<std::vector<HeapRoot>*>(p)->push_back(root);
            },
            &result);
        return result;
    };
    ASSERT_EQ(roots().size(), 1U);
    ASSERT_TRUE(heap.collect(roots()));
    EXPECT_TRUE(heap.objectInfo(token));
    activations.acknowledgeRoots();
    const W4 null{};
    ASSERT_EQ(activations.write(binding.value, 2, {&null, 1}), ActivationError::None);
    EXPECT_TRUE(activations.rootsChanged());
    ASSERT_TRUE(heap.collect(roots()));
    EXPECT_EQ(heap.objectInfo(token).error, ObjectError::InvalidToken);
    EXPECT_TRUE(roots().empty());
    suspended.value.reset();
    EXPECT_EQ(activations.stats().live, 0U);
}

TEST(Objects, AutomaticObjectFieldsValidateClassViewsBeforeChangingStorage) {
    Heap heap;
    std::array<ObjectClass, 3> types{};
    types[0].name = "Base";
    types[1].name = "Derived";
    types[1].base = ObjectClassId{0};
    types[2].name = "Other";
    ASSERT_EQ(heap.addClasses(types), ObjectError::None);
    ActivationStore store(std::pmr::new_delete_resource(), &heap), unbound;
    const ActivationField field{ActivationKind::Object, 64, {}, ObjectClassId{0}};
    EXPECT_EQ(unbound.addLayout({&field, 1}).error, ActivationError::InvalidLayout);
    auto invalid = field;
    invalid.width = 32;
    EXPECT_EQ(store.addLayout({&invalid, 1}).error, ActivationError::InvalidLayout);
    invalid = field;
    invalid.objectView = ObjectClassId{99};
    EXPECT_EQ(store.addLayout({&invalid, 1}).error, ActivationError::InvalidLayout);
    invalid = field;
    invalid.kind = ActivationKind::Two;
    EXPECT_EQ(store.addLayout({&invalid, 1}).error, ActivationError::InvalidLayout);
    auto layout = store.addLayout({&field, 1});
    ASSERT_TRUE(layout);
    auto frame = store.create(layout.value);
    ASSERT_TRUE(frame);
    auto binding = store.bind(frame.value, 0);
    ASSERT_TRUE(binding);
    Rng rng;
    auto derived = heap.newObject(ObjectClassId{1}, rng);
    ASSERT_TRUE(derived);
    auto other = heap.newObject(ObjectClassId{2}, rng);
    ASSERT_TRUE(other);
    const W4 valid{derived.value.token().bits, 0}, wrong{other.value.token().bits, 0};
    ASSERT_EQ(store.write(binding.value, 0, {&valid, 1}), ActivationError::None);
    store.acknowledgeRoots();
    EXPECT_EQ(store.write(binding.value, 0, {&wrong, 1}), ActivationError::InvalidValue);
    W4 output{};
    ASSERT_EQ(store.read(binding.value, 0, {&output, 1}), ActivationError::None);
    EXPECT_EQ(output.a, valid.a);
    EXPECT_FALSE(store.rootsChanged());
    const auto pointerWord = heap.objectWord(derived.value.token());
    EXPECT_EQ(store.write(binding.value, 0, {&pointerWord, 1}), ActivationError::InvalidValue);
}

}  // namespace
