// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <array>
#include <memory_resource>
#include <vector>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;

struct Scope {
    k::ActivationLease owner;
    k::ActivationBinding index, array, other;
    explicit Scope(k::Context& context, k::ActivationKind kind = k::ActivationKind::Four) {
        auto& store = context.activations();
        const auto layout = store.addLayout(std::vector<k::ActivationField>{
            {k::ActivationKind::Two, 32, {}}, {kind, 257, {{-1, 1}}}, {kind, 257, {}}});
        if (!layout) return;
        auto created = store.create(layout.value);
        if (!created) return;
        owner = std::move(created.value);
        index = store.bind(owner, 0).value;
        array = store.bind(owner, 1).value;
        other = store.bind(owner, 2).value;
    }
};

struct Watch {
    k::ActivationBinding index, array;
    std::uint32_t lsb = 63, width = 65;
    unsigned evals = 0, guards = 0, resumes = 0;
    bool valid = true, allow = false;
    k::ProcessId process{0};
    static void eval(kirima_context* handle, void* frame, std::uint64_t* out) {
        auto& watch = *static_cast<Watch*>(frame);
        auto& store = k::fromC(handle).activations();
        ++watch.evals;
        k::W4 selected{};
        watch.valid =
            watch.valid && store.read(watch.index, 0, {&selected, 1}) == k::ActivationError::None;
        std::array<k::W4, 2> value{};
        const auto count = (watch.width + 63) / 64;
        watch.valid = watch.valid && store.read(watch.array, static_cast<std::uint32_t>(selected.a),
                                                {value.data(), count}, watch.lsb,
                                                watch.width) == k::ActivationError::None;
        for (std::uint32_t i = 0; i < count; ++i) {
            out[2 * i] = value[i].a;
            out[2 * i + 1] = value[i].b;
        }
    }
    static int guard(kirima_context*, void* frame) {
        auto& watch = *static_cast<Watch*>(frame);
        ++watch.guards;
        return watch.allow ? 1 : 0;
    }
    static kirima_status resume(kirima_context*, void* frame) {
        auto& watch = *static_cast<Watch*>(frame);
        ++watch.resumes;
        return static_cast<kirima_status>(k::Status::Done);
    }
    k::MonitorSpec spec() {
        k::MonitorSpec result;
        result.eval = &eval;
        result.frame = this;
        result.width = width;
        result.guard = &guard;
        result.guardFrame = this;
        result.activation.capture = true;
        result.activation.bindings = {index, array};
        return result;
    }
};

void write(k::Context& context, const k::ActivationBinding& binding, std::uint32_t element,
           std::uint32_t lsb, k::W4 value) {
    ASSERT_EQ(context.scheduler().writeActivation(binding, element, {&value, 1}, lsb, 1),
              k::ActivationError::None);
}

TEST(ActivationScheduler, DynamicReadsReattachAndKeepOnePhysicalExtentAfterOwnerReturn) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context context;
        auto& scheduler = context.scheduler();
        Scope scope(context);
        ASSERT_TRUE(scope.owner);
        Watch watch{scope.index, scope.array};
        watch.process = scheduler.addProcess(&Watch::resume, &watch, group);
        const auto spec = watch.spec();
        scheduler.waitAny(watch.process, {}, {&spec, 1});
        ASSERT_EQ(watch.evals, 1U);
        EXPECT_EQ(context.activations().stats().leases, 2U);
        scope.owner.reset();
        EXPECT_EQ(context.activations().stats().live, 1U);
        write(context, scope.array, 1, 63, {1, 0});
        write(context, scope.array, 0, 62, {1, 0});
        write(context, scope.other, 0, 63, {1, 0});
        EXPECT_EQ(watch.evals, 1U);
        write(context, scope.array, 0, 127, {1, 1});
        EXPECT_EQ(watch.evals, 2U);
        EXPECT_EQ(watch.guards, 1U);
        const k::W4 index{1, 0};
        ASSERT_EQ(scheduler.writeActivation(scope.index, 0, {&index, 1}), k::ActivationError::None);
        EXPECT_EQ(watch.evals, 3U);
        write(context, scope.array, 0, 64, {1, 0});
        EXPECT_EQ(watch.evals, 3U);
        // An unchanged normalized write does not announce a new event.
        write(context, scope.array, 1, 63, {1, 0});
        EXPECT_EQ(watch.evals, 3U);
        watch.allow = true;
        write(context, scope.array, 1, 127, {0, 1});
        EXPECT_EQ(watch.evals, 4U);
        EXPECT_TRUE(watch.valid);
        EXPECT_EQ(context.activations().stats().live, 0U);
        EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(watch.resumes, 1U);
    }
}

TEST(ActivationScheduler, StaticSensitivityIncludesUnexecutedBranchesAndValidatesBounds) {
    k::Context context;
    Scope scope(context);
    Watch watch{scope.index, scope.array};
    watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
    auto spec = watch.spec();
    spec.activation.capture = false;
    spec.activation.bindings.clear();
    spec.activation.parts = {{scope.array, 0, 3, 63, 65}};
    context.scheduler().waitAny(watch.process, {}, {&spec, 1});
    write(context, scope.array, 2, 64, {1, 0});
    EXPECT_EQ(watch.evals, 2U);
    EXPECT_EQ(watch.guards, 0U);
    write(context, scope.array, 2, 128, {1, 0});
    EXPECT_EQ(watch.evals, 2U);
    context.scheduler().kill(watch.process);
    EXPECT_EQ(context.activations().stats().leases, 1U);
    const auto region = context.activations().region({scope.array, 0, 3});
    ASSERT_TRUE(region);
    EXPECT_EQ(region.value.width, 257U);
    for (const k::ActivationPart invalid : {k::ActivationPart{scope.array, 0, 0},
                                            {scope.array, 3, 1},
                                            {scope.array, 1, UINT32_MAX},
                                            {scope.array, 0, 1, 257, 1},
                                            {scope.array, 0, 1, 1, 257}})
        EXPECT_EQ(context.activations().region(invalid).error, k::ActivationError::OutOfRange);
}

TEST(ActivationScheduler, UntilTrueUsesFourStateTruthAndNormalizationAndRejectsReadonlyWrites) {
    for (const auto kind : {k::ActivationKind::Two, k::ActivationKind::Four}) {
        k::Context context;
        Scope scope(context, kind);
        Watch watch{scope.index, scope.array};
        watch.lsb = 63;
        watch.width = 1;
        watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
        auto spec = watch.spec();
        spec.untilTrue = true;
        context.scheduler().waitAny(watch.process, {}, {&spec, 1});
        const auto readonly = context.activations().readOnly(scope.array);
        ASSERT_TRUE(readonly);
        const k::W4 one{1, 0};
        EXPECT_EQ(context.scheduler().writeActivation(readonly.value, 0, {&one, 1}, 63, 1),
                  k::ActivationError::ReadOnly);
        write(context, scope.array, 0, 63, {1, 1});
        EXPECT_EQ(watch.evals, kind == k::ActivationKind::Four ? 2U : 1U);
        EXPECT_EQ(watch.guards, 0U);
        write(context, scope.array, 0, 63, {1, 0});
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(watch.resumes, 1U);
        EXPECT_TRUE(watch.valid);
    }
}

struct Notices {
    unsigned count = 0;
    static void record(void* frame, const k::ActivationAccess&) noexcept {
        ++static_cast<Notices*>(frame)->count;
    }
};

TEST(ActivationScheduler, CanceledDependenciesDoNotWakeReusedRecordsOrDifferentContexts) {
    k::Context context, foreign;
    Scope first(context), second(context), other(foreign);
    Notices changes, reads;
    context.activations().setChangeHook({&Notices::record, &changes});
    context.activations().setReadHook({&Notices::record, &reads});
    Watch old{first.index, first.array}, next{second.index, second.array};
    old.process = context.scheduler().addProcess(&Watch::resume, &old, k::Group::Active);
    next.process = context.scheduler().addProcess(&Watch::resume, &next, k::Group::Reactive);
    auto spec = old.spec();
    context.scheduler().waitAny(old.process, {}, {&spec, 1});
    // Keep a queued notification while the monitor record is recycled.
    const k::W4 one{1, 0};
    ASSERT_EQ(context.activations().write(first.array, 0, {&one, 1}, 63, 1),
              k::ActivationError::None);
    context.scheduler().kill(old.process);
    spec = next.spec();
    context.scheduler().waitAny(next.process, {}, {&spec, 1});
    context.scheduler().flushHeapChanges();
    EXPECT_EQ(next.evals, 1U);
    write(foreign, other.array, 0, 63, {1, 0});
    EXPECT_EQ(next.evals, 1U);
    EXPECT_EQ(context.activations().stats().leases, 3U);
    context.scheduler().kill(next.process);
    EXPECT_EQ(context.activations().stats().leases, 2U);
    const auto previousChange = context.activations().setChangeHook({});
    EXPECT_EQ(previousChange.fn, &Notices::record);
    EXPECT_EQ(previousChange.frame, &changes);
    const auto previousRead = context.activations().setReadHook({});
    EXPECT_EQ(previousRead.fn, &Notices::record);
    EXPECT_EQ(previousRead.frame, &reads);
    EXPECT_EQ(reads.count, 0U);
    EXPECT_EQ(changes.count, 0U);
}

TEST(ActivationScheduler, InvalidRegistrationRollsBackAllAcquiredExtentsBeforeEvaluation) {
    for (unsigned bad = 0; bad != 3; ++bad) {
        k::Context context, foreign;
        Scope scope(context), other(foreign);
        Watch watch{scope.index, scope.array};
        watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
        auto spec = watch.spec();
        if (bad == 0)
            spec.activation.bindings.push_back(other.array);
        else if (bad == 1)
            spec.activation.parts.push_back({scope.array, 3, 1});
        else {
            auto stale = context.activations().create(context.activations().addLayout({}).value);
            ASSERT_TRUE(stale);
            stale.value.reset();
            spec.activation.bindings.push_back({});
        }
        context.scheduler().waitAny(watch.process, {}, {&spec, 1});
        EXPECT_EQ(watch.evals, 0U);
        EXPECT_EQ(context.activations().stats().leases, 1U);
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Internal);
        context.scheduler().kill(watch.process);
    }
}

class Memory final : public std::pmr::memory_resource {
public:
    bool fail = false;
    std::size_t remaining = SIZE_MAX, allocations = 0;

private:
    void* do_allocate(std::size_t size, std::size_t align) override {
        ++allocations;
        if (fail || remaining == 0) throw std::bad_alloc{};
        if (remaining != SIZE_MAX) --remaining;
        return std::pmr::new_delete_resource()->allocate(size, align);
    }
    void do_deallocate(void* pointer, std::size_t size, std::size_t align) override {
        std::pmr::new_delete_resource()->deallocate(pointer, size, align);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ActivationScheduler, CaptureAndNoticeAllocationFailuresReturnResourceLimitsAndRestoreHooks) {
    for (const bool notice : {false, true}) {
        Memory memory;
        k::Context context({nullptr, nullptr}, nullptr, &memory);
        Scope scope(context);
        Watch watch{scope.index, scope.array};
        watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
        Notices reads, changes;
        context.activations().setReadHook({&Notices::record, &reads});
        context.activations().setChangeHook({&Notices::record, &changes});
        auto spec = watch.spec();
        memory.fail = !notice;
        context.scheduler().waitAny(watch.process, {}, {&spec, 1});
        if (notice) {
            memory.fail = true;
            write(context, scope.array, 0, 63, {1, 0});
        }
        EXPECT_TRUE(context.scheduler().heapMonitorLimited());
        EXPECT_EQ(context.scheduler().run(), k::EndReason::ResourceLimit);
        context.scheduler().kill(watch.process);
        EXPECT_EQ(context.activations().stats().leases, 1U);
        EXPECT_EQ(context.activations().setReadHook({}).fn, &Notices::record);
        EXPECT_EQ(context.activations().setChangeHook({}).fn, &Notices::record);
        k::W4 value{};
        EXPECT_EQ(context.activations().read(scope.array, 0, {&value, 1}, 63, 1),
                  k::ActivationError::None);
        EXPECT_EQ(value.a, notice ? 1U : 0U);
    }
}

TEST(ActivationScheduler, CompletedWaitsKeepRegistrationStorageBoundedAndReleasePins) {
    k::Context context;
    Scope scope(context);
    Watch watch{scope.index, scope.array};
    watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
    const auto spec = watch.spec();
    for (unsigned cycle = 0; cycle != 10000; ++cycle) {
        context.scheduler().waitAny(watch.process, {}, {&spec, 1});
        EXPECT_EQ(context.activations().stats().leases, 2U);
        context.scheduler().waitDelay(watch.process, 1);
        EXPECT_EQ(context.activations().stats().leases, 1U);
    }
    EXPECT_LE(context.scheduler().waitListEntries(), 1U);
    EXPECT_EQ(watch.evals, 10000U);
    EXPECT_TRUE(watch.valid);
}

TEST(ActivationScheduler, EveryRegistrationAllocationFailureRestoresOwnershipAndReadHooks) {
    unsigned failures = 0;
    for (std::size_t budget = 0; budget != 32; ++budget) {
        Memory memory;
        k::Context context({nullptr, nullptr}, nullptr, &memory);
        Scope scope(context);
        Watch watch{scope.index, scope.array};
        watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
        Notices reads, changes;
        context.activations().setReadHook({&Notices::record, &reads});
        context.activations().setChangeHook({&Notices::record, &changes});
        const auto spec = watch.spec();
        memory.remaining = budget;
        context.scheduler().waitAny(watch.process, {}, {&spec, 1});
        const bool failed = context.scheduler().heapMonitorLimited();
        memory.remaining = SIZE_MAX;
        context.scheduler().kill(watch.process);
        EXPECT_EQ(context.activations().stats().leases, 1U);
        EXPECT_EQ(context.activations().setReadHook({}).fn, &Notices::record);
        EXPECT_EQ(context.activations().setChangeHook({}).fn, &Notices::record);
        if (!failed) {
            EXPECT_TRUE(watch.valid);
            EXPECT_EQ(watch.evals, 1U);
            break;
        }
        ++failures;
        EXPECT_EQ(context.scheduler().run(), k::EndReason::ResourceLimit);
    }
    // Lease storage, callback pin storage and each distinct read footprint.
    EXPECT_GE(failures, 4U);
    EXPECT_LT(failures, 32U);
}

TEST(ActivationScheduler, EdgeControlsUseAllFourStateTransitionsAfterWideSelection) {
    const std::array<k::W4, 4> states{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    for (const auto edge : {k::Edge::Pos, k::Edge::Neg})
        for (unsigned from = 0; from != 4; ++from)
            for (unsigned to = 0; to != 4; ++to) {
                SCOPED_TRACE(from);
                SCOPED_TRACE(to);
                k::Context context;
                Scope scope(context);
                ASSERT_EQ(context.activations().write(scope.array, 0, {&states[from], 1}, 127, 1),
                          k::ActivationError::None);
                Watch watch{scope.index, scope.array};
                watch.width = 1;
                watch.lsb = 127;
                watch.allow = true;
                watch.process =
                    context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
                auto spec = watch.spec();
                spec.edge = edge;
                context.scheduler().waitAny(watch.process, {}, {&spec, 1});
                write(context, scope.array, 0, 127, states[to]);
                const bool expected = edge == k::Edge::Pos
                                          ? (from == 0 && to != 0) || (from >= 2 && to == 1)
                                          : (from == 1 && to != 1) || (from >= 2 && to == 0);
                EXPECT_EQ(watch.guards, expected ? 1U : 0U);
                EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
                EXPECT_EQ(watch.resumes, expected ? 1U : 0U);
                EXPECT_TRUE(watch.valid);
                context.scheduler().kill(watch.process);
            }
}

struct CancelDuringEval {
    k::ProcessId process{0};
    k::ActivationBinding binding;
    unsigned evals = 0, resumes = 0;
    bool valid = true;
    std::uint64_t live = 0, value = 0;
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<CancelDuringEval*>(frame)->resumes;
        return static_cast<kirima_status>(k::Status::Done);
    }
    static void eval(kirima_context* handle, void* frame, std::uint64_t* out) {
        auto& self = *static_cast<CancelDuringEval*>(frame);
        auto& context = k::fromC(handle);
        if (++self.evals == 2) context.scheduler().kill(self.process);
        k::W4 word{};
        self.live = context.activations().stats().live;
        self.valid = self.valid && context.activations().read(self.binding, 0, {&word, 1}, 63, 1) ==
                                       k::ActivationError::None;
        self.value = word.a;
        out[0] = word.a;
        out[1] = word.b;
    }
};

TEST(ActivationScheduler, SelfCancellationKeepsCallbackBindingsLiveUntilEvaluationReturns) {
    for (const bool capture : {false, true}) {
        k::Context context;
        Scope scope(context);
        CancelDuringEval callback;
        callback.binding = scope.array;
        callback.process =
            context.scheduler().addProcess(&CancelDuringEval::resume, &callback, k::Group::Active);
        k::MonitorSpec spec;
        spec.eval = &CancelDuringEval::eval;
        spec.frame = &callback;
        spec.activation.capture = capture;
        spec.activation.parts = {{scope.array, 0, 1, 63, 1}};
        context.scheduler().waitAny(callback.process, {}, {&spec, 1});
        scope.owner.reset();
        write(context, scope.array, 0, 63, {1, 0});
        EXPECT_EQ(callback.evals, 2U);
        EXPECT_TRUE(callback.valid);
        EXPECT_EQ(callback.value, 1U);
        EXPECT_EQ(callback.live, 1U);
        EXPECT_EQ(context.activations().stats().live, 0U);
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(callback.resumes, 0U);
    }
}

struct ContainerWatch {
    k::ActivationBinding binding;
    k::HeapTypeId type{0};
    unsigned evals = 0, resumes = 0;
    bool valid = true;
    static void eval(kirima_context* handle, void* frame, std::uint64_t* out) {
        auto& watch = *static_cast<ContainerWatch*>(frame);
        auto& context = k::fromC(handle);
        ++watch.evals;
        k::W4 token{}, value{};
        watch.valid = watch.valid && context.activations().read(watch.binding, 0, {&token, 1}) ==
                                         k::ActivationError::None;
        watch.valid = watch.valid &&
                      static_cast<bool>(context.heap().read(watch.type, {token.a}, 0, {&value, 1}));
        out[0] = value.a;
        out[1] = value.b;
    }
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<ContainerWatch*>(frame)->resumes;
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(ActivationScheduler, ContainerReplacementAndCellMutationShareOneWaitAndTypedRootLifetime) {
    k::Context context;
    const auto layout = context.heap().addLayout(std::vector<k::HeapWord>{{UINT64_MAX, true}});
    ASSERT_TRUE(layout);
    const auto type = context.heap().addType({k::ContainerKind::Queue, layout.value});
    ASSERT_TRUE(type);
    auto first = context.heap().fromValues(type.value, std::vector<k::W4>{{1, 0}});
    auto second = context.heap().fromValues(type.value, std::vector<k::W4>{{2, 0}});
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto storage = context.activations().addLayout(
        std::vector<k::ActivationField>{{k::ActivationKind::Container, 64, {}}});
    ASSERT_TRUE(storage);
    auto owner = context.activations().create(storage.value);
    ASSERT_TRUE(owner);
    const auto binding = context.activations().bind(owner.value, 0);
    ASSERT_TRUE(binding);
    k::W4 token{first.value.bits, 0};
    ASSERT_EQ(context.activations().write(binding.value, 0, {&token, 1}), k::ActivationError::None);
    ContainerWatch watch{binding.value, type.value};
    const auto process =
        context.scheduler().addProcess(&ContainerWatch::resume, &watch, k::Group::Active);
    k::MonitorSpec spec;
    spec.heap = true;
    spec.activation.capture = true;
    spec.activation.bindings = {binding.value};
    spec.eval = &ContainerWatch::eval;
    spec.frame = &watch;
    spec.width = 64;
    // Keep waiting while exercising both dependency levels.
    spec.guard = [](kirima_context*, void*) { return 0; };
    context.scheduler().waitAny(process, {}, {&spec, 1});
    owner.value.reset();
    ASSERT_TRUE(context.heap().write(type.value, first.value, 0, std::array<k::W4, 1>{{{3, 0}}}));
    context.scheduler().flushHeapChanges();
    EXPECT_EQ(watch.evals, 2U);
    token.a = second.value.bits;
    ASSERT_EQ(context.scheduler().writeActivation(binding.value, 0, {&token, 1}),
              k::ActivationError::None);
    EXPECT_EQ(watch.evals, 3U);
    ASSERT_TRUE(context.heap().write(type.value, first.value, 0, std::array<k::W4, 1>{{{4, 0}}}));
    context.scheduler().flushHeapChanges();
    EXPECT_EQ(watch.evals, 3U);
    ASSERT_TRUE(context.heap().write(type.value, second.value, 0, std::array<k::W4, 1>{{{5, 0}}}));
    context.scheduler().flushHeapChanges();
    EXPECT_EQ(watch.evals, 4U);
    EXPECT_TRUE(watch.valid);
    std::vector<k::HeapRoot> roots;
    roots.reserve(1);
    context.activations().visitRoots(
        [](void* frame, k::HeapRoot root) noexcept {
            // Preallocated below: the root visitor itself does not allocate.
            static_cast<std::vector<k::HeapRoot>*>(frame)->push_back(root);
        },
        &roots);
    ASSERT_EQ(roots.size(), 1U);
    EXPECT_EQ(roots[0].bits, second.value.bits);
    ASSERT_TRUE(context.heap().collect(roots));
    EXPECT_EQ(context.heap().stats().containers, 1U);
    context.scheduler().kill(process);
    EXPECT_EQ(context.activations().stats().live, 0U);
    ASSERT_TRUE(context.heap().collect({}));
    EXPECT_EQ(context.heap().stats().containers, 0U);
}

struct Cascade {
    k::ActivationBinding binding, target;
    unsigned evals = 0, guards = 0, resumes = 0;
    bool* inside = nullptr;
    bool reentered = false, valid = true;
    static void eval(kirima_context* handle, void* frame, std::uint64_t* out) {
        auto& callback = *static_cast<Cascade*>(frame);
        ++callback.evals;
        callback.reentered = callback.reentered || *callback.inside;
        k::W4 value{};
        callback.valid = callback.valid &&
                         k::fromC(handle).activations().read(callback.binding, 0, {&value, 1}, 63,
                                                             1) == k::ActivationError::None;
        out[0] = value.a;
        out[1] = value.b;
    }
    static int guard(kirima_context* handle, void* frame) {
        auto& callback = *static_cast<Cascade*>(frame);
        ++callback.guards;
        *callback.inside = true;
        const k::W4 value{1, 0};
        callback.valid = callback.valid &&
                         k::fromC(handle).scheduler().writeActivation(
                             callback.target, 0, {&value, 1}, 63, 1) == k::ActivationError::None;
        *callback.inside = false;
        return 1;
    }
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<Cascade*>(frame)->resumes;
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(ActivationScheduler, GuardWritesUseTheOrderedLateQueueWithoutReenteringOtherEvaluators) {
    k::Context context;
    Scope scope(context);
    bool inside = false;
    Cascade first{scope.array, scope.other}, second{scope.other, {}};
    first.inside = &inside;
    second.inside = &inside;
    const auto a = context.scheduler().addProcess(&Cascade::resume, &first, k::Group::Active);
    const auto b = context.scheduler().addProcess(&Cascade::resume, &second, k::Group::Reactive);
    k::MonitorSpec spec;
    spec.eval = &Cascade::eval;
    spec.frame = &second;
    spec.activation.capture = true;
    spec.activation.bindings = {scope.other};
    context.scheduler().waitAny(b, {}, {&spec, 1});
    spec.frame = &first;
    spec.guard = &Cascade::guard;
    spec.guardFrame = &first;
    spec.activation.bindings = {scope.array};
    context.scheduler().waitAny(a, {}, {&spec, 1});
    scope.owner.reset();
    write(context, scope.array, 0, 63, {1, 0});
    EXPECT_EQ(first.evals, 2U);
    EXPECT_EQ(first.guards, 1U);
    EXPECT_EQ(second.evals, 2U);
    EXPECT_FALSE(first.reentered);
    EXPECT_FALSE(second.reentered);
    EXPECT_TRUE(first.valid);
    EXPECT_TRUE(second.valid);
    EXPECT_EQ(context.activations().stats().live, 0U);
    EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(first.resumes, 1U);
    EXPECT_EQ(second.resumes, 1U);
}

TEST(ActivationScheduler, NamedEventCompletionDetachesAutomaticReadsFromTheSameWait) {
    k::Context context;
    Scope scope(context);
    Watch watch{scope.index, scope.array};
    watch.process = context.scheduler().addProcess(&Watch::resume, &watch, k::Group::Active);
    const auto event = context.scheduler().addEvent();
    const auto spec = watch.spec();
    const k::NamedWaitItem named{event};
    context.scheduler().waitAny(watch.process, {}, {&spec, 1}, {&named, 1});
    EXPECT_EQ(context.activations().stats().leases, 2U);
    context.scheduler().trigger(event);
    EXPECT_EQ(context.activations().stats().leases, 1U);
    write(context, scope.array, 0, 63, {1, 0});
    EXPECT_EQ(watch.evals, 1U);
    EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(watch.resumes, 1U);
}

struct InitialWrite {
    k::ActivationBinding binding;
    unsigned evals = 0, resumes = 0;
    bool valid = true;
    static void eval(kirima_context* handle, void* frame, std::uint64_t* out) {
        auto& self = *static_cast<InitialWrite*>(frame);
        auto& context = k::fromC(handle);
        ++self.evals;
        k::W4 before{};
        self.valid = self.valid && context.activations().read(self.binding, 0, {&before, 1}, 63,
                                                              1) == k::ActivationError::None;
        out[0] = before.a;
        out[1] = before.b;
        const k::W4 one{1, 0};
        self.valid =
            self.valid && context.scheduler().writeActivation(self.binding, 0, {&one, 1}, 63, 1) ==
                              k::ActivationError::None;
    }
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<InitialWrite*>(frame)->resumes;
        return static_cast<kirima_status>(k::Status::Done);
    }
};

TEST(ActivationScheduler, InitialEvaluationWritesAreAnnouncedAfterTheWholeWaitRegisters) {
    k::Context context;
    Scope scope(context);
    InitialWrite callback{scope.array};
    const auto process =
        context.scheduler().addProcess(&InitialWrite::resume, &callback, k::Group::Active);
    k::MonitorSpec spec;
    spec.eval = &InitialWrite::eval;
    spec.frame = &callback;
    spec.untilTrue = true;
    spec.activation.capture = true;
    spec.activation.bindings = {scope.array};
    context.scheduler().waitAny(process, {}, {&spec, 1});
    EXPECT_EQ(callback.evals, 2U);
    EXPECT_TRUE(callback.valid);
    EXPECT_EQ(context.activations().stats().leases, 1U);
    EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(callback.resumes, 1U);
}

}  // namespace
