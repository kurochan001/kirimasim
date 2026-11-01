// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Independently authored checks of the approved specify contract §§2–6.
#include <array>
#include <exception>
#include <memory_resource>
#include <new>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/edge.h"
#include "kirimasim/valsem/module_path.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using R = k::StrengthRange;
using E = k::StrengthError;
k::StrengthOriginState state(const k::Context& ctx, unsigned id) {
    const auto value = ctx.strengthDrivers().origin(id);
    EXPECT_TRUE(value.has_value());
    return value.value_or(k::StrengthOriginState{});
}
R range(unsigned logic) {
    constexpr std::array<R, 4> ranges{{{-6, -6}, {6, 6}, {}, {-6, 6}}};
    return ranges[logic];
}
k::ModulePathDelays delay(std::uint64_t n) {
    k::ModulePathDelays d;
    d.fill(n);
    return d;
}

struct Cell {
    k::Context ctx;
    k::SlotId a, b, q;
    k::SlotBits source;
    std::vector<std::pair<std::uint64_t, R>> notices;
    explicit Cell(unsigned initial = 0, unsigned width = 1, unsigned element = 0,
                  unsigned bit = 0) {
        a = ctx.store().add(k::Domain::Four, width, element + 1, k::Init::Zero);
        b = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
        q = ctx.store().add(k::Domain::Four, 1, 1,
                            static_cast<k::Init>(initial == 2   ? 3
                                                 : initial == 3 ? 2
                                                                : initial));
        source = {a, element, bit, 1};
        const R value = range(initial);
        EXPECT_EQ(ctx.scheduler().enableStrengths(q, {&value, 1}), E::None);
        const k::StrengthNodeSpec node{{q, 0, 0, 1}};
        const k::StrengthOriginSpec origin{0, value};
        EXPECT_EQ(ctx.scheduler().configureStrengthDrivers({&node, 1}, {&origin, 1}), E::None);
        EXPECT_EQ(ctx.strengths().subscribe({q, 0, 0, 1}, &record, this).error, E::None);
    }
    static void record(void* raw, k::SlotBits, std::span<const R>,
                       std::span<const R> after) noexcept {
        auto& self = *static_cast<Cell*>(raw);
        try {
            self.notices.emplace_back(self.ctx.scheduler().time(), after[0]);
        } catch (...) {
            // No allocation exception can cross a native observer callback.
            std::terminate();
        }
    }
    void start(std::uint64_t n = 5, k::ModulePathEdge edge = k::ModulePathEdge::Any) {
        const k::ModulePathSpec path{source, 0, edge, delay(n)};
        ASSERT_EQ(ctx.scheduler().configureModulePaths({&path, 1}), E::None);
        ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    }
    void write(k::SlotBits at, unsigned logic) {
        const std::array<std::uint64_t, 2> words{logic == 1 || logic == 3 ? 1U : 0U,
                                                 logic >= 2 ? 1U : 0U};
        ctx.scheduler().write(at, words.data());
    }
    R read() const {
        R value;
        EXPECT_EQ(ctx.strengths().read({q, 0, 0, 1}, {&value, 1}), E::None);
        return value;
    }
};

enum class Act { A, B, Drive, Force, Release, Check, Inactive };
struct Step {
    std::uint64_t time;
    Act kind;
    unsigned value = 0;
};
struct Script {
    Cell& cell;
    std::vector<Step> steps;
    std::size_t next = 0;
    k::ProcessId process;
    static kirima_status run(kirima_context*, void* raw) {
        auto& self = *static_cast<Script*>(raw);
        auto& c = self.cell;
        auto& s = c.ctx.scheduler();
        while (self.next < self.steps.size()) {
            const auto step = self.steps[self.next];
            if (s.time() < step.time) {
                s.waitDelay(self.process, step.time - s.time());
                return static_cast<kirima_status>(k::Status::Waiting);
            }
            ++self.next;
            switch (step.kind) {
                case Act::A: c.write(c.source, step.value); break;
                case Act::B: c.write({c.b, 0, 0, 1}, step.value); break;
                case Act::Drive: EXPECT_EQ(s.driveStrength(0, range(step.value)), E::None); break;
                case Act::Force:
                case Act::Release: {
                    const auto v = step.value;
                    const k::StrengthForceUpdate force{
                        0, step.kind == Act::Release ? std::nullopt
                                                     : std::optional<k::W4>{{v & 1U, v >> 1}}};
                    EXPECT_EQ(s.setStrengthForces({&force, 1}).error, E::None);
                    break;
                }
                case Act::Check: EXPECT_EQ(c.read(), range(step.value)); break;
                case Act::Inactive:
                    s.waitDelay(self.process, 0);
                    return static_cast<kirima_status>(k::Status::Waiting);
            }
        }
        return static_cast<kirima_status>(k::Status::Done);
    }
    void start(k::Group group = k::Group::Active) {
        process = cell.ctx.scheduler().addProcess(&run, this, group);
        cell.ctx.scheduler().start(process);
    }
};

TEST(ModulePathValues, AllAritiesAndAllTransitionsMatchIndependentEndpointReference) {
    constexpr std::array<std::uint64_t, 12> values{17, 5, 13, 7, 19, 11, 3, 23, 2, 29, 31, 37};
    for (unsigned n : {1, 2, 3, 6, 12}) {
        const auto input = std::span(values).first(n);
        const auto normalized = k::modulePathDelays(input);
        ASSERT_TRUE(normalized);
        for (unsigned from = 0; from < 4; ++from)
            for (unsigned to = 0; to < 4; ++to) {
                SCOPED_TRACE(::testing::Message() << n << ':' << from << "->" << to);
                EXPECT_EQ(k::modulePathDelay(normalized.value_or(k::ModulePathDelays{}), from, to),
                          v::modulePathDelay(input, static_cast<v::Logic>(from),
                                             static_cast<v::Logic>(to)));
            }
    }
    for (unsigned n : {0, 4, 5, 7, 8, 9, 10, 11})
        EXPECT_FALSE(k::modulePathDelays(std::span(values).first(n)));
}

class ModulePathTransition : public ::testing::TestWithParam<unsigned> {};
TEST_P(ModulePathTransition, PublishesCompleteOriginAtSelectedAbsoluteDeadline) {
    constexpr std::array<std::uint64_t, 12> values{17, 5, 13, 7, 19, 11, 3, 23, 2, 29, 31, 37};
    constexpr std::array<unsigned, 5> arities{1, 2, 3, 6, 12};
    const auto n = arities[GetParam() / 12];
    const auto pair = GetParam() % 12;
    const auto from = pair / 3;
    const auto to = pair % 3 >= from ? pair % 3 + 1 : pair % 3;
    Cell c(from);
    const auto input = std::span(values).first(n);
    const auto normalized = k::modulePathDelays(input);
    ASSERT_TRUE(normalized.has_value());
    const k::ModulePathSpec path{c.source, 0, k::ModulePathEdge::Any,
                                 normalized.value_or(k::ModulePathDelays{})};
    ASSERT_EQ(c.ctx.scheduler().configureModulePaths({&path, 1}), E::None);
    ASSERT_EQ(c.ctx.scheduler().startStrengthDrivers(), E::None);
    c.write(c.source, 1);
    ASSERT_EQ(c.ctx.scheduler().driveStrength(0, range(to)), E::None);
    EXPECT_EQ(c.read(), range(from));
    ASSERT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(c.notices.size(), 1U);
    const auto expected =
        v::modulePathDelay(input, static_cast<v::Logic>(from), static_cast<v::Logic>(to));
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(c.notices[0], (std::pair{expected.value_or(UINT64_MAX), range(to)}));
}
INSTANTIATE_TEST_SUITE_P(AllSixty, ModulePathTransition, ::testing::Range(0U, 60U));

TEST(ModulePaths, EpochZeroAndScalarSelectionAcrossWideWordsAndElements) {
    Cell c(3, 193, 2, 130);
    c.start(7);
    EXPECT_EQ(c.ctx.scheduler().driveStrength(0, range(1)), E::None);
    EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(c.notices.size(), 1U);
    EXPECT_EQ(c.notices[0], (std::pair{std::uint64_t{7}, range(1)}));
    EXPECT_EQ(c.ctx.strengthDrivers().originCount(), 1U);
}
TEST(ModulePaths, LatestSourceWinsAndTiesChooseSmallestTransitionDelay) {
    for (bool tie : {false, true}) {
        Cell c;
        const std::array<k::ModulePathSpec, 2> paths{
            {{c.source, 0, k::ModulePathEdge::Any, delay(tie ? 17 : 7)},
             {{c.b, 0, 0, 1}, 0, k::ModulePathEdge::Any, delay(tie ? 5 : 11)}}};
        ASSERT_EQ(c.ctx.scheduler().configureModulePaths(paths), E::None);
        ASSERT_EQ(c.ctx.scheduler().startStrengthDrivers(), E::None);
        Script script{c, {{tie ? 12U : 10U, Act::A, 1}, {12, Act::B, 1}, {15, Act::Drive, 1}}};
        script.start();
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        ASSERT_EQ(c.notices.size(), 1U);
        EXPECT_EQ(c.notices[0].first, tie ? 17U : 23U);
    }
}
TEST(ModulePaths, DistributedAndPathDeadlinesCombineByMaximum) {
    for (unsigned distributed : {0, 4, 9}) {
        Cell c;
        c.start(7);
        Script script{c, {{10, Act::A, 1}, {10 + distributed, Act::Drive, 1}}};
        script.start();
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        ASSERT_EQ(c.notices.size(), 1U);
        EXPECT_EQ(c.notices[0].first, 10U + std::max(distributed, 7U));
    }
}
TEST(ModulePaths, IdenticalPendingKeepsDeadlineAndCommittedReversionCancelsPulse) {
    Cell c;
    c.start();
    Script script{c,
                  {{10, Act::A, 1},
                   {10, Act::Drive, 1},
                   {12, Act::A, 0},
                   {12, Act::Drive, 0},
                   {20, Act::A, 1},
                   {20, Act::Drive, 1},
                   {22, Act::A, 2},
                   {22, Act::Drive, 1},
                   {30, Act::A, 0},
                   {30, Act::Drive, 0}}};
    script.start();
    EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.notices,
              (std::vector<std::pair<std::uint64_t, R>>{{25, range(1)}, {35, range(0)}}));
}
TEST(ModulePaths, ReplacementOwnsPayloadAndStaleTimerCannotPublish) {
    Cell c;
    c.start();
    Script script{c,
                  {{10, Act::A, 1},
                   {10, Act::Drive, 1},
                   {12, Act::A, 2},
                   {12, Act::Drive, 3},
                   {16, Act::Check, 0}}};
    script.start();
    EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.notices, (std::vector<std::pair<std::uint64_t, R>>{{17, range(3)}}));
}
TEST(ModulePaths, ZeroAndOverdueAreActiveBeforeExplicitInactiveObservation) {
    for (unsigned arrival : {0, 9}) {
        Cell c;
        c.start(arrival == 0 ? 0 : 5);
        Script script{c,
                      {{10, Act::A, 1},
                       {10 + arrival, Act::Drive, 1},
                       {10 + arrival, Act::Inactive},
                       {10 + arrival, Act::Check, 1}}};
        script.start();
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        ASSERT_EQ(c.notices.size(), 1U);
        EXPECT_EQ(c.notices[0].first, 10U + arrival);
    }
}
TEST(ModulePaths, EdgeEligibilityDescribesLatestEventAndDoesNotReuseOldMatchingEdge) {
    for (auto edge : {k::ModulePathEdge::Pos, k::ModulePathEdge::Neg}) {
        Cell c;
        c.start(7, edge);
        Script script{c,
                      {{10, Act::A, edge == k::ModulePathEdge::Pos ? 1U : 2U},
                       {11, Act::A, edge == k::ModulePathEdge::Pos ? 0U : 1U},
                       {12, Act::Drive, 1}}};
        script.start();
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        ASSERT_EQ(c.notices.size(), 1U);
        EXPECT_EQ(c.notices[0].first, 12U);
    }
}
TEST(ModulePaths, UnqualifiedStartupDelaysAndQualifiedStartupHasNoInventedEdge) {
    for (auto edge : {k::ModulePathEdge::Any, k::ModulePathEdge::Pos, k::ModulePathEdge::Neg}) {
        Cell c(3);
        c.start(7, edge);
        EXPECT_EQ(c.ctx.scheduler().driveStrength(0, range(1)), E::None);
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        ASSERT_EQ(c.notices.size(), 1U);
        EXPECT_EQ(c.notices[0].first, edge == k::ModulePathEdge::Any ? 7U : 0U);
    }
}
TEST(ModulePaths, ForcePreservesFutureAndElapsedPendingPhysicalContribution) {
    Cell c;
    c.start(7);
    Script script{c,
                  {{5, Act::Force, 0},
                   {10, Act::A, 1},
                   {10, Act::Drive, 1},
                   {12, Act::Force, 0},
                   {15, Act::Release},
                   {15, Act::Check, 0},
                   {16, Act::Force, 0},
                   {18, Act::Check, 0},
                   {19, Act::Release},
                   {19, Act::Check, 1}}};
    script.start();
    EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.notices, (std::vector<std::pair<std::uint64_t, R>>{{19, range(1)}}));
    ASSERT_TRUE(c.ctx.strengthDrivers().origin(0));
    EXPECT_EQ(state(c.ctx, 0).committed, range(1));
}
TEST(ModulePaths, IndependentContextsDoNotShareClocksOrPendingValues) {
    Cell a, b;
    a.start(3);
    b.start(11);
    Script sa{a, {{5, Act::A, 1}, {5, Act::Drive, 1}}};
    Script sb{b, {{2, Act::A, 1}, {2, Act::Drive, 1}}};
    sa.start();
    sb.start();
    EXPECT_EQ(a.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(b.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(a.notices, (std::vector<std::pair<std::uint64_t, R>>{{8, range(1)}}));
    EXPECT_EQ(b.notices, (std::vector<std::pair<std::uint64_t, R>>{{13, range(1)}}));
}
TEST(ModulePaths, RegistrationRejectsMalformedRowsAtomicallyAndOwnsInput) {
    for (unsigned mode = 0; mode < 9; ++mode) {
        Cell c;
        k::ModulePathSpec valid{c.source, 0, k::ModulePathEdge::Any, delay(7)};
        std::vector paths{valid};
        switch (mode) {
            case 0: paths.clear(); break;
            case 1: paths[0].source.slot = k::SlotId{99}; break;
            case 2: paths[0].source.elem = 1; break;
            case 3: paths[0].source.lsb = 1; break;
            case 4: paths[0].source.width = 0; break;
            case 5: paths[0].source.width = 2; break;
            case 6: paths[0].origin = 1; break;
            case 7: paths[0].edge = static_cast<k::ModulePathEdge>(99); break;
            case 8: paths.push_back(valid); break;
            default: FAIL() << "unknown invalid-row case";
        }
        EXPECT_NE(c.ctx.scheduler().configureModulePaths(paths), E::None);
        paths = {valid};
        ASSERT_EQ(c.ctx.scheduler().configureModulePaths(paths), E::None);
        paths[0].delays.fill(99);
        EXPECT_EQ(c.ctx.scheduler().configureModulePaths(paths), E::InvalidTarget);
        ASSERT_EQ(c.ctx.scheduler().startStrengthDrivers(), E::None);
        EXPECT_EQ(c.ctx.scheduler().driveStrengthDelayed(0, range(1), 1, 1, 1), E::InvalidTarget);
        EXPECT_EQ(c.ctx.scheduler().driveStrength(0, range(1)), E::None);
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(c.notices, (std::vector<std::pair<std::uint64_t, R>>{{7, range(1)}}));
    }
}
TEST(ModulePaths, WideSourceChangesUseSelectedBitAndIgnoreOtherBitsAndElements) {
    Cell c(0, 193, 2, 130);
    c.start(7);
    c.write({c.a, 0, 130, 1}, 1);
    c.write({c.a, 2, 129, 1}, 1);
    Script script{c, {{10, Act::A, 1}, {12, Act::Drive, 1}}};
    script.start();
    EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.notices, (std::vector<std::pair<std::uint64_t, R>>{{17, range(1)}}));
}
TEST(ModulePaths, EveryQualifiedSourceTransitionAgreesWithValueReference) {
    for (unsigned from = 0; from < 4; ++from)
        for (unsigned to = 0; to < 4; ++to)
            for (auto edge : {k::ModulePathEdge::Pos, k::ModulePathEdge::Neg}) {
                Cell c;
                auto* words = c.ctx.store().element(c.a, 0);
                words[0] = from == 1 || from == 3;
                words[1] = from >= 2;
                c.start(7, edge);
                Script script{c, {{10, Act::A, to}, {12, Act::Drive, 1}}};
                script.start();
                const bool match =
                    edge == k::ModulePathEdge::Pos
                        ? v::isPosedge(static_cast<v::Logic>(from), static_cast<v::Logic>(to))
                        : v::isNegedge(static_cast<v::Logic>(from), static_cast<v::Logic>(to));
                EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
                ASSERT_EQ(c.notices.size(), 1U);
                EXPECT_EQ(c.notices[0].first, match ? 17U : 12U) << from << "->" << to;
            }
}
TEST(ModulePaths, StrengthOnlySourcePublicationDoesNotResetLogicClock) {
    k::Context ctx;
    const auto a = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
    const auto q = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
    const R initial = range(0);
    ASSERT_EQ(ctx.scheduler().enableStrengths(a, {&initial, 1}), E::None);
    ASSERT_EQ(ctx.scheduler().enableStrengths(q, {&initial, 1}), E::None);
    const std::array<k::StrengthNodeSpec, 2> nodes{
        {{{a, 0, 0, 1}, k::NetKind::Wire, 0}, {{q, 0, 0, 1}, k::NetKind::Wire, 1}}};
    const std::array<k::StrengthOriginSpec, 2> roots{{{0, initial}, {1, initial}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, roots), E::None);
    const k::ModulePathSpec path{{a, 0, 0, 1}, 1, k::ModulePathEdge::Pos, delay(7)};
    ASSERT_EQ(ctx.scheduler().configureModulePaths({&path, 1}), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    // No posedge: this metadata-only update must not invent an eligible clock.
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {-3, -3}), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(ctx.scheduler().driveStrength(1, range(1)), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ctx.scheduler().time(), 0U);
    EXPECT_EQ(state(ctx, 1).committed, range(1));
    // A real logic publication must clock the path even without value subscribers.
    ASSERT_EQ(ctx.scheduler().driveStrength(0, range(1)), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(ctx.scheduler().driveStrength(1, range(0)), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ctx.scheduler().time(), 7U);
    EXPECT_EQ(state(ctx, 1).committed, range(0));
}
TEST(ModulePaths, MetadataOnlyDestinationUpdateKeepsItsCompleteContribution) {
    Cell c;
    c.start(7);
    ASSERT_EQ(c.ctx.scheduler().driveStrength(0, {-3, -3}), E::None);
    EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.read(), (R{-3, -3}));
    EXPECT_EQ(c.ctx.scheduler().time(), 0U);
}
TEST(ModulePaths, UnrepresentableSelectedDelayDoesNotWrapAndFiniteTieWins) {
    for (bool finiteTie : {false, true}) {
        Cell c;
        auto invalid = delay(7);
        invalid.fill(std::nullopt);
        std::vector<k::ModulePathSpec> paths{{c.source, 0, k::ModulePathEdge::Any, invalid}};
        if (finiteTie) paths.push_back({{c.b, 0, 0, 1}, 0, k::ModulePathEdge::Any, delay(5)});
        ASSERT_EQ(c.ctx.scheduler().configureModulePaths(paths), E::None);
        ASSERT_EQ(c.ctx.scheduler().startStrengthDrivers(), E::None);
        const auto error = c.ctx.scheduler().driveStrength(0, range(1));
        EXPECT_EQ(error, finiteTie ? E::None : E::InvalidValue);
        EXPECT_EQ(c.ctx.scheduler().takeOverflow(), !finiteTie);
        EXPECT_EQ(c.ctx.scheduler().run(), k::EndReason::Quiescent);
        if (finiteTie) {
            EXPECT_EQ(c.read(), range(1));
            EXPECT_EQ(c.ctx.scheduler().time(), 5U);
        } else {
            EXPECT_EQ(c.read(), range(0));
            EXPECT_TRUE(c.notices.empty());
        }
    }
}
class PathFailingMemory final : public std::pmr::memory_resource {
public:
    std::size_t remaining = SIZE_MAX;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc();
        if (remaining != SIZE_MAX) --remaining;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(ModulePaths, AllocationFailureLeavesRegistrationRetryableAndNoPublishedState) {
    unsigned failures = 0, successes = 0;
    for (unsigned budget = 0; budget < 10; ++budget) {
        PathFailingMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        const auto a = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
        const auto q = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
        const R initial = range(0);
        ASSERT_EQ(ctx.scheduler().enableStrengths(q, {&initial, 1}), E::None);
        const k::StrengthNodeSpec node{{q, 0, 0, 1}};
        const k::StrengthOriginSpec origin{0, initial};
        ASSERT_EQ(ctx.scheduler().configureStrengthDrivers({&node, 1}, {&origin, 1}), E::None);
        const k::ModulePathSpec path{{a, 0, 0, 1}, 0, k::ModulePathEdge::Any, delay(5)};
        memory.remaining = budget;
        const auto error = ctx.scheduler().configureModulePaths({&path, 1});
        memory.remaining = SIZE_MAX;
        if (error == E::ResourceLimit) {
            ++failures;
            EXPECT_EQ(state(ctx, 0).committed, initial);
            ASSERT_EQ(ctx.scheduler().configureModulePaths({&path, 1}), E::None);
        } else {
            ++successes;
            EXPECT_EQ(error, E::None);
        }
        ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
        ASSERT_EQ(ctx.scheduler().driveStrength(0, range(1)), E::None);
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(ctx.scheduler().time(), 5U);
        EXPECT_EQ(state(ctx, 0).committed, range(1));
    }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}
}  // namespace
