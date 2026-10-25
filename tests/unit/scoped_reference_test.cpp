// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <new>
#include <vector>

#include "frame_value.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/w4ops.h"
#include "scoped_reference.h"

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
namespace v = kirimasim::vm::detail;
using Error = k::ActivationError;

struct Body {
    std::vector<s::FrameVar> frame;
    s::ActivationInfo info;
    explicit Body(std::uint32_t width = 129) {
        frame = {{"first", s::u4(width)},
                 {"ordinary", s::u2(32)},
                 {"last", s::u2(7)},
                 {"array", s::u4(width), s::kNone, {{2, 0}}},
                 {"block", s::u2(32)}};
        for (const auto row : {0U, 2U, 3U, 4U}) {
            frame[row].home = s::FrameHome::Scoped;
            frame[row].activationScope = s::ActivationScopeId{row == 4 ? 1U : 0U};
        }
        // Deliberately not frame order. Field ordinals are not leaf steps.
        info.root = s::ActivationScopeId{0};
        info.scopes = {{s::kNone, {s::FrameVarId{3}, s::FrameVarId{2}, s::FrameVarId{0}}},
                       {s::ActivationScopeId{0}, {s::FrameVarId{4}}}};
    }
    auto compile(k::ActivationStore& store) const {
        return v::ActivationPlan::compile(store, frame, info);
    }
};
auto origin(const v::ActivationEnvironment& environment, std::uint32_t row) {
    auto result = environment.origin(s::FrameVarId{row});
    EXPECT_TRUE(result);
    return result.value;
}
v::FrameValue view(k::Context& context, const v::ScopedReferences::Selection& at) {
    return {context.activations(), context.scheduler(), at.binding, at.element, at.width, at.lsb};
}

TEST(ScopedReferences, WideArraysResolveActualFieldAndElementWithNoMirrorOrOwnership) {
    unsigned selections = 0;
    for (const auto width : {1U, 7U, 32U, 63U, 64U, 65U, 127U, 128U, 129U, 257U}) {
        k::Context context;
        auto& store = context.activations();
        Body body(width);
        auto plan = body.compile(store);
        ASSERT_TRUE(plan);
        auto environment = v::ActivationEnvironment::root(store, plan.value);
        ASSERT_TRUE(environment);
        v::ScopedReferences refs(store);
        const auto shape = refs.addShape(plan.value);
        ASSERT_TRUE(shape);
        const auto before = store.stats();
        for (std::uint32_t e = 0; e < 3; ++e) {
            auto token = refs.make(shape.value, origin(*environment.value, 3), e, 91, 1);
            ASSERT_TRUE(token);
            for (std::uint32_t chunk = 0; chunk < (width + 63) / 64; ++chunk) {
                SCOPED_TRACE(width);
                SCOPED_TRACE(e);
                SCOPED_TRACE(chunk);
                const auto selected = refs.resolveWord(token.value, chunk);
                ASSERT_TRUE(selected);
                EXPECT_EQ(selected.value.binding.field(), 0U);
                EXPECT_EQ(selected.value.element, e);
                EXPECT_EQ(selected.value.lsb, chunk * 64);
                EXPECT_EQ(selected.value.width, std::min(64U, width - chunk * 64));
                const k::W4 input{
                    (k::mask(selected.value.width) ^ e) & k::mask(selected.value.width),
                    (k::mask(selected.value.width) ^ chunk) & k::mask(selected.value.width)};
                ASSERT_EQ(view(context, selected.value).write({&input, 1}), Error::None);
                k::W4 actual{};
                ASSERT_EQ(store.read(origin(*environment.value, 3).binding(), e, {&actual, 1},
                                     chunk * 64, selected.value.width),
                          Error::None);
                EXPECT_EQ(actual.a, input.a);
                EXPECT_EQ(actual.b, input.b);
                const k::W4 external{e + 1, 0};
                ASSERT_EQ(store.write(selected.value.binding, e, {&external, 1}, chunk * 64,
                                      selected.value.width),
                          Error::None);
                ASSERT_EQ(view(context, selected.value).read({&actual, 1}), Error::None);
                EXPECT_EQ(actual.a, external.a & k::mask(selected.value.width));
                ++selections;
            }
        }
        EXPECT_EQ(store.stats().leases, before.leases);
        EXPECT_EQ(store.stats().words, before.words);
        refs.release(91);
        EXPECT_EQ(refs.stats().live, 0U);
    }
    EXPECT_EQ(selections, 57U);
}

TEST(ScopedReferences, LeafStepsUseLogicalFrameRowsAndCannotCrossOrdinaryOrLexicalHoles) {
    k::ActivationStore store;
    Body body(64);
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto env = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(env);
    ASSERT_EQ(env.value->enter(s::ActivationScopeId{1}), Error::None);
    v::ScopedReferences refs(store);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    const auto first = refs.make(shape.value, origin(*env.value, 0), 0, 1, 1);
    ASSERT_TRUE(first);
    EXPECT_EQ(refs.step(first.value, 1, 1, 1).error, Error::OutOfRange);  // ordinary row
    const auto leaf = refs.step(first.value, 2, 1, 1);
    ASSERT_TRUE(leaf);
    const auto selected = refs.resolveWord(leaf.value, 0);
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected.value.binding.field(), 1U);
    EXPECT_EQ(selected.value.width, 7U);
    const auto direct = refs.make(shape.value, origin(*env.value, 2), 0, 1, 1);
    ASSERT_TRUE(direct);
    EXPECT_EQ(direct.value, leaf.value);  // normalized addresses are interned
    EXPECT_EQ(refs.step(first.value, 3, 1, 1).error, Error::OutOfRange);  // array placeholder
    EXPECT_EQ(refs.step(first.value, 4, 1, 1).error, Error::OutOfRange);  // different extent
    EXPECT_EQ(refs.step(first.value, std::numeric_limits<std::uint64_t>::max(), 1, 1).error,
              Error::OutOfRange);
    EXPECT_EQ(refs.resolve(first.value, 0, 65).error, Error::OutOfRange);
    EXPECT_EQ(refs.resolve(first.value, 0, 0).error, Error::OutOfRange);
}

TEST(ScopedReferences, CapturesKeepOriginalPlanAfterOwnerReturnAndReentry) {
    k::ActivationStore store;
    Body body;
    body.frame[3].activationScope = s::ActivationScopeId{1};
    body.info.scopes[0].fields.erase(body.info.scopes[0].fields.begin());
    body.info.scopes[1].fields.push_back(s::FrameVarId{3});
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto owner = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(owner);
    ASSERT_EQ(owner.value->enter(s::ActivationScopeId{1}), Error::None);
    std::vector<s::FrameVar> childFrame{body.frame[3]};
    childFrame[0].home = s::FrameHome::Capture;
    childFrame[0].activationScope = s::kNone;
    childFrame[0].captureSource = s::FrameVarId{3};
    const s::ActivationInfo childInfo{
        {}, s::kNone, {s::ActivationScopeId{0}, s::ActivationScopeId{1}}};
    auto childPlan = v::ActivationPlan::compile(store, childFrame, childInfo);
    ASSERT_TRUE(childPlan);
    const std::array plans{&childPlan.value};
    auto child = v::ActivationEnvironment::children(store, plan.value, owner.value.get(), plans,
                                                    v::CaptureJoin::None);
    ASSERT_TRUE(child);
    const auto captured = origin(*child.value[0], 0);
    EXPECT_EQ(&captured.plan(), &plan.value);
    EXPECT_EQ(captured.row(), s::FrameVarId{3});
    ASSERT_EQ(owner.value->leave(s::ActivationScopeId{1}), Error::None);
    ASSERT_EQ(owner.value->enter(s::ActivationScopeId{1}), Error::None);
    EXPECT_NE(captured.binding().token(), origin(*owner.value, 3).binding().token());
    owner.value->finish();
    v::ScopedReferences refs(store);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    const auto token = refs.make(shape.value, captured, 2, 7, 0);
    ASSERT_TRUE(token);
    EXPECT_TRUE(refs.resolve(token.value, 0, 129));
    child.value.clear();
    EXPECT_EQ(refs.resolve(token.value, 0, 129).error, Error::InvalidBinding);
}

TEST(ScopedReferences, ReadonlyCapturesCannotRecoverWriteAccessByStepping) {
    k::Context context;
    auto& store = context.activations();
    Body body(64);
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto owner = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(owner);
    std::vector<s::FrameVar> childFrame{body.frame[0]};
    childFrame[0].home = s::FrameHome::Capture;
    childFrame[0].activationScope = s::kNone;
    childFrame[0].captureSource = s::FrameVarId{0};
    childFrame[0].readonly = true;
    const s::ActivationInfo info{{}, s::kNone, {s::ActivationScopeId{0}}};
    auto childPlan = v::ActivationPlan::compile(store, childFrame, info);
    ASSERT_TRUE(childPlan);
    const std::array plans{&childPlan.value};
    auto children = v::ActivationEnvironment::children(store, plan.value, owner.value.get(), plans,
                                                       v::CaptureJoin::None);
    ASSERT_TRUE(children);
    v::ScopedReferences refs(store);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    const auto writable = refs.make(shape.value, origin(*owner.value, 0), 0, 9, 0);
    ASSERT_TRUE(writable);
    const auto token = refs.make(shape.value, origin(*children.value[0], 0), 0, 9, 0);
    ASSERT_TRUE(token);
    EXPECT_NE(writable.value, token.value);
    const auto moved = refs.step(token.value, 2, 9, 0);
    ASSERT_TRUE(moved);
    const auto at = refs.resolveWord(moved.value, 0);
    ASSERT_TRUE(at);
    EXPECT_TRUE(at.value.binding.readonly());
    const k::W4 data{7, 0};
    EXPECT_EQ(view(context, at.value).write({&data, 1}), Error::ReadOnly);
}

TEST(ScopedReferences, ScopeTicketsAreReusedAndStaleGenerationsNeverResolve) {
    k::ActivationStore store;
    Body body;
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto env = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(env);
    v::ScopedReferences refs(store);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    const auto root = refs.make(shape.value, origin(*env.value, 0), 0, 2, 1);
    ASSERT_TRUE(root);
    std::uint64_t previous = 0;
    for (unsigned i = 0; i < 10000; ++i) {
        ASSERT_EQ(env.value->enter(s::ActivationScopeId{1}), Error::None);
        const auto local = refs.make(shape.value, origin(*env.value, 4), 0, 2, 2);
        ASSERT_TRUE(local);
        if (previous != 0) EXPECT_FALSE(refs.resolveWord(previous, 0));
        for (unsigned repeat = 0; repeat < 4; ++repeat)
            EXPECT_EQ(refs.make(shape.value, origin(*env.value, 4), 0, 2, 2).value, local.value);
        refs.release(2, 2);
        EXPECT_TRUE(refs.resolveWord(root.value, 0));
        EXPECT_FALSE(refs.resolveWord(local.value, 0));
        ASSERT_EQ(env.value->leave(s::ActivationScopeId{1}), Error::None);
        previous = local.value;
    }
    EXPECT_EQ(refs.stats().live, 1U);
    EXPECT_EQ(refs.stats().entries, 2U);
    const auto sibling = refs.make(shape.value, origin(*env.value, 0), 0, 3, 1);
    ASSERT_TRUE(sibling);
    refs.release(2);
    EXPECT_FALSE(refs.resolveWord(root.value, 0));
    EXPECT_TRUE(refs.resolveWord(sibling.value, 0));
    refs.release(3);
    EXPECT_EQ(refs.stats().live, 0U);
}

TEST(ScopedReferences, InvalidIdentityShapeIndexAndSelectionLeaveNoTickets) {
    k::ActivationStore store, foreign;
    Body body;
    auto plan = body.compile(store), otherPlan = body.compile(foreign);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(otherPlan);
    auto env = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(env);
    v::ScopedReferences refs(store);
    EXPECT_EQ(refs.addShape(v::ActivationPlan{}).error, Error::InvalidLayout);
    EXPECT_EQ(refs.addShape(otherPlan.value).error, Error::InvalidLayout);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    EXPECT_FALSE(refs.make(shape.value, {}, 0, 1, 1));
    EXPECT_FALSE(refs.make(99, origin(*env.value, 0), 0, 1, 1));
    EXPECT_FALSE(refs.make(shape.value, origin(*env.value, 3), 3, 1, 1));
    EXPECT_FALSE(refs.resolveWord(0, 0));
    EXPECT_FALSE(refs.resolveWord(UINT64_MAX, 0));
    EXPECT_EQ(refs.stats().live, 0U);
    const auto stale = origin(*env.value, 0);
    store.clear();
    EXPECT_FALSE(refs.make(shape.value, stale, 0, 1, 1));
    EXPECT_EQ(refs.stats().live, 0U);
}

class Budget final : public std::pmr::memory_resource {
public:
    std::size_t remaining = std::numeric_limits<std::size_t>::max();
    std::size_t live = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc();
        --remaining;
        auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++live;
        return result;
    }
    void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
        --live;
        std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ScopedReferences, EveryShapeAndTicketAllocationFailureIsTransactional) {
    k::ActivationStore store;
    Body body;
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto env = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(env);
    unsigned shapeFailures = 0, ticketFailures = 0;
    for (std::size_t budget = 0; budget < 30; ++budget) {
        Budget memory;
        {
            v::ScopedReferences refs(store, &memory);
            memory.remaining = budget;
            const auto shape = refs.addShape(plan.value);
            if (!shape) {
                EXPECT_EQ(shape.error, Error::ResourceLimit);
                ++shapeFailures;
            } else {
                const auto ref = refs.make(shape.value, origin(*env.value, 3), 2, 1, 1);
                if (!ref) {
                    EXPECT_EQ(ref.error, Error::ResourceLimit);
                    EXPECT_EQ(refs.stats().live, 0U);
                    ++ticketFailures;
                    memory.remaining = 30;
                    EXPECT_TRUE(refs.make(shape.value, origin(*env.value, 3), 2, 1, 1));
                }
                refs.release(1);
                EXPECT_EQ(refs.stats().live, 0U);
            }
        }
        EXPECT_EQ(memory.live, 0U);
        EXPECT_EQ(store.stats().leases, 1U);
    }
    EXPECT_GE(shapeFailures, 5U);
    EXPECT_GE(ticketFailures, 3U);
}

TEST(ScopedReferences, ForwardedCaptureOriginsNeverBecomeIntermediateRows) {
    k::ActivationStore store;
    Body body;
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto owner = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(owner);
    std::vector<s::FrameVar> middleFrame{body.frame[3]};
    middleFrame[0].home = s::FrameHome::Capture;
    middleFrame[0].activationScope = s::kNone;
    middleFrame[0].captureSource = s::FrameVarId{3};
    const s::ActivationInfo middleInfo{{}, s::kNone, {s::ActivationScopeId{0}}};
    auto middlePlan = v::ActivationPlan::compile(store, middleFrame, middleInfo);
    ASSERT_TRUE(middlePlan);
    const std::array middlePlans{&middlePlan.value};
    auto middle = v::ActivationEnvironment::children(store, plan.value, owner.value.get(),
                                                     middlePlans, v::CaptureJoin::None);
    ASSERT_TRUE(middle);
    std::vector<s::FrameVar> descendantFrame = middleFrame;
    descendantFrame[0].captureSource = s::FrameVarId{0};
    const s::ActivationInfo descendantInfo{{}, s::kNone, {}, true};
    auto descendantPlan = v::ActivationPlan::compile(store, descendantFrame, descendantInfo);
    ASSERT_TRUE(descendantPlan);
    const std::array descendantPlans{&descendantPlan.value};
    auto descendant = v::ActivationEnvironment::children(
        store, middlePlan.value, middle.value[0].get(), descendantPlans, v::CaptureJoin::None);
    ASSERT_TRUE(descendant);
    owner.value->finish();
    middle.value.clear();
    const auto forwarded = origin(*descendant.value[0], 0);
    EXPECT_EQ(&forwarded.plan(), &plan.value);
    EXPECT_EQ(forwarded.row(), s::FrameVarId{3});
    v::ScopedReferences refs(store);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    const auto token = refs.make(shape.value, forwarded, 1, 8, 0);
    ASSERT_TRUE(token);
    EXPECT_TRUE(refs.resolve(token.value, 0, 129));
    descendant.value.clear();
    EXPECT_FALSE(refs.resolve(token.value, 0, 129));
}

TEST(ScopedReferences, FrameLayoutSeparatesLogicalAddressesFromPhysicalOrdinaryWords) {
    const std::vector<s::FrameVar> ordinary{{"wide", s::u4(129)},
                                            {"narrow", s::u2(7)},
                                            {"text", s::Type{s::TypeKind::String}},
                                            {"array", s::u4(65), s::kNone, {{2, 0}}},
                                            {"alias", s::refType(), s::kNone, {}, s::u2(32)},
                                            {"typed", s::dynarrayType(s::TypeId{0})}};
    const v::FrameLayout old(ordinary);
    EXPECT_EQ(old.size, 14U);
    EXPECT_EQ(old.wordOf(s::FrameVarId{0}), 0U);
    EXPECT_EQ(old.wordOf(s::FrameVarId{1}), 3U);
    EXPECT_EQ(old.wordOf(s::FrameVarId{3}), 8U);
    EXPECT_EQ(old.wordOf(s::FrameVarId{4}), 6U);
    EXPECT_EQ(old.roots, (std::pmr::vector<std::size_t>{4, 5}));
    auto mixed = ordinary;
    mixed[0].home = s::FrameHome::Scoped;
    mixed[2].home = mixed[5].home = s::FrameHome::Capture;
    const v::FrameLayout decoded(mixed);
    EXPECT_EQ(decoded.size, 9U);
    EXPECT_EQ(decoded.logicalSize, old.size);
    EXPECT_EQ(decoded.wordOf(s::FrameVarId{1}), 0U);
    EXPECT_EQ(decoded.wordOf(s::FrameVarId{3}), 3U);
    EXPECT_EQ(decoded.wordOf(s::FrameVarId{4}), 2U);
    EXPECT_EQ(decoded.logical, old.logical);
    EXPECT_EQ(decoded.roots, (std::pmr::vector<std::size_t>{4}));
}

TEST(ScopedReferences, ShortrealSelectionsAndSubviewWritesUseTheirActualWidthAndOrigin) {
    k::Context context;
    auto& store = context.activations();
    Body body;
    body.frame[0].type = s::Type{s::TypeKind::ShortReal};
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto env = v::ActivationEnvironment::root(store, plan.value);
    ASSERT_TRUE(env);
    v::ScopedReferences refs(store);
    const auto shape = refs.addShape(plan.value);
    ASSERT_TRUE(shape);
    const auto token = refs.make(shape.value, origin(*env.value, 0), 0, 2, 1);
    ASSERT_TRUE(token);
    const auto at = refs.resolveWord(token.value, 0);
    ASSERT_TRUE(at);
    EXPECT_EQ(at.value.width, 32U);
    const k::W4 bits{0xffc12345, 0};
    ASSERT_EQ(view(context, at.value).write({&bits, 1}), Error::None);
    k::W4 read{};
    ASSERT_EQ(view(context, at.value).read({&read, 1}), Error::None);
    EXPECT_EQ(read.a, bits.a);
    EXPECT_EQ(refs.resolve(token.value, 0, 64).error, Error::OutOfRange);
    const auto wide = refs.make(shape.value, origin(*env.value, 3), 1, 2, 1);
    ASSERT_TRUE(wide);
    const auto high = refs.resolveWord(wide.value, 1);
    ASSERT_TRUE(high);
    const k::W4 part{3, 2};
    ASSERT_EQ(view(context, high.value).write({&part, 1}, 31, 2), Error::None);
    std::array<k::W4, 3> value{};
    ASSERT_EQ(store.read(origin(*env.value, 3).binding(), 1, value), Error::None);
    EXPECT_EQ(value[0].a, 0U);
    EXPECT_EQ(value[0].b, 0U);
    EXPECT_EQ(value[1].a, std::uint64_t{3} << 31);
    EXPECT_EQ(value[1].b, std::uint64_t{2} << 31);
    EXPECT_EQ(value[2].a, 0U);
    EXPECT_EQ(value[2].b, 0U);
}

TEST(ScopedReferences, ImmutableBorrowedRefViewsPreservePayloadAndRejectRebinding) {
    const k::W4 token{0xabcdef0123456789ULL, 0xe000000123456789ULL};
    const auto place = v::FrameValue::reference(token);
    k::W4 actual{};
    ASSERT_EQ(place.read({&actual, 1}), Error::None);
    EXPECT_EQ(actual.a, token.a);
    EXPECT_EQ(actual.b, token.b);
    EXPECT_EQ(place.write({&actual, 1}), Error::ReadOnly);
    EXPECT_EQ(place.read({&actual, 1}, 0, 32), Error::OutOfRange);
    EXPECT_EQ(place.read({}), Error::OutOfRange);
}

}  // namespace
