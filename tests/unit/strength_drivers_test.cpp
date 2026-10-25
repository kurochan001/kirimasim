// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original physical-origin/inertial matrices, MOS/switch contract 1/6/7,
// IEEE 1800 4.9.5, 10.3.3, 28.7 and 28.16. No source-language support claim.
#include <array>
#include <csignal>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/mos.h"
#include "kirimasim/valsem/strength.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using R = k::StrengthRange;
using E = k::StrengthError;

k::SlotId slot(k::Context& ctx, unsigned width = 1, unsigned elements = 1,
               k::Init init = k::Init::Z) {
    const auto id = ctx.store().add(k::Domain::Four, width, elements, init);
    const R range = init == k::Init::Z ? R{} : R{-6, 6};
    EXPECT_EQ(
        ctx.scheduler().enableStrengths(id, std::vector<R>(std::size_t{width} * elements, range)),
        E::None);
    return id;
}
R read(const k::Context& ctx, k::SlotBits at) {
    R value;
    EXPECT_EQ(ctx.strengths().read(at, {&value, 1}), E::None);
    return value;
}
k::StrengthOriginState state(const k::Context& ctx, std::uint32_t origin) {
    const auto value = ctx.strengthDrivers().origin(origin);
    EXPECT_TRUE(value.has_value());
    return value.value_or(k::StrengthOriginState{});
}

std::uint64_t count(const k::Context& ctx, const char* name) {
    for (std::uint32_t i = 0; i < ctx.counters().size(); ++i)
        if (ctx.counters().name(k::CounterId{i}) == name)
            return ctx.counters().value(k::CounterId{i});
    return 0;
}
struct Notice {
    k::Context* ctx = nullptr;
    std::array<std::uint64_t, 64> times{};
    std::array<R, 64> values{};
    unsigned used = 0;
    static void record(void* raw, k::SlotBits, std::span<const R>,
                       std::span<const R> after) noexcept {
        auto& self = *static_cast<Notice*>(raw);
        if (self.used < self.times.size()) {
            self.times[self.used] = self.ctx->scheduler().time();
            self.values[self.used] = after[0];
        }
        ++self.used;
    }
};
void observe(k::Context& ctx, k::SlotId id, Notice& notice) {
    notice.ctx = &ctx;
    ASSERT_EQ(ctx.strengths().subscribe({id, 0, 0, 1}, &Notice::record, &notice).error, E::None);
}
void configureOne(k::Context& ctx, k::SlotId id, R initial) {
    const k::StrengthNodeSpec node{{id, 0, 0, 1}};
    const k::StrengthOriginSpec origin{0, initial};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers({&node, 1}, {&origin, 1}), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
}
struct Action {
    std::uint64_t at = 0;
    R value;
    std::array<std::uint64_t, 3> delay{};
    bool immediate = false;
    std::uint32_t origin = 0;
};
struct Script {
    k::ProcessId process{0};
    std::vector<Action> actions;
    std::size_t next = 0;
    unsigned errors = 0;
    static kirima_status run(kirima_context* raw, void* frame) {
        auto& self = *static_cast<Script*>(frame);
        auto& scheduler = k::fromC(raw).scheduler();
        if (self.next < self.actions.size() && scheduler.time() != self.actions[self.next].at) {
            scheduler.waitDelay(self.process, self.actions[self.next].at - scheduler.time());
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        while (self.next < self.actions.size() && self.actions[self.next].at == scheduler.time()) {
            const auto& a = self.actions[self.next++];
            const auto status = a.immediate
                                    ? scheduler.driveStrength(a.origin, a.value)
                                    : scheduler.driveStrengthDelayed(a.origin, a.value, a.delay[0],
                                                                     a.delay[1], a.delay[2]);
            if (status != E::None) ++self.errors;
        }
        if (self.next == self.actions.size()) return static_cast<kirima_status>(k::Status::Done);
        scheduler.waitDelay(self.process, self.actions[self.next].at - scheduler.time());
        return static_cast<kirima_status>(k::Status::Waiting);
    }
    void start(k::Context& ctx, k::Group group = k::Group::Active) {
        process = ctx.scheduler().addProcess(&run, this, group);
        ctx.scheduler().start(process);
    }
};

TEST(StrengthDrivers, RegistrationOwnsTablesAndPreservesOrdinaryXAndMosZUntilInitialActiveSolve) {
    k::Context ctx;
    const auto id = slot(ctx, 3, 1, k::Init::X);
    std::vector<k::StrengthNodeSpec> nodes{{{id, 0, 0, 1}}, {{id, 0, 1, 1}}, {{id, 0, 2, 1}}};
    std::vector<k::StrengthOriginSpec> origins{{0, {-6, 6}}, {1, {}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    nodes.clear();
    origins.clear();
    EXPECT_EQ(read(ctx, {id, 0, 1, 1}), (R{-6, 6}));
    EXPECT_EQ(count(ctx, "slot.writes"), 0U);
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
    EXPECT_EQ(ctx.scheduler().driveStrength(0, {1, 1}), E::InvalidTarget);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    EXPECT_EQ(read(ctx, {id, 0, 1, 1}), (R{-6, 6}));
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(read(ctx, {id, 0, 0, 1}), (R{-6, 6}));
    EXPECT_EQ(read(ctx, {id, 0, 1, 1}), R{});
    EXPECT_EQ(read(ctx, {id, 0, 2, 1}), R{});
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
    EXPECT_EQ(ctx.scheduler().startStrengthDrivers(), E::InvalidTarget);
}

TEST(StrengthDrivers, EveryIntervalAgainstEveryPointAndNetTieKindKeepsCompletePackedMetadata) {
    constexpr std::array kinds{k::NetKind::Wire,   k::NetKind::Tri, k::NetKind::Wand,
                               k::NetKind::Triand, k::NetKind::Wor, k::NetKind::Trior,
                               k::NetKind::Uwire};
    k::Context ctx;
    const auto id = slot(ctx, 129, 3);
    std::vector<k::StrengthNodeSpec> nodes;
    std::vector<k::StrengthOriginSpec> origins;
    std::vector<std::uint32_t> variable;
    for (unsigned kind = 0; kind < kinds.size(); ++kind)
        for (int point = -7; point <= 7; ++point) {
            const auto n = static_cast<std::uint32_t>(nodes.size());
            nodes.push_back({{id, n % 3, n, 1}, kinds[kind], kind});
            variable.push_back(static_cast<std::uint32_t>(origins.size()));
            origins.push_back({n, {}});
            if (kinds[kind] != k::NetKind::Uwire)
                origins.push_back(
                    {n, {static_cast<std::int8_t>(point), static_cast<std::int8_t>(point)}});
        }
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    for (int lo = -7; lo <= 7; ++lo)
        for (int hi = lo; hi <= 7; ++hi) {
            const R range{static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)};
            for (const auto origin : variable)
                ASSERT_EQ(ctx.scheduler().driveStrength(origin, range), E::None);
            ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            for (unsigned n = 0; n < nodes.size(); ++n) {
                const auto kind = n / 15;
                std::vector<v::Signal> inputs{{lo, hi}};
                if (kinds[kind] != k::NetKind::Uwire)
                    inputs.push_back({int(n % 15) - 7, int(n % 15) - 7});
                const auto expected =
                    v::resolveSignals(static_cast<v::NetKind>(kinds[kind]), inputs);
                EXPECT_EQ(read(ctx, nodes[n].at), (R{static_cast<std::int8_t>(expected.lo),
                                                     static_cast<std::int8_t>(expected.hi)}));
            }
            for (unsigned e = 0; e < 3; ++e) {
                std::array<k::W4, 17> packed{};
                ASSERT_EQ(ctx.strengths().readPacked({id, e, 0, 129}, packed), E::None);
                for (unsigned n = e; n < nodes.size(); n += 3)
                    EXPECT_EQ(k::strengthRangeAt(packed, n),
                              std::optional<R>(read(ctx, nodes[n].at)));
                EXPECT_EQ(packed.back().a >> 8, 0U);
                EXPECT_EQ(packed.back().b >> 8, 0U);
            }
        }
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
}

TEST(StrengthDrivers, ExplicitPullAndSupplyArePhysicalOriginsRatherThanDuplicatedNetDrivers) {
    k::Context ctx;
    const auto id = slot(ctx, 4);
    const std::array<k::StrengthNodeSpec, 4> nodes{
        {{{id, 0, 0, 1}}, {{id, 0, 1, 1}}, {{id, 0, 2, 1}}, {{id, 0, 3, 1}}}};
    const std::array<k::StrengthOriginSpec, 5> origins{
        {{0, {-5, -5}}, {1, {5, 5}}, {2, {-7, -7}}, {3, {7, 7}}, {3, {-6, -6}}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    for (unsigned bit = 0; bit < 4; ++bit)
        EXPECT_EQ(read(ctx, {id, 0, bit, 1}), origins[bit].initial);
    ASSERT_EQ(ctx.scheduler().driveStrength(3, {}), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(read(ctx, {id, 0, 3, 1}), (R{-6, -6}));
}

TEST(StrengthDrivers, InvalidStaticRowsAreAtomicAndRetryable) {
    for (unsigned mode = 0; mode < 14; ++mode) {
        k::Context ctx;
        const auto id = slot(ctx, 2);
        std::vector<k::StrengthNodeSpec> nodes{{{id, 0, 0, 1}}, {{id, 0, 1, 1}}};
        std::vector<k::StrengthOriginSpec> origins{{0, {1, 1}}, {1, {-1, -1}}};
        switch (mode) {
            case 0: nodes.clear(); break;
            case 1: nodes[0].at.slot = k::SlotId{999}; break;
            case 2: nodes[0].at.elem = 1; break;
            case 3: nodes[0].at.width = 0; break;
            case 4: nodes[0].at.width = 2; break;
            case 5: nodes[0].at.lsb = 2; break;
            case 6: nodes[1].at = nodes[0].at; break;
            case 7: nodes[0].kind = k::NetKind::Tri0; break;
            case 8: nodes[0].kind = static_cast<k::NetKind>(255); break;
            case 9: nodes[0].component = 2; break;
            case 10: nodes[0].component = nodes[1].component = 1; break;
            case 11: origins[0].node = 2; break;
            case 12: origins[0].initial = {7, -7}; break;
            case 13:
                nodes[0].kind = k::NetKind::Uwire;
                origins[1].node = 0;
                break;
            default: ADD_FAILURE() << "unknown static rejection case"; break;
        }
        EXPECT_NE(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None) << mode;
        EXPECT_FALSE(ctx.strengthDrivers().configured());
        EXPECT_EQ(ctx.strengthDrivers().nodeCount(), 0U);
        EXPECT_EQ(count(ctx, "slot.writes"), 0U);
        configureOne(ctx, id, {6, 6});
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(read(ctx, {id, 0, 0, 1}), (R{6, 6}));
        const k::StrengthNodeSpec one{{id, 0, 0, 1}};
        EXPECT_EQ(ctx.scheduler().configureStrengthDrivers({&one, 1}, {}), E::InvalidTarget);
    }
}

class FailingMemory final : public std::pmr::memory_resource {
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
TEST(StrengthDrivers, AllocationFailuresDoNotPublishPartialConfigurationOrResolution) {
    unsigned failures = 0, successes = 0;
    for (unsigned budget = 0; budget < 24; ++budget) {
        FailingMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        const auto id = slot(ctx, 2);
        const std::array<k::StrengthNodeSpec, 2> nodes{{{{id, 0, 0, 1}}, {{id, 0, 1, 1}}}};
        const std::array<k::StrengthOriginSpec, 2> origins{{{0, {6, 6}}, {1, {-6, -6}}}};
        memory.remaining = budget;
        const auto error = ctx.scheduler().configureStrengthDrivers(nodes, origins);
        memory.remaining = SIZE_MAX;
        if (error == E::ResourceLimit) {
            ++failures;
            EXPECT_FALSE(ctx.strengthDrivers().configured());
            EXPECT_EQ(ctx.strengthDrivers().originCount(), 0U);
            EXPECT_EQ(ctx.strengthDrivers().nodeCount(), 0U);
            ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
        } else {
            ++successes;
            ASSERT_EQ(error, E::None);
        }
        ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
        memory.remaining = 0;
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::ResourceLimit);
        EXPECT_EQ(read(ctx, {id, 0, 0, 1}), R{});
        EXPECT_EQ(read(ctx, {id, 0, 1, 1}), R{});
        EXPECT_EQ(count(ctx, "slot.writes"), 0U);
        memory.remaining = SIZE_MAX;
    }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}

TEST(StrengthDrivers, RepeatedRootsCoalesceIntoOneCompleteComponentSnapshot) {
    k::Context ctx;
    const auto id = slot(ctx);
    const k::StrengthNodeSpec node{{id, 0, 0, 1}};
    const std::array<k::StrengthOriginSpec, 2> origins{{{0, {}}, {0, {}}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers({&node, 1}, origins), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    Notice notices;
    observe(ctx, id, notices);
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {6, 6}), E::None);
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {3, 6}), E::None);
    ASSERT_EQ(ctx.scheduler().driveStrength(1, {-4, -4}), E::None);
    EXPECT_EQ(read(ctx, {id, 0, 0, 1}), R{});
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(notices.used, 1U);
    EXPECT_EQ(notices.values[0], (R{-4, 6}));
    EXPECT_EQ(count(ctx, "slot.writes"), 1U);
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
    EXPECT_EQ(ctx.scheduler().driveStrength(99, {}), E::InvalidTarget);
    EXPECT_EQ(ctx.scheduler().driveStrength(0, {-8, 6}), E::InvalidValue);
}

struct AtomicComponent {
    k::Context* ctx;
    k::SlotId id;
    unsigned calls = 0, errors = 0;
    static void notice(void* raw, k::SlotBits, std::span<const R>,
                       std::span<const R> after) noexcept {
        auto& self = *static_cast<AtomicComponent*>(raw);
        ++self.calls;
        std::array<R, 2> actual;
        if (self.ctx->strengths().read({self.id, 0, 0, 2}, actual) != E::None) ++self.errors;
        const bool first = self.calls <= 2;
        if (actual != (first ? std::array<R, 2>{R{6, 6}, R{-6, -6}} : std::array<R, 2>{R{}, R{}}))
            ++self.errors;
        if ((first && after[0] == R{}) || (!first && after[0] != R{})) ++self.errors;
        if (self.calls == 1) {
            if (self.ctx->scheduler().driveStrength(0, {}) != E::None) ++self.errors;
            if (self.ctx->scheduler().driveStrength(1, {}) != E::None) ++self.errors;
        }
    }
};
TEST(StrengthDrivers,
     CompleteComponentCommitsBeforeCallbacksAndReentrantDrivesQueueDistinctSnapshots) {
    k::Context ctx;
    const auto id = slot(ctx, 2);
    const std::array<k::StrengthNodeSpec, 2> nodes{{{{id, 0, 0, 1}}, {{id, 0, 1, 1}}}};
    const std::array<k::StrengthOriginSpec, 2> origins{{{0, {6, 6}}, {1, {-6, -6}}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    AtomicComponent snapshot{&ctx, id};
    ASSERT_EQ(ctx.strengths().subscribe({id, 0, 0, 2}, &AtomicComponent::notice, &snapshot).error,
              E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(snapshot.calls, 4U);
    EXPECT_EQ(snapshot.errors, 0U);
    EXPECT_EQ(count(ctx, "slot.writes"), 4U);
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
}

struct Device {
    k::Context* ctx;
    k::MosKind kind;
    std::uint32_t output;
    k::W4 first{1, 0}, second{0, 0};
    unsigned evaluations = 0, failures = 0;
    static void update(void* raw, k::SlotBits, std::span<const R>,
                       std::span<const R> after) noexcept {
        auto& self = *static_cast<Device*>(raw);
        ++self.evaluations;
        const auto result = k::mosStrength(self.kind, after[0], self.first, self.second);
        if (!result || self.ctx->scheduler().driveStrength(self.output, *result) != E::None)
            ++self.failures;
    }
};
TEST(StrengthDrivers,
     CompleteRangeNoticesPropagateThroughSixDirectedMosDevicesWithoutOrdinaryEdges) {
    k::Context ctx;
    const auto input = slot(ctx);
    const auto output = slot(ctx, 6);
    std::vector<k::StrengthNodeSpec> nodes{{{input, 0, 0, 1}}};
    std::vector<k::StrengthOriginSpec> origins{{0, {3, 3}}};
    for (unsigned bit = 0; bit < 6; ++bit) {
        nodes.push_back({{output, 0, bit, 1}, k::NetKind::Wire, bit + 1});
        origins.push_back({bit + 1, {}});
    }
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    std::vector<Device> devices;
    devices.reserve(6);
    for (unsigned kind = 0; kind < 6; ++kind) {
        auto& device = devices.emplace_back(Device{&ctx, static_cast<k::MosKind>(kind), kind + 1});
        if (kind == 1 || kind == 3) device.first = {0, 0};
        ASSERT_EQ(ctx.strengths().subscribe({input, 0, 0, 1}, &Device::update, &device).error,
                  E::None);
    }
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    for (const auto range : {R{3, 3}, R{3, 6}, R{6, 6}}) {
        ASSERT_EQ(ctx.scheduler().driveStrength(0, range), E::None);
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        for (unsigned kind = 0; kind < 6; ++kind) {
            const auto& device = devices[kind];
            const auto expected =
                v::mosStrength(static_cast<v::MosKind>(kind), {range.lo, range.hi},
                               kind == 1 || kind == 3 ? v::Logic::Zero : v::Logic::One);
            if (!expected.has_value()) {
                ADD_FAILURE() << "reference rejected a valid device";
                continue;
            }
            EXPECT_EQ(read(ctx, {output, 0, kind, 1}), (R{static_cast<std::int8_t>(expected->lo),
                                                          static_cast<std::int8_t>(expected->hi)}));
            EXPECT_EQ(device.failures, 0U);
        }
    }
    for (const auto& device : devices)
        EXPECT_EQ(device.evaluations, 3U);
    // Only initial Z->1 changes the input's projected value. Metadata still
    // causes three dependent evaluations; no synthetic processes are created.
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
}

TEST(StrengthDrivers, SamePendingIntervalKeepsDeadlineAndOwnsCapturedValue) {
    k::Context ctx;
    const auto id = slot(ctx);
    configureOne(ctx, id, {});
    Notice notices;
    observe(ctx, id, notices);
    Script script{k::ProcessId{0}, {{0, {3, 6}, {5, 7, 9}}, {2, {3, 6}, {99, 99, 99}}}};
    script.start(ctx);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(script.errors, 0U);
    ASSERT_EQ(notices.used, 1U);
    EXPECT_EQ(notices.times[0], 5U);
    EXPECT_EQ(notices.values[0], (R{3, 6}));
    EXPECT_FALSE(state(ctx, 0).hasPending);
}

TEST(StrengthDrivers, DifferentSameValueIntervalCancelsAndReschedulesFromCurrentTime) {
    k::Context ctx;
    const auto id = slot(ctx);
    configureOne(ctx, id, {3, 3});
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    Notice notices;
    observe(ctx, id, notices);
    Script script{k::ProcessId{0}, {{0, {3, 6}, {5, 5, 5}}, {2, {6, 6}, {7, 7, 7}}}};
    script.start(ctx);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(notices.used, 1U);
    EXPECT_EQ(notices.times[0], 9U);
    EXPECT_EQ(notices.values[0], (R{6, 6}));
    EXPECT_EQ(script.errors, 0U);
}

TEST(StrengthDrivers,
     RevertingToCommittedRangeCancelsWithoutSchedulingAndImmediateDriveCancelsTimers) {
    for (const bool immediate : {false, true}) {
        k::Context ctx;
        const auto id = slot(ctx);
        configureOne(ctx, id, {3, 3});
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        Notice notices;
        observe(ctx, id, notices);
        Script script{
            k::ProcessId{0},
            {{0, {3, 6}, {5, 5, 5}}, {2, immediate ? R{7, 7} : R{3, 3}, {99, 99, 99}, immediate}}};
        script.start(ctx);
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(notices.used, immediate ? 1U : 0U);
        if (immediate) {
            EXPECT_EQ(notices.times[0], 2U);
            EXPECT_EQ(notices.values[0], (R{7, 7}));
        }
        EXPECT_EQ(read(ctx, {id, 0, 0, 1}), immediate ? (R{7, 7}) : (R{3, 3}));
        EXPECT_FALSE(state(ctx, 0).hasPending);
        EXPECT_EQ(script.errors, 0U);
    }
}

TEST(StrengthDrivers, DestinationValueSelectsScalarDelayForEveryValueAndStrengthOnlyChange) {
    constexpr std::array ranges{R{-6, -6}, R{6, 6}, R{}, R{-6, 6}, R{3, 6}, R{-6, -3}};
    constexpr std::array<std::uint64_t, 6> due{7, 11, 13, 7, 11, 7};
    for (const auto initial : ranges)
        for (unsigned n = 0; n < ranges.size(); ++n) {
            if (initial == ranges[n]) continue;
            k::Context ctx;
            const auto id = slot(ctx);
            configureOne(ctx, id, initial);
            ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            Notice notices;
            observe(ctx, id, notices);
            ASSERT_EQ(ctx.scheduler().driveStrengthDelayed(0, ranges[n], 11, 7, 13), E::None);
            EXPECT_EQ(read(ctx, {id, 0, 0, 1}), initial);
            ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            ASSERT_EQ(notices.used, 1U);
            EXPECT_EQ(notices.times[0], due[n]);
            EXPECT_EQ(notices.values[0], ranges[n]);
        }
}

TEST(StrengthDrivers, ScalarOriginsHaveIndependentInertialTimers) {
    k::Context ctx;
    const auto id = slot(ctx, 2);
    const std::array<k::StrengthNodeSpec, 2> nodes{{{{id, 0, 0, 1}}, {{id, 0, 1, 1}}}};
    const std::array<k::StrengthOriginSpec, 2> origins{{{0, {}}, {1, {}}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    Script script{k::ProcessId{0},
                  {{0, {6, 6}, {5, 5, 5}, false, 0},
                   {0, {6, 6}, {3, 3, 3}, false, 1},
                   {2, {3, 6}, {7, 7, 7}, false, 0}}};
    script.start(ctx);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(read(ctx, {id, 0, 0, 1}), (R{3, 6}));
    EXPECT_EQ(read(ctx, {id, 0, 1, 1}), (R{6, 6}));
    EXPECT_EQ(ctx.scheduler().time(), 9U);
    EXPECT_EQ(state(ctx, 1).generation, 1U);
    EXPECT_EQ(script.errors, 0U);
}

struct Order {
    k::Context* ctx;
    k::SlotId id;
    std::array<unsigned, 8> observed{};
    unsigned next = 0;
    static void notice(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<Order*>(raw);
        self.observed[self.next++] = 3;
    }
    static kirima_status drive(kirima_context*, void* raw) {
        auto& self = *static_cast<Order*>(raw);
        self.observed[self.next++] = 1;
        EXPECT_EQ(self.ctx->scheduler().driveStrengthDelayed(0, {6, 6}, 0, 0, 0), E::None);
        EXPECT_EQ(read(*self.ctx, {self.id, 0, 0, 1}), R{});
        return static_cast<kirima_status>(k::Status::Done);
    }
    static kirima_status direct(kirima_context*, void* raw) {
        auto& self = *static_cast<Order*>(raw);
        self.observed[self.next++] = 1;
        EXPECT_EQ(self.ctx->scheduler().driveStrength(0, {6, 6}), E::None);
        return static_cast<kirima_status>(k::Status::Done);
    }
    static kirima_status probe(kirima_context*, void* raw) {
        auto& self = *static_cast<Order*>(raw);
        self.observed[self.next++] = 2;
        EXPECT_EQ(read(*self.ctx, {self.id, 0, 0, 1}), R{});
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(StrengthDrivers, ExplicitZeroWaitsForInactiveAndReactiveWritesQueueLaterActiveWork) {
    for (const bool reactive : {false, true}) {
        k::Context ctx;
        const auto id = slot(ctx);
        configureOne(ctx, id, {});
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        Order order{&ctx, id};
        ASSERT_EQ(ctx.strengths().subscribe({id, 0, 0, 1}, &Order::notice, &order).error, E::None);
        const auto group = reactive ? k::Group::Reactive : k::Group::Active;
        const auto a =
            ctx.scheduler().addProcess(reactive ? &Order::direct : &Order::drive, &order, group);
        const auto b = ctx.scheduler().addProcess(&Order::probe, &order, group);
        ctx.scheduler().start(a);
        ctx.scheduler().start(b);
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(order.next, 3U);
        EXPECT_EQ(order.observed[0], 1U);
        EXPECT_EQ(order.observed[1], 2U);
        EXPECT_EQ(order.observed[2], 3U);
        EXPECT_EQ(count(ctx, "sched.events.active"), reactive ? 2U : 4U);
        EXPECT_EQ(count(ctx, "sched.events.reactive"), reactive ? 2U : 0U);
        EXPECT_EQ(count(ctx, "sched.events.inactive"), reactive ? 0U : 1U);
        EXPECT_EQ(count(ctx, "proc.created"), 2U);
        EXPECT_EQ(read(ctx, {id, 0, 0, 1}), (R{6, 6}));
    }
}

struct Overflow {
    k::ProcessId process{0};
    unsigned step = 0;
    static kirima_status run(kirima_context* raw, void* frame) {
        auto& self = *static_cast<Overflow*>(frame);
        auto& ctx = k::fromC(raw);
        auto& scheduler = ctx.scheduler();
        if (self.step++ == 0) {
            scheduler.waitDelay(self.process, UINT64_MAX - 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        EXPECT_EQ(scheduler.driveStrengthDelayed(0, {3, 6}, 1, 1, 1), E::None);
        const auto generation = state(ctx, 0).generation;
        EXPECT_EQ(scheduler.driveStrengthDelayed(0, {6, 6}, 3, 3, 3), E::InvalidValue);
        EXPECT_TRUE(scheduler.takeOverflow());
        EXPECT_EQ(state(ctx, 0).generation, generation);
        EXPECT_EQ(state(ctx, 0).pending, (R{3, 6}));
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(StrengthDrivers, OverflowDoesNotReplaceThePreviouslyOwnedPendingRange) {
    k::Context ctx;
    const auto id = slot(ctx);
    configureOne(ctx, id, {});
    Overflow overflow;
    overflow.process = ctx.scheduler().addProcess(&Overflow::run, &overflow, k::Group::Active);
    ctx.scheduler().start(overflow.process);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ctx.scheduler().time(), UINT64_MAX);
    EXPECT_EQ(read(ctx, {id, 0, 0, 1}), (R{3, 6}));
}

TEST(StrengthDrivers, ContextsAndStopRequestsDoNotLeakOriginsOrPublishIncompleteResults) {
    k::Context first, second;
    const auto a = slot(first), b = slot(second);
    configureOne(first, a, {6, 6});
    configureOne(second, b, {-6, -6});
    volatile std::sig_atomic_t interrupted = 1;
    first.scheduler().setInterruptFlag(&interrupted);
    EXPECT_EQ(first.scheduler().run(), k::EndReason::Interrupted);
    EXPECT_EQ(read(first, {a, 0, 0, 1}), R{});
    EXPECT_EQ(second.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(read(second, {b, 0, 0, 1}), (R{-6, -6}));
    EXPECT_EQ(count(first, "proc.created"), 0U);
    EXPECT_EQ(count(second, "proc.created"), 0U);
}
}  // namespace
