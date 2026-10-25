// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "activation_environment.h"
#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
namespace v = kirimasim::vm::detail;
using Env = v::ActivationEnvironment;
using Plan = v::ActivationPlan;
using Error = k::ActivationError;
using Join = v::CaptureJoin;

struct Body {
    std::vector<s::FrameVar> frame;
    s::ActivationInfo activation;
    auto compile(k::ActivationStore& store) const {
        return Plan::compile(store, frame, activation);
    }
};
Body ownerBody() {
    Body body;
    body.frame = {{"number", s::u2(64)},
                  {"array", s::u4(129), s::kNone, {{-1, 1}}},
                  {"unread", s::dynarrayType(s::TypeId{0})}};
    for (std::size_t i = 0; i < body.frame.size(); ++i) {
        body.frame[i].home = s::FrameHome::Scoped;
        body.frame[i].activationScope = s::ActivationScopeId{i == 1 ? 1U : 0U};
    }
    body.activation.root = s::ActivationScopeId{0};
    body.activation.scopes = {{s::kNone, {s::FrameVarId{0}, s::FrameVarId{2}}},
                              {s::ActivationScopeId{0}, {s::FrameVarId{1}}},
                              {s::ActivationScopeId{1}, {}}};
    return body;
}
Body childBody(const Body& parent) {
    Body body;
    body.frame = {parent.frame[1], parent.frame[0], {"local", s::u2(32)}};
    for (std::uint32_t i = 0; i < 2; ++i) {
        body.frame[i].home = s::FrameHome::Capture;
        body.frame[i].activationScope = s::kNone;
        body.frame[i].captureSource = s::FrameVarId{1 - i};
    }
    body.frame[2].home = s::FrameHome::Scoped;
    body.frame[2].activationScope = s::ActivationScopeId{0};
    body.activation.root = s::ActivationScopeId{0};
    body.activation.scopes = {{s::kNone, {s::FrameVarId{2}}}};
    body.activation.extents = {s::ActivationScopeId{0}, s::ActivationScopeId{1},
                               s::ActivationScopeId{2}};
    return body;
}
k::ActivationBinding binding(const Env& environment, std::uint32_t field) {
    const auto value = environment.binding(s::FrameVarId{field});
    EXPECT_TRUE(value);
    return value.value;
}
void write(k::ActivationStore& store, k::ActivationBinding field, std::uint64_t value) {
    const k::W4 data{value, 0};
    ASSERT_EQ(store.write(field, 0, {&data, 1}, 0, 32), Error::None);
}
std::uint64_t read(k::ActivationStore& store, k::ActivationBinding field) {
    k::W4 data{};
    EXPECT_EQ(store.read(field, 0, {&data, 1}, 0, 32), Error::None);
    EXPECT_EQ(data.b, 0U);
    return data.a;
}
void enterBlocks(Env& environment) {
    ASSERT_EQ(environment.enter(s::ActivationScopeId{1}), Error::None);
    ASSERT_EQ(environment.enter(s::ActivationScopeId{2}), Error::None);
}

TEST(ActivationEnvironment, SiblingsSharePhysicalFieldsAndKeepFullUnreadAndEmptyExtents) {
    for (const auto join : {Join::All, Join::Any, Join::None}) {
        k::ActivationStore store;
        const Body parentBody = ownerBody(), branchBody = childBody(parentBody);
        auto parentPlan = parentBody.compile(store), childPlan = branchBody.compile(store);
        ASSERT_TRUE(parentPlan);
        ASSERT_TRUE(childPlan);
        auto parent = Env::root(store, parentPlan.value);
        ASSERT_TRUE(parent);
        enterBlocks(*parent.value);
        const std::array plans{&childPlan.value, &childPlan.value};
        auto children = Env::children(store, parentPlan.value, parent.value.get(), plans, join);
        ASSERT_TRUE(children);
        ASSERT_EQ(children.value.size(), 2U);
        EXPECT_EQ(children.value[0]->importedExtents(), 3U);
        EXPECT_EQ(store.stats().live, 5U);
        EXPECT_EQ(store.stats().leases, 11U);
        const auto actual = binding(*parent.value, 1);
        const auto first = binding(*children.value[0], 0), second = binding(*children.value[1], 0);
        EXPECT_EQ(first.token(), actual.token());
        EXPECT_EQ(first.field(), actual.field());
        EXPECT_EQ(second.token(), actual.token());
        write(store, actual, 7);
        EXPECT_EQ(read(store, first), 7U);
        write(store, second, 19);
        EXPECT_EQ(read(store, actual), 19U);
        parent.value->finish();
        EXPECT_FALSE(parent.value->binding(s::FrameVarId{1}));
        EXPECT_EQ(read(store, first), 19U);
        children.value[0]->finish();
        EXPECT_EQ(store.stats().live, 4U);
        EXPECT_EQ(store.stats().leases, 4U);
        EXPECT_EQ(read(store, second), 19U);
        children.value[1]->finish();
        EXPECT_EQ(store.stats().live, 0U);
        EXPECT_EQ(store.stats().leases, 0U);
        EXPECT_FALSE(store.retain(first));
    }
}

TEST(ActivationEnvironment, CallsAndReenteredBlocksKeepDistinctImmutableBindings) {
    k::ActivationStore store;
    const Body body = ownerBody(), child = childBody(body);
    auto plan = body.compile(store), childPlan = child.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    auto first = Env::root(store, plan.value), second = Env::root(store, plan.value);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    enterBlocks(*first.value);
    enterBlocks(*second.value);
    const auto old = binding(*first.value, 1), separate = binding(*second.value, 1);
    EXPECT_NE(old.token(), separate.token());
    write(store, old, 7);
    write(store, separate, 29);
    const std::array plans{&childPlan.value};
    auto captured = Env::children(store, plan.value, first.value.get(), plans, Join::None);
    ASSERT_TRUE(captured);
    ASSERT_EQ(first.value->unwind(1), Error::None);
    enterBlocks(*first.value);
    const auto fresh = binding(*first.value, 1);
    EXPECT_NE(old.token(), fresh.token());
    write(store, fresh, 41);
    EXPECT_EQ(read(store, binding(*captured.value[0], 0)), 7U);
    EXPECT_EQ(read(store, separate), 29U);
    EXPECT_EQ(read(store, fresh), 41U);
    captured.value.clear();
    EXPECT_FALSE(store.retain(old));
    EXPECT_TRUE(store.retain(fresh));
}

TEST(ActivationEnvironment, DescendantsForwardOriginalExtentsAfterBothOwnersFinish) {
    k::ActivationStore store;
    const Body body = ownerBody(), child = childBody(body);
    Body grandchild;
    grandchild.frame = {child.frame[0], child.frame[2]};
    for (std::uint32_t i = 0; i < 2; ++i) {
        grandchild.frame[i].home = s::FrameHome::Capture;
        grandchild.frame[i].activationScope = s::kNone;
        grandchild.frame[i].captureSource = s::FrameVarId{i == 0 ? 0U : 2U};
    }
    grandchild.activation.extents = {s::ActivationScopeId{0}};
    grandchild.activation.inheritCaptures = true;
    auto plan = body.compile(store), childPlan = child.compile(store),
         grandPlan = grandchild.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    ASSERT_TRUE(grandPlan);
    auto owner = Env::root(store, plan.value);
    ASSERT_TRUE(owner);
    enterBlocks(*owner.value);
    const auto original = binding(*owner.value, 1);
    write(store, original, 71);
    const std::array childPlans{&childPlan.value}, grandPlans{&grandPlan.value};
    auto middle = Env::children(store, plan.value, owner.value.get(), childPlans, Join::None);
    ASSERT_TRUE(middle);
    write(store, binding(*middle.value[0], 2), 83);
    auto last =
        Env::children(store, childPlan.value, middle.value[0].get(), grandPlans, Join::None);
    ASSERT_TRUE(last);
    EXPECT_EQ(last.value[0]->importedExtents(), 4U);
    owner.value.reset();
    middle.value.clear();
    EXPECT_EQ(store.stats().live, 4U);
    EXPECT_EQ(binding(*last.value[0], 0).token(), original.token());
    EXPECT_EQ(read(store, binding(*last.value[0], 0)), 71U);
    EXPECT_EQ(read(store, binding(*last.value[0], 1)), 83U);
    last.value.clear();
    EXPECT_EQ(store.stats().live, 0U);
}

TEST(ActivationEnvironment, TemporaryPinsIncludeEmptyExtentsAndHaveNoControlState) {
    k::ActivationStore store;
    const Body body = ownerBody();
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto environment = Env::root(store, plan.value);
    ASSERT_TRUE(environment);
    enterBlocks(*environment.value);
    const auto value = binding(*environment.value, 1);
    write(store, value, 97);
    auto pins = environment.value->pin();
    ASSERT_TRUE(pins);
    EXPECT_EQ(pins.value.size(), 3U);
    EXPECT_EQ(store.stats().leases, 6U);
    environment.value.reset();
    EXPECT_EQ(store.stats().live, 3U);
    EXPECT_EQ(read(store, value), 97U);
    pins.value.clear();
    EXPECT_EQ(store.stats().live, 0U);
    EXPECT_FALSE(store.retain(value));
}

TEST(ActivationEnvironment, ScopeOrderAndSavedDepthAreCheckedBeforeOwnershipChanges) {
    k::ActivationStore store;
    const Body body = ownerBody();
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto environment = Env::root(store, plan.value);
    ASSERT_TRUE(environment);
    EXPECT_EQ(environment.value->enter(s::ActivationScopeId{0}), Error::InvalidBinding);
    EXPECT_EQ(environment.value->enter(s::ActivationScopeId{2}), Error::InvalidBinding);
    EXPECT_EQ(environment.value->leave(s::ActivationScopeId{0}), Error::InvalidBinding);
    EXPECT_EQ(environment.value->unwind(0), Error::InvalidBinding);
    enterBlocks(*environment.value);
    const auto previous = binding(*environment.value, 1);
    EXPECT_EQ(environment.value->leave(s::ActivationScopeId{1}), Error::InvalidBinding);
    EXPECT_EQ(environment.value->unwind(4), Error::InvalidBinding);
    EXPECT_EQ(environment.value->active().size(), 3U);
    ASSERT_EQ(environment.value->unwind(1), Error::None);
    EXPECT_EQ(store.stats().live, 1U);
    EXPECT_FALSE(store.retain(previous));
    environment.value->finish();
    environment.value->finish();
    EXPECT_EQ(environment.value->enter(s::ActivationScopeId{1}), Error::InvalidBinding);
    EXPECT_EQ(environment.value->unwind(0), Error::InvalidBinding);
    EXPECT_EQ(store.stats().live, 0U);
}

TEST(ActivationEnvironment, RootlessBodiesEnterAndLeaveTheirFirstLexicalBlock) {
    k::ActivationStore store;
    Body body;
    body.frame = {{"number", s::u4(32)}};
    body.frame[0].home = s::FrameHome::Scoped;
    body.frame[0].activationScope = s::ActivationScopeId{0};
    body.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto environment = Env::root(store, plan.value);
    ASSERT_TRUE(environment);
    EXPECT_TRUE(environment.value->active().empty());
    EXPECT_FALSE(environment.value->binding(s::FrameVarId{0}));
    ASSERT_EQ(environment.value->enter(s::ActivationScopeId{0}), Error::None);
    ASSERT_EQ(environment.value->leave(s::ActivationScopeId{0}), Error::None);
    EXPECT_EQ(store.stats().live, 0U);
    EXPECT_EQ(environment.value->unwind(0), Error::None);
}

TEST(ActivationEnvironment, ReadonlyViewsKeepIdentityWithoutRegainingWriteRights) {
    k::ActivationStore store;
    const Body body = ownerBody();
    Body child = childBody(body);
    child.frame[0].readonly = true;
    auto plan = body.compile(store), childPlan = child.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    auto parent = Env::root(store, plan.value);
    ASSERT_TRUE(parent);
    enterBlocks(*parent.value);
    const std::array plans{&childPlan.value};
    auto children = Env::children(store, plan.value, parent.value.get(), plans, Join::Any);
    ASSERT_TRUE(children);
    const auto value = binding(*children.value[0], 0);
    const k::W4 number{7, 0};
    EXPECT_TRUE(value.readonly());
    EXPECT_EQ(store.write(value, 0, {&number, 1}, 0, 32), Error::ReadOnly);
    write(store, binding(*parent.value, 1), 37);
    EXPECT_EQ(read(store, value), 37U);
    Body grand;
    grand.frame = {child.frame[0]};
    grand.frame[0].readonly = false;
    grand.frame[0].captureSource = s::FrameVarId{0};
    grand.activation.extents = {s::ActivationScopeId{0}};
    grand.activation.inheritCaptures = true;
    auto grandPlan = grand.compile(store);
    ASSERT_TRUE(grandPlan);
    const std::array grandPlans{&grandPlan.value};
    const auto before = store.stats();
    auto forbidden =
        Env::children(store, childPlan.value, children.value[0].get(), grandPlans, Join::All);
    EXPECT_FALSE(forbidden);
    EXPECT_EQ(store.stats().leases, before.leases);
    EXPECT_EQ(store.stats().live, before.live);
}

struct ReferenceSource {
    v::BorrowedReference value{{0x123456789abcdef0ULL, 0x8123456789abcdefULL}, false, true};
    Error error = Error::None;
    unsigned calls = 0;
    static k::ActivationResult<v::BorrowedReference> resolve(void* frame, s::FrameVarId) noexcept {
        auto& source = *static_cast<ReferenceSource*>(frame);
        ++source.calls;
        return {source.value, source.error};
    }
};

TEST(ActivationEnvironment, BorrowedReferencesPreservePayloadQualificationAndActualRights) {
    unsigned cases = 0;
    for (const bool qualified : {false, true})
        for (const bool staticActual : {false, true})
            for (const auto join : {Join::All, Join::Any, Join::None}) {
                k::ActivationStore store;
                Body parent, child;
                parent.frame = {{"formal", s::refType(), s::kNone, {{-1, 1}}, s::u4(129)}};
                parent.frame[0].refStatic = qualified;
                child.frame = parent.frame;
                child.frame[0].home = s::FrameHome::Capture;
                child.frame[0].captureSource = s::FrameVarId{0};
                auto parentPlan = parent.compile(store), childPlan = child.compile(store);
                ASSERT_TRUE(parentPlan);
                ASSERT_TRUE(childPlan);
                ReferenceSource source;
                source.value.staticActual = staticActual;
                const std::array plans{&childPlan.value};
                auto result = Env::children(store, parentPlan.value, nullptr, plans, join,
                                            &ReferenceSource::resolve, &source);
                const bool allowed = qualified ? staticActual : join == Join::All;
                ASSERT_EQ(static_cast<bool>(result), allowed);
                if (allowed) {
                    const auto reference = result.value[0]->reference(s::FrameVarId{0});
                    ASSERT_TRUE(reference);
                    EXPECT_EQ(reference.value.token.a, source.value.token.a);
                    EXPECT_EQ(reference.value.token.b, source.value.token.b);
                    EXPECT_EQ(reference.value.staticActual, staticActual);
                    EXPECT_EQ(store.stats().leases, 0U);
                }
                ++cases;
            }
    EXPECT_EQ(cases, 12U);
}

TEST(ActivationEnvironment, EffectiveConstRightsAndReferenceErrorsCannotBeLostDuringCapture) {
    k::ActivationStore store;
    Body parent, child;
    parent.frame = {{"formal", s::refType(), s::kNone, {}, s::u4(32)}};
    parent.frame[0].refStatic = true;
    child.frame = parent.frame;
    child.frame[0].home = s::FrameHome::Capture;
    child.frame[0].captureSource = s::FrameVarId{0};
    auto parentPlan = parent.compile(store), writablePlan = child.compile(store);
    ASSERT_TRUE(parentPlan);
    ASSERT_TRUE(writablePlan);
    ReferenceSource source;
    source.value.readonly = true;
    const std::array writable{&writablePlan.value};
    EXPECT_FALSE(Env::children(store, parentPlan.value, nullptr, writable, Join::None,
                               &ReferenceSource::resolve, &source));
    Body readonlyChild = child;
    readonlyChild.frame[0].readonly = true;
    auto readonlyPlan = readonlyChild.compile(store);
    ASSERT_TRUE(readonlyPlan);
    const std::array readonly{&readonlyPlan.value};
    auto result = Env::children(store, parentPlan.value, nullptr, readonly, Join::None,
                                &ReferenceSource::resolve, &source);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value[0]->reference(s::FrameVarId{0}).value.readonly);
    source.error = Error::ResourceLimit;
    EXPECT_EQ(Env::children(store, parentPlan.value, nullptr, readonly, Join::None,
                            &ReferenceSource::resolve, &source)
                  .error,
              Error::ResourceLimit);
    EXPECT_EQ(Env::children(store, parentPlan.value, nullptr, readonly, Join::None).error,
              Error::InvalidBinding);
}

TEST(ActivationEnvironment, PartialGroupsRollbackAndReuseReleasedStorage) {
    k::ActivationStore store;
    const Body body = ownerBody(), child = childBody(body);
    Body incompatible = child;
    incompatible.frame[0].type = s::u4(128);
    auto plan = body.compile(store), childPlan = child.compile(store),
         badPlan = incompatible.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    ASSERT_TRUE(badPlan);
    auto parent = Env::root(store, plan.value);
    ASSERT_TRUE(parent);
    enterBlocks(*parent.value);
    const std::array plans{&childPlan.value, &badPlan.value};
    const auto before = store.stats();
    for (unsigned i = 0; i < 1000; ++i) {
        const auto result = Env::children(store, plan.value, parent.value.get(), plans, Join::None);
        EXPECT_EQ(result.error, Error::InvalidBinding);
        EXPECT_TRUE(result.value.empty());
        EXPECT_EQ(store.stats().live, before.live);
        EXPECT_EQ(store.stats().leases, before.leases);
        EXPECT_EQ(store.stats().words, before.words);
    }
    EXPECT_LE(store.stats().entries, 5U);
}

TEST(ActivationEnvironment, MissingFullChainsOrInheritedExtentsNeverCaptureOrdinaryValues) {
    k::ActivationStore store;
    const Body body = ownerBody();
    Body child = childBody(body);
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto parent = Env::root(store, plan.value);
    ASSERT_TRUE(parent);
    enterBlocks(*parent.value);
    for (const auto& extents :
         {std::vector<s::ActivationScopeId>{s::ActivationScopeId{0}, s::ActivationScopeId{1}},
          std::vector<s::ActivationScopeId>{s::ActivationScopeId{0}, s::ActivationScopeId{2},
                                            s::ActivationScopeId{1}}}) {
        child.activation.extents = extents;
        auto childPlan = child.compile(store);
        ASSERT_TRUE(childPlan);
        const std::array plans{&childPlan.value};
        EXPECT_FALSE(Env::children(store, plan.value, parent.value.get(), plans, Join::All));
    }
    Body ordinary, snapshot;
    ordinary.frame = {{"value", s::u4(32)}};
    snapshot.frame = ordinary.frame;
    snapshot.frame[0].home = s::FrameHome::Capture;
    snapshot.frame[0].captureSource = s::FrameVarId{0};
    auto ordinaryPlan = ordinary.compile(store), snapshotPlan = snapshot.compile(store);
    ASSERT_TRUE(ordinaryPlan);
    ASSERT_TRUE(snapshotPlan);
    const std::array snapshots{&snapshotPlan.value};
    EXPECT_FALSE(Env::children(store, ordinaryPlan.value, nullptr, snapshots, Join::All));
    EXPECT_EQ(store.stats().live, 3U);
    EXPECT_EQ(store.stats().leases, 3U);
}

TEST(ActivationEnvironment, PlansAndEnvironmentsCannotCrossStoresOrBeUsedAfterMoves) {
    k::ActivationStore a, b;
    const Body body = ownerBody();
    auto original = body.compile(a), foreign = body.compile(b), other = body.compile(a);
    ASSERT_TRUE(original);
    ASSERT_TRUE(foreign);
    ASSERT_TRUE(other);
    Plan plan(std::move(original.value));
    EXPECT_FALSE(original.value.valid());
    EXPECT_EQ(Env::root(a, original.value).error, Error::InvalidLayout);
    EXPECT_EQ(Env::root(b, plan).error, Error::InvalidLayout);
    auto owner = Env::root(a, plan);
    ASSERT_TRUE(owner);
    EXPECT_EQ(Env::children(a, foreign.value, nullptr, {}, Join::All).error, Error::InvalidBinding);
    EXPECT_EQ(Env::children(a, other.value, owner.value.get(), {}, Join::All).error,
              Error::InvalidBinding);
    EXPECT_EQ(Env::children(a, plan, nullptr, {}, Join::All).error, Error::InvalidBinding);
    EXPECT_EQ(a.stats().live, 1U);
    EXPECT_EQ(b.stats().live, 0U);
}

TEST(ActivationEnvironment, ScopeGraphsUseIterativeConstructionAndLinearExtentRetention) {
    k::ActivationStore store;
    Body body;
    constexpr std::uint32_t count = 10000;
    body.activation.root = s::ActivationScopeId{0};
    body.activation.scopes.resize(count);
    for (std::uint32_t i = 1; i < count; ++i)
        body.activation.scopes[i].parent = s::ActivationScopeId{i - 1};
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto owner = Env::root(store, plan.value);
    ASSERT_TRUE(owner);
    for (std::uint32_t i = 1; i < count; ++i)
        ASSERT_EQ(owner.value->enter(s::ActivationScopeId{i}), Error::None);
    auto pins = owner.value->pin();
    ASSERT_TRUE(pins);
    EXPECT_EQ(pins.value.size(), count);
    EXPECT_EQ(store.stats().live, count);
    EXPECT_EQ(store.stats().leases, 2 * count);
    owner.value.reset();
    EXPECT_EQ(store.stats().live, count);
    pins.value.clear();
    EXPECT_EQ(store.stats().live, 0U);
}

class FailingMemory final : public std::pmr::memory_resource {
public:
    std::optional<std::size_t> remaining;
    std::size_t allocated = 0;
    bool matchingDeallocation = true;
    bool owns(const void* pointer) const {
        return pointer != nullptr && std::find(live_.begin(), live_.end(), pointer) != live_.end();
    }

private:
    std::array<void*, 64> live_{};
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining) {
            if (*remaining == 0) throw std::bad_alloc{};
            --*remaining;
        }
        void* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        if (const auto at = std::find(live_.begin(), live_.end(), nullptr); at != live_.end())
            *at = result;
        allocated += bytes;
        return result;
    }
    void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
        if (const auto at = std::find(live_.begin(), live_.end(), value); at != live_.end())
            *at = nullptr;
        else
            matchingDeallocation = false;
        allocated -= bytes;
        std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ActivationEnvironment, RootObjectsUseTheirResourceAndMatchingDeallocation) {
    FailingMemory memory;
    k::ActivationStore store;
    const Body body = ownerBody();
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    memory.remaining = 0;
    EXPECT_EQ(Env::root(store, plan.value, &memory).error, Error::ResourceLimit);
    EXPECT_EQ(memory.allocated, 0U);
    EXPECT_EQ(store.stats().live, 0U);
    memory.remaining.reset();
    auto owner = Env::root(store, plan.value, &memory);
    ASSERT_TRUE(owner);
    const auto* address = owner.value.get();
    EXPECT_TRUE(memory.owns(address));
    owner.value.reset();
    EXPECT_FALSE(memory.owns(address));
    EXPECT_TRUE(memory.matchingDeallocation);
    EXPECT_EQ(memory.allocated, 0U);
    EXPECT_EQ(store.stats().live, 0U);
}

TEST(ActivationEnvironment, EveryGroupAllocationFailureReleasesAllProvisionalOwnership) {
    for (const bool kernelFailure : {false, true}) {
        unsigned failed = 0;
        bool succeeded = false;
        for (std::size_t budget = 0; budget < 100 && !succeeded; ++budget) {
            FailingMemory kernelMemory, environmentMemory;
            {
                k::ActivationStore store(&kernelMemory);
                const Body body = ownerBody(), child = childBody(body);
                auto plan = body.compile(store), childPlan = child.compile(store);
                ASSERT_TRUE(plan);
                ASSERT_TRUE(childPlan);
                auto parent = Env::root(store, plan.value);
                ASSERT_TRUE(parent);
                enterBlocks(*parent.value);
                const auto actual = binding(*parent.value, 1);
                write(store, actual, 101);
                const auto before = store.stats();
                const std::array plans{&childPlan.value, &childPlan.value, &childPlan.value};
                (kernelFailure ? kernelMemory : environmentMemory).remaining = budget;
                {
                    auto result = Env::children(store, plan.value, parent.value.get(), plans,
                                                Join::None, nullptr, nullptr, &environmentMemory);
                    if (result) {
                        succeeded = true;
                        EXPECT_EQ(result.value.size(), 3U);
                    } else {
                        ++failed;
                        EXPECT_EQ(result.error, Error::ResourceLimit);
                        EXPECT_TRUE(result.value.empty());
                    }
                }
                EXPECT_EQ(store.stats().live, before.live);
                EXPECT_EQ(store.stats().leases, before.leases);
                EXPECT_EQ(store.stats().words, before.words);
                EXPECT_EQ(read(store, actual), 101U);
                EXPECT_EQ(environmentMemory.allocated, 0U);
            }
            EXPECT_EQ(kernelMemory.allocated, 0U);
            EXPECT_EQ(environmentMemory.allocated, 0U);
            EXPECT_TRUE(kernelMemory.matchingDeallocation);
            EXPECT_TRUE(environmentMemory.matchingDeallocation);
        }
        EXPECT_TRUE(succeeded);
        EXPECT_GE(failed, kernelFailure ? 3U : 10U);
    }
}

TEST(ActivationEnvironment, PinFailureAndScopeEntryFailurePreserveTheExecutingEnvironment) {
    FailingMemory kernelMemory, environmentMemory;
    k::ActivationStore store(&kernelMemory);
    const Body body = ownerBody();
    auto plan = body.compile(store);
    ASSERT_TRUE(plan);
    auto owner = Env::root(store, plan.value, &environmentMemory);
    ASSERT_TRUE(owner);
    const auto before = store.stats();
    kernelMemory.remaining = 0;
    EXPECT_EQ(owner.value->enter(s::ActivationScopeId{1}), Error::ResourceLimit);
    EXPECT_EQ(store.stats().live, before.live);
    EXPECT_EQ(store.stats().leases, before.leases);
    EXPECT_EQ(owner.value->active().size(), 1U);
    kernelMemory.remaining.reset();
    enterBlocks(*owner.value);
    const auto leases = store.stats().leases;
    environmentMemory.remaining = 0;
    EXPECT_EQ(owner.value->pin().error, Error::ResourceLimit);
    EXPECT_EQ(store.stats().leases, leases);
    owner.value->finish();
    EXPECT_EQ(store.stats().live, 0U);  // Cleanup does not allocate.
    EXPECT_FALSE(owner.value->binding(s::FrameVarId{0}));
}

TEST(ActivationEnvironment, UnreadTypedFieldsStayRootsUntilTheLastDescendantFinishes) {
    k::Context context;
    auto& store = context.activations();
    auto& heap = context.heap();
    const auto layout = heap.addLayout(std::array{k::HeapWord{}});
    ASSERT_TRUE(layout);
    const auto type = heap.addType({k::ContainerKind::DynamicArray, layout.value, 0});
    ASSERT_TRUE(type);
    const auto kept = heap.construct(type.value, 2), unrooted = heap.construct(type.value, 3);
    ASSERT_TRUE(kept);
    ASSERT_TRUE(unrooted);
    const Body body = ownerBody(), child = childBody(body);
    auto plan = body.compile(store), childPlan = child.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    auto parent = Env::root(store, plan.value);
    ASSERT_TRUE(parent);
    enterBlocks(*parent.value);
    const k::W4 container{kept.value.bits, 0}, integer{unrooted.value.bits, 0};
    ASSERT_EQ(store.write(binding(*parent.value, 2), 0, {&container, 1}), Error::None);
    ASSERT_EQ(store.write(binding(*parent.value, 0), 0, {&integer, 1}), Error::None);
    const std::array plans{&childPlan.value, &childPlan.value};
    auto children = Env::children(store, plan.value, parent.value.get(), plans, Join::None);
    ASSERT_TRUE(children);
    parent.value.reset();
    struct Roots {
        std::array<k::HeapRoot, 2> values{};
        std::size_t count = 0;
        static void append(void* frame, k::HeapRoot root) noexcept {
            auto& roots = *static_cast<Roots*>(frame);
            if (roots.count < roots.values.size()) roots.values[roots.count] = root;
            ++roots.count;
        }
    } roots;
    store.visitRoots(&Roots::append, &roots);
    ASSERT_EQ(roots.count, 1U);  // Captured fields are not duplicate physical holders.
    ASSERT_TRUE(heap.collect(std::span(roots.values).first(roots.count)));
    EXPECT_TRUE(heap.actualType(kept.value));
    EXPECT_FALSE(heap.actualType(unrooted.value));
    children.value.clear();
    roots.count = 0;
    store.visitRoots(&Roots::append, &roots);
    EXPECT_EQ(roots.count, 0U);
    ASSERT_TRUE(heap.collect({}));
    EXPECT_FALSE(heap.actualType(kept.value));
}

TEST(ActivationEnvironment, StaticReferenceForwardingNeverFollowsAReusedFormalBinding) {
    k::ActivationStore store;
    Body body, child, grand;
    body.frame = {{"formal", s::refType(), s::kNone, {}, s::u4(32)}};
    body.frame[0].refStatic = true;
    child.frame = body.frame;
    child.frame[0].home = s::FrameHome::Capture;
    child.frame[0].captureSource = s::FrameVarId{0};
    child.frame[0].readonly = true;
    grand = child;
    grand.activation.inheritCaptures = true;
    auto plan = body.compile(store), childPlan = child.compile(store),
         grandPlan = grand.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    ASSERT_TRUE(grandPlan);
    ReferenceSource source;
    source.value.token = {41, 0};
    const std::array childPlans{&childPlan.value}, grandPlans{&grandPlan.value};
    auto middle = Env::children(store, plan.value, nullptr, childPlans, Join::None,
                                &ReferenceSource::resolve, &source);
    ASSERT_TRUE(middle);
    auto last =
        Env::children(store, childPlan.value, middle.value[0].get(), grandPlans, Join::None);
    ASSERT_TRUE(last);
    EXPECT_EQ(source.calls, 1U);
    source.value.token = {97, 6};
    middle.value.clear();
    const auto reference = last.value[0]->reference(s::FrameVarId{0});
    ASSERT_TRUE(reference);
    EXPECT_EQ(reference.value.token.a, 41U);
    EXPECT_EQ(reference.value.token.b, 0U);
    EXPECT_TRUE(reference.value.readonly);
    EXPECT_TRUE(reference.value.staticActual);
    struct References {
        unsigned count = 0;
        v::BorrowedReference value;
    } references;
    last.value[0]->visitReferences(
        [](void* frame, v::BorrowedReference value) noexcept {
            auto& result = *static_cast<References*>(frame);
            ++result.count;
            result.value = value;
        },
        &references);
    EXPECT_EQ(references.count, 1U);
    EXPECT_EQ(references.value.token.a, 41U);
    EXPECT_EQ(store.stats().leases, 0U);
}

TEST(ActivationEnvironment, UnsupportedOwnedProvidersAndMalformedTablesAreRejected) {
    unsigned cases = 0;
    for (const auto type :
         {s::Type{s::TypeKind::Event, 1}, s::refType(), s::voidType(), s::u4(0)}) {
        k::ActivationStore store;
        Body body;
        body.frame = {{"field", type}};
        body.frame[0].home = s::FrameHome::Scoped;
        body.frame[0].activationScope = s::ActivationScopeId{0};
        body.activation.root = s::ActivationScopeId{0};
        body.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
        EXPECT_EQ(body.compile(store).error, Error::InvalidLayout);
        EXPECT_EQ(store.stats().live, 0U);
        ++cases;
    }
    for (unsigned kind = 0; kind < 7; ++kind) {
        k::ActivationStore store;
        Body body = ownerBody();
        if (kind == 0) body.activation.scopes[0].parent = s::ActivationScopeId{2};
        if (kind == 1) body.activation.scopes[0].fields.push_back(s::FrameVarId{0});
        if (kind == 2) body.activation.scopes[1].fields.clear();
        if (kind == 3)
            body.frame[0].dims = {{std::numeric_limits<std::int64_t>::min(),
                                   std::numeric_limits<std::int64_t>::max()}};
        if (kind == 4) body.frame[0].refStatic = true;
        if (kind == 5) body.frame[0].readonly = true;
        if (kind == 6) {
            body.frame.push_back({"ordinary", s::u4(32)});
            body.frame.back().readonly = true;
        }
        EXPECT_EQ(body.compile(store).error, Error::InvalidLayout);
        EXPECT_EQ(store.stats().live, 0U);
        ++cases;
    }
    EXPECT_EQ(cases, 11U);
}

TEST(ActivationEnvironment, SupportedStorageTypesKeepWidthsShapesAndKernelNormalization) {
    std::vector<s::Type> types;
    for (const auto width : {1U, 32U, 64U, 65U, 129U, 257U})
        for (const auto type : {s::u2(width), s::s2(width), s::u4(width), s::s4(width)})
            types.push_back(type);
    for (const auto type : {s::Type{s::TypeKind::Real, 0}, s::Type{s::TypeKind::ShortReal, 0},
                            s::Type{s::TypeKind::String, 0}, s::Type{s::TypeKind::Chandle, 0},
                            s::dynarrayType(s::TypeId{0}), s::queueType(s::TypeId{0})})
        types.push_back(type);
    unsigned cases = 0;
    for (const auto type : types) {
        for (const auto& dims :
             {std::vector<s::ArrayDim>{}, std::vector<s::ArrayDim>{{-1, 1}},
              std::vector<s::ArrayDim>{{3, 1}}, std::vector<s::ArrayDim>{{2, 1}, {-2, 0}}}) {
            SCOPED_TRACE(::testing::Message() << static_cast<unsigned>(type.kind) << '/'
                                              << type.width << '/' << dims.size());
            k::ActivationStore store;
            Body body;
            body.frame = {{"field", type, s::kNone, dims}};
            body.frame[0].home = s::FrameHome::Scoped;
            body.frame[0].activationScope = s::ActivationScopeId{0};
            body.activation.root = s::ActivationScopeId{0};
            body.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
            auto plan = body.compile(store);
            ASSERT_TRUE(plan);
            auto owner = Env::root(store, plan.value);
            ASSERT_TRUE(owner);
            const auto field = binding(*owner.value, 0);
            const auto region = store.region({field});
            ASSERT_TRUE(region);
            const auto width = s::isIntegral(type)                   ? type.width
                               : type.kind == s::TypeKind::ShortReal ? 32U
                                                                     : 64U;
            EXPECT_EQ(region.value.width, width);
            const auto elements = dims.empty() ? 1U : dims.size() == 1 ? 3U : 6U;
            EXPECT_EQ(region.value.numElems, 1U);
            std::vector<k::W4> value((width + 63) / 64), actual(value.size());
            if (s::isIntegral(type))
                for (auto& word : value)
                    word = {0xa5a5a5a5a5a5a5a5ULL, 0xf0f0f0f0f0f0f0f0ULL};
            ASSERT_EQ(store.write(field, elements - 1, value), Error::None);
            ASSERT_EQ(store.read(field, elements - 1, actual), Error::None);
            for (std::uint32_t bit = 0; bit < value.size() * 64; ++bit) {
                const auto mask = std::uint64_t{1} << (bit % 64);
                const bool a = (value[bit / 64].a & mask) != 0;
                const bool b = (value[bit / 64].b & mask) != 0;
                EXPECT_EQ((actual[bit / 64].a & mask) != 0,
                          bit < width &&
                              (s::isIntegral(type) && !s::isFourState(type) ? a && !b : a));
                EXPECT_EQ((actual[bit / 64].b & mask) != 0,
                          bit < width && s::isFourState(type) && b);
            }
            EXPECT_EQ(store.read(field, elements, actual), Error::OutOfRange);
            ++cases;
        }
    }
    EXPECT_EQ(cases, 120U);
}

TEST(ActivationEnvironment, ForwardedReferenceMetadataMustDeclareInheritedEnvironment) {
    k::ActivationStore store;
    Body body, child, grand;
    body.frame = {{"formal", s::refType(), s::kNone, {}, s::u4(32)}};
    body.frame[0].refStatic = true;
    child.frame = body.frame;
    child.frame[0].home = s::FrameHome::Capture;
    child.frame[0].captureSource = s::FrameVarId{0};
    grand = child;  // Even an empty extent chain needs explicit forwarding metadata.
    auto plan = body.compile(store), childPlan = child.compile(store),
         grandPlan = grand.compile(store);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    ASSERT_TRUE(grandPlan);
    ReferenceSource source;
    const std::array childPlans{&childPlan.value}, grandPlans{&grandPlan.value};
    auto middle = Env::children(store, plan.value, nullptr, childPlans, Join::None,
                                &ReferenceSource::resolve, &source);
    ASSERT_TRUE(middle);
    EXPECT_EQ(
        Env::children(store, childPlan.value, middle.value[0].get(), grandPlans, Join::None).error,
        Error::InvalidBinding);
    EXPECT_EQ(source.calls, 1U);
}

}  // namespace
