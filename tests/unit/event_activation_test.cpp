// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original typed-field/provider tests for automatic-capture contract 5/6/7.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "activation_environment.h"
#include "frame_layout.h"
#include "frame_value.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
namespace v = kirimasim::vm::detail;
using Env = v::ActivationEnvironment;
using Plan = v::ActivationPlan;
using Error = k::ActivationError;
const s::Type kEvent{s::TypeKind::Event, 0};

struct Body {
    std::vector<s::FrameVar> frame;
    s::ActivationInfo activation;
    auto compile(k::ActivationStore& store) const {
        return Plan::compile(store, frame, activation);
    }
};
Body ownerBody(std::vector<s::ArrayDim> dimensions = {{2, 0}}) {
    Body body;
    body.frame = {{"event", kEvent},
                  {"unread", kEvent, s::kNone, std::move(dimensions)},
                  {"integer", s::u2(64)},
                  {"block", kEvent}};
    for (std::size_t i = 0; i < body.frame.size(); ++i) {
        body.frame[i].home = s::FrameHome::Scoped;
        body.frame[i].activationScope = s::ActivationScopeId{i == 3 ? 1U : 0U};
    }
    body.activation.root = s::ActivationScopeId{0};
    body.activation.scopes = {{s::kNone, {s::FrameVarId{0}, s::FrameVarId{1}, s::FrameVarId{2}}},
                              {s::ActivationScopeId{0}, {s::FrameVarId{3}}}};
    return body;
}
Body childBody(const Body& parent, unsigned row = 0) {
    Body child;
    child.frame = {parent.frame[row]};
    child.frame[0].home = s::FrameHome::Capture;
    child.frame[0].activationScope = s::kNone;
    child.frame[0].captureSource = s::FrameVarId{row};
    child.activation.extents = {s::ActivationScopeId{0}, s::ActivationScopeId{1}};
    return child;
}
k::ActivationBinding binding(const Env& environment, unsigned row) {
    auto result = environment.binding(s::FrameVarId{row});
    EXPECT_TRUE(result);
    return result.value;
}
void write(k::Context& context, k::ActivationBinding place, std::uint64_t bits,
           std::uint32_t element = 0) {
    const k::W4 value{bits, 0};
    v::FrameValue access(context.activations(), context.scheduler(), place, element, 64);
    ASSERT_EQ(access.write({&value, 1}), Error::None);
}
std::uint64_t read(k::Context& context, k::ActivationBinding place, std::uint32_t element = 0) {
    k::W4 value;
    v::FrameValue access(context.activations(), context.scheduler(), place, element, 64);
    EXPECT_EQ(access.read({&value, 1}), Error::None);
    EXPECT_EQ(value.b, 0U);
    return value.a;
}
k::ManagedEventError collect(k::Context& context) {
    auto& fields = context.activations();
    auto& events = context.managedEvents();
    if (fields.eventRootsChanged()) events.rootsChanged();
    const std::array providers{fields.eventRootProvider()};
    const auto result = events.collect(providers);
    if (result == k::ManagedEventError::None) fields.acknowledgeEventRoots();
    return result;
}
std::vector<k::ManagedEventToken> initialize(k::Context& context, const Body& body,
                                             Env& environment) {
    std::vector<k::ManagedEventToken> tokens;
    for (unsigned row : {0U, 1U, 3U})
        for (std::uint32_t e = 0; e < s::elementCount(body.frame[row]); ++e) {
            auto created = context.managedEvents().create();
            EXPECT_TRUE(created);
            if (!created) return {};
            const auto token = created.value.token();
            write(context, binding(environment, row), token.bits, e);
            tokens.push_back(token);
        }
    return tokens;
}

TEST(EventActivation, CanonicalScalarAndArrayFieldsVerifyWithTheirCaptureMetadata) {
    for (const auto& dims :
         {std::vector<s::ArrayDim>{}, std::vector<s::ArrayDim>{{-1, 1}},
          std::vector<s::ArrayDim>{{5, 2}}, std::vector<s::ArrayDim>{{2, 1}, {-1, 1}}}) {
        const auto owner = ownerBody(dims), child = childBody(owner);
        s::Design design;
        design.sources = {"event_activation.sv"};
        design.instances = {{"top", "top", s::kNone}};
        s::Process parent;
        parent.name = "top.owner";
        parent.loc = {0, 1, 1};
        parent.frame = owner.frame;
        parent.activation = owner.activation;
        const auto make = [](s::Op op, std::vector<s::Operand> args = {}) {
            return s::Instr{op, s::voidType(), s::kNone, std::move(args), {0, 1, 1}};
        };
        parent.blocks = {
            {{},
             {make(s::Op::ActivationEnter, {s::Operand::activation(s::ActivationScopeId{1})}),
              make(s::Op::Fork, {s::Operand::process(s::ProcessId{1}), s::Operand::str("join_none"),
                                 s::Operand::block(s::BlockId{1})})}},
            {{},
             {make(s::Op::ActivationLeave, {s::Operand::activation(s::ActivationScopeId{1})}),
              make(s::Op::Ret)}}};
        design.processes.push_back(parent);
        s::Process branch;
        branch.name = "top.child";
        branch.kind = s::ProcessKind::General;
        branch.loc = {0, 1, 1};
        branch.parent = s::Owner::of(s::ProcessId{0});
        branch.frame = child.frame;
        branch.activation = child.activation;
        branch.blocks = {{{}, {make(s::Op::Ret)}}};
        design.processes.push_back(branch);
        const auto errors = s::verify(design);
        for (const auto& error : errors)
            ADD_FAILURE() << error.message;
        ASSERT_TRUE(errors.empty());
        const auto text = s::toText(design);
        EXPECT_NE(text.find("\"event\" : event home=activation#0"), std::string::npos);
        EXPECT_NE(text.find("\"event\" : event home=capture%f0"), std::string::npos);
    }
}

TEST(EventActivation, UnreadArrayElementsSurviveThroughWholeCapturedExtents) {
    for (auto join : {v::CaptureJoin::All, v::CaptureJoin::Any, v::CaptureJoin::None})
        for (const auto& dims :
             {std::vector<s::ArrayDim>{}, std::vector<s::ArrayDim>{{-1, 1}},
              std::vector<s::ArrayDim>{{5, 2}}, std::vector<s::ArrayDim>{{2, 1}, {-1, 1}}}) {
            k::Context context;
            const auto owner = ownerBody(dims), child = childBody(owner, 2);
            auto plan = owner.compile(context.activations());
            auto childPlan = child.compile(context.activations());
            ASSERT_TRUE(plan);
            ASSERT_TRUE(childPlan);
            auto environment = Env::root(context.activations(), plan.value);
            ASSERT_TRUE(environment);
            ASSERT_EQ(environment.value->enter(s::ActivationScopeId{1}), Error::None);
            const auto tokens = initialize(context, owner, *environment.value);
            ASSERT_EQ(tokens.size(), 2U + s::elementCount(owner.frame[1]));
            const std::array plans{&childPlan.value, &childPlan.value};
            auto children = Env::children(context.activations(), plan.value,
                                          environment.value.get(), plans, join);
            ASSERT_TRUE(children);
            environment.value->finish();
            ASSERT_EQ(collect(context), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), tokens.size());
            for (auto token : tokens)
                EXPECT_TRUE(context.managedEvents().valid(token));
            children.value[0]->finish();
            ASSERT_EQ(collect(context), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), tokens.size());
            children.value[1]->finish();
            EXPECT_TRUE(context.activations().eventRootsChanged());
            ASSERT_EQ(collect(context), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
            for (auto token : tokens)
                EXPECT_FALSE(context.managedEvents().valid(token));
        }
}

TEST(EventActivation, CapturedValuesShareTheObjectAndAnActiveWaitPinsItAfterScopeRelease) {
    k::Context context;
    const auto owner = ownerBody(), child = childBody(owner);
    auto plan = owner.compile(context.activations()),
         childPlan = child.compile(context.activations());
    ASSERT_TRUE(plan);
    ASSERT_TRUE(childPlan);
    auto environment = Env::root(context.activations(), plan.value);
    ASSERT_TRUE(environment);
    ASSERT_EQ(environment.value->enter(s::ActivationScopeId{1}), Error::None);
    auto created = context.managedEvents().create();
    ASSERT_TRUE(created);
    const auto token = created.value.token();
    write(context, binding(*environment.value, 0), token.bits);
    const std::array plans{&childPlan.value};
    auto children = Env::children(context.activations(), plan.value, environment.value.get(), plans,
                                  v::CaptureJoin::None);
    ASSERT_TRUE(children);
    EXPECT_EQ(read(context, binding(*children.value[0], 0)), token.bits);
    const auto process = context.scheduler().addProcess(nullptr, nullptr, k::Group::Active);
    ASSERT_EQ(context.scheduler().waitNamedValue(process, k::EventValue::managed(token)),
              k::ManagedEventError::None);
    created.value.reset();
    environment.value->finish();
    children.value[0]->finish();
    ASSERT_EQ(collect(context), k::ManagedEventError::None);
    EXPECT_TRUE(context.managedEvents().valid(token));
    EXPECT_EQ(context.activations().stats().live, 0U);
    context.scheduler().cancelWait(process);
    ASSERT_EQ(collect(context), k::ManagedEventError::None);
    EXPECT_FALSE(context.managedEvents().valid(token));
}

TEST(EventActivation, ReentryAndReturnReusePhysicalRecordsWithoutAClockOrHeapMutation) {
    k::Context context;
    const auto owner = ownerBody();
    auto plan = owner.compile(context.activations());
    ASSERT_TRUE(plan);
    k::ManagedEventToken previous;
    for (unsigned i = 0; i < 512; ++i) {
        auto environment = Env::root(context.activations(), plan.value);
        ASSERT_TRUE(environment);
        auto created = context.managedEvents().create();
        ASSERT_TRUE(created);
        const auto token = created.value.token();
        EXPECT_NE(token, previous);
        write(context, binding(*environment.value, 0), token.bits);
        created.value.reset();
        ASSERT_EQ(collect(context), k::ManagedEventError::None);
        EXPECT_TRUE(context.managedEvents().valid(token));
        EXPECT_FALSE(context.managedEvents().collectionEligible());
        environment.value->finish();
        EXPECT_TRUE(context.activations().eventRootsChanged());
        ASSERT_EQ(collect(context), k::ManagedEventError::None);
        EXPECT_FALSE(context.managedEvents().valid(token));
        EXPECT_EQ(context.managedEvents().allocatedObjects(), 1U);
        EXPECT_EQ(context.activations().stats().live, 0U);
        previous = token;
    }
    EXPECT_EQ(context.scheduler().time(), 0U);
}

TEST(EventActivation, IntegerBitsCannotKeepAReleasedEventAlive) {
    k::Context context;
    const auto owner = ownerBody();
    Body integer;
    integer.frame = {{"integer", s::u2(64)}};
    integer.frame[0].home = s::FrameHome::Scoped;
    integer.frame[0].activationScope = s::ActivationScopeId{0};
    integer.activation.root = s::ActivationScopeId{0};
    integer.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
    auto plan = owner.compile(context.activations()),
         intPlan = integer.compile(context.activations());
    ASSERT_TRUE(plan);
    ASSERT_TRUE(intPlan);
    auto environment = Env::root(context.activations(), plan.value);
    auto numbers = Env::root(context.activations(), intPlan.value);
    ASSERT_TRUE(environment);
    ASSERT_TRUE(numbers);
    auto created = context.managedEvents().create();
    ASSERT_TRUE(created);
    const auto token = created.value.token();
    write(context, binding(*environment.value, 0), token.bits);
    write(context, binding(*numbers.value, 0), token.bits);
    created.value.reset();
    ASSERT_EQ(collect(context), k::ManagedEventError::None);
    EXPECT_TRUE(context.managedEvents().valid(token));
    environment.value->finish();
    ASSERT_EQ(collect(context), k::ManagedEventError::None);
    EXPECT_FALSE(context.managedEvents().valid(token));
    EXPECT_EQ(read(context, binding(*numbers.value, 0)), token.bits);
    EXPECT_EQ(context.activations().stats().live, 1U);
}

TEST(EventActivation, OrdinaryEventRootRowsAreSeparateFromHeapAndPromotedRoots) {
    std::vector<s::FrameVar> fields{
        {"scalar", kEvent},  {"array", kEvent, s::kNone, {{2, 0}}},
        {"bits", s::u2(64)}, {"scoped", kEvent},
        {"capture", kEvent}, {"ref", s::refType(), s::kNone, {}, kEvent}};
    fields[3].home = s::FrameHome::Scoped;
    fields[4].home = s::FrameHome::Capture;
    const v::FrameLayout layout(fields);
    ASSERT_EQ(layout.eventRoots.size(), 2U);
    EXPECT_EQ(layout.eventRoots[0], 0U);
    EXPECT_EQ(layout.eventRoots[1], 1U);
    ASSERT_EQ(layout.roots.size(), 1U);
    EXPECT_EQ(layout.roots[0], 5U);
    EXPECT_EQ(layout.chunks[0], 1U);
    EXPECT_EQ(layout.chunks[1], 1U);
    EXPECT_GT(layout.wordOf(s::FrameVarId{1}), layout.wordOf(s::FrameVarId{2}));
}
}  // namespace
