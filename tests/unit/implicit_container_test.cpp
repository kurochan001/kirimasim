// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;

struct Observer {
    unsigned resumes = 0;
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<Observer*>(frame)->resumes;
        return static_cast<kirima_status>(k::Status::Waiting);
    }
};
k::MonitorSpec binding(k::SlotId slot, k::HeapTypeId type) {
    k::MonitorSpec spec;
    spec.implicitContainer = k::ImplicitContainerBinding{slot, type};
    return spec;
}
struct Notices {
    unsigned count = 0;
    static void record(void* raw, k::HeapRoot) noexcept { ++static_cast<Notices*>(raw)->count; }
};

// The oracle is a sequence of bit strings. It knows neither heap identities
// nor monitor records, and compares source values with ordinary string equality.
using Model = std::vector<std::string>;
std::vector<k::W4> encode(const Model& model) {
    std::vector<k::W4> result;
    for (const auto& bits : model) {
        const auto start = result.size();
        result.resize(start + (bits.size() + 63) / 64);
        for (std::size_t i = 0; i < bits.size(); ++i) {
            auto& word = result[start + i / 64];
            const auto mask = std::uint64_t{1} << (i % 64);
            if (bits[i] == '1' || bits[i] == 'x') word.a |= mask;
            if (bits[i] == 'x' || bits[i] == 'z') word.b |= mask;
        }
    }
    return result;
}

TEST(ImplicitContainer, ReplacementTracesMatchIndependentBitStringModel) {
    for (const auto group : {k::Group::Active, k::Group::Reactive})
        for (const auto kind : {k::ContainerKind::Queue, k::ContainerKind::DynamicArray})
            for (const bool four : {false, true})
                for (const unsigned width : {1U, 7U, 65U, 130U}) {
                    SCOPED_TRACE(::testing::Message()
                                 << static_cast<int>(group) << ' ' << static_cast<int>(kind) << ' '
                                 << four << ' ' << width);
                    k::Context ctx;
                    auto& heap = ctx.heap();
                    auto& scheduler = ctx.scheduler();
                    std::vector<k::HeapWord> shape((width + 63) / 64);
                    for (auto& word : shape)
                        word.fourState = four;
                    shape.back().mask = k::mask((width - 1) % 64 + 1);
                    const auto layout = heap.addLayout(shape);
                    ASSERT_TRUE(layout);
                    const auto type = heap.addType({kind, layout.value});
                    ASSERT_TRUE(type);
                    const auto slot = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
                    Observer observer;
                    const auto p = scheduler.addProcess(&Observer::resume, &observer, group);
                    const std::string zero(width, '0'), one(width, '1');
                    const std::string x(width, four ? 'x' : '0'), z(width, four ? 'z' : '0');
                    const std::vector<Model> script{{},  {zero}, {zero},      {one},
                                                    {x}, {z},    {one, zero}, {zero, one},
                                                    {},  {},     {one}};
                    Model before;
                    unsigned expected = 0;
                    for (const auto& after : script) {
                        auto value = heap.fromValues(type.value, encode(after));
                        ASSERT_TRUE(value);
                        const auto stats = heap.stats();
                        Notices reads, allocations;
                        heap.setReadHook({&Notices::record, &reads});
                        heap.setAllocationHook({&Notices::record, &allocations});
                        scheduler.waitMonitor(p, binding(slot, type.value));
                        EXPECT_FALSE(scheduler.ending());
                        EXPECT_EQ(reads.count, 0U);
                        EXPECT_EQ(allocations.count, 0U);
                        EXPECT_EQ(heap.stats().containers, stats.containers);
                        EXPECT_EQ(heap.stats().cells, stats.cells);
                        heap.setReadHook({});
                        heap.setAllocationHook({});
                        scheduler.write({.slot = slot}, &value.value.bits);
                        expected += before != after;
                        EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
                        EXPECT_EQ(observer.resumes, expected);
                        const std::array roots{k::HeapRoot::container(value.value)};
                        ASSERT_TRUE(heap.collect(roots));
                        before = after;
                    }
                    scheduler.cancelWait(p);
                }
}

TEST(ImplicitContainer, EqualCopyReattachesAndDetachedReferencesDoNotWake) {
    k::Context ctx;
    auto& heap = ctx.heap();
    auto& scheduler = ctx.scheduler();
    const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
    ASSERT_TRUE(layout);
    const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
    ASSERT_TRUE(type);
    auto original = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}, {2, 0}});
    ASSERT_TRUE(original);
    const auto oldCell = heap.cell(type.value, original.value, 0);
    ASSERT_TRUE(oldCell);
    const auto slot = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    *ctx.store().element(slot, 0) = original.value.bits;
    Observer observer;
    const auto p = scheduler.addProcess(&Observer::resume, &observer, k::Group::Active);
    scheduler.waitMonitor(p, binding(slot, type.value));
    const auto copied = heap.copy(type.value, original.value);
    ASSERT_TRUE(copied);
    scheduler.write({.slot = slot}, &copied.value.bits);
    ASSERT_TRUE(heap.writeCell(layout.value, oldCell.value, std::vector<k::W4>{{9, 0}}));
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(observer.resumes, 0U);
    // A -> B -> A before a flush remains a change, despite unchanged size.
    auto current = copied.value;
    ASSERT_TRUE(heap.write(type.value, current, 0, std::vector<k::W4>{{3, 0}}));
    ASSERT_TRUE(heap.write(type.value, current, 0, std::vector<k::W4>{{1, 0}}));
    scheduler.flushHeapChanges();
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(observer.resumes, 1U);
    scheduler.waitMonitor(p, binding(slot, type.value));
    ASSERT_TRUE(heap.write(type.value, current, 0, std::vector<k::W4>{{0xff, 0xff}}));
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(observer.resumes, 2U);
    scheduler.waitMonitor(p, binding(slot, type.value));
    ASSERT_TRUE(heap.write(type.value, current, 0, std::vector<k::W4>{{0, 0xff}}));
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(observer.resumes, 3U);  // X -> Z, with unchanged size and b bits.
    scheduler.cancelWait(p);
    ASSERT_TRUE(heap.collect(std::vector<k::HeapRoot>{k::HeapRoot::container(current),
                                                      k::HeapRoot::cell(oldCell.value)}));
    EXPECT_EQ(heap.actualType(original.value).status.error, k::HeapError::InvalidHandle);
    ASSERT_TRUE(heap.writeCell(layout.value, oldCell.value, std::vector<k::W4>{{4, 0}}));
    const auto recycled = heap.fromValues(type.value, std::vector<k::W4>{{7, 0}});
    ASSERT_TRUE(recycled);
    EXPECT_NE(recycled.value.bits, original.value.bits);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(observer.resumes, 3U);
}

TEST(ImplicitContainer, BatchPublicationAndOrWaitResumeEachOwnerOnce) {
    k::Context ctx;
    auto& heap = ctx.heap();
    auto& scheduler = ctx.scheduler();
    const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
    ASSERT_TRUE(layout);
    const auto type = heap.addType({k::ContainerKind::Queue, layout.value, 2});
    ASSERT_TRUE(type);
    auto value = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}, {2, 0}});
    ASSERT_TRUE(value);
    const auto slot = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    const auto signal = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    *ctx.store().element(slot, 0) = value.value.bits;
    std::array<Observer, 3> observers;
    std::array<k::ProcessId, 3> ids;
    for (std::size_t i = 0; i < observers.size(); ++i) {
        ids[i] = scheduler.addProcess(&Observer::resume, &observers[i], k::Group::Active);
        const k::WaitItem item{{signal}};
        const std::array monitors{binding(slot, type.value), binding(slot, type.value)};
        scheduler.waitAny(ids[i], {&item, 1}, monitors);
    }
    ASSERT_TRUE(heap.write(type.value, value.value, -1, std::vector<k::W4>{{3, 0}}));
    ASSERT_TRUE(heap.swapValues(type.value, value.value, 0, 0));
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    for (const auto& observer : observers)
        EXPECT_EQ(observer.resumes, 0U);
    ASSERT_TRUE(heap.swapValues(type.value, value.value, 0, 1));
    const std::uint64_t one = 1;
    scheduler.write({.slot = signal}, &one);
    ASSERT_TRUE(heap.swapValues(type.value, value.value, 0, 1));
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    for (const auto& observer : observers)
        EXPECT_EQ(observer.resumes, 1U);
    for (const auto id : ids)
        scheduler.cancelWait(id);
}

TEST(ImplicitContainer, TransientChangesInsideAnExpressionWalkAreNotCollapsed) {
    for (const bool replace : {false, true}) {
        k::Context ctx;
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        const auto original = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}});
        const auto other = heap.fromValues(type.value, std::vector<k::W4>{{9, 0}});
        ASSERT_TRUE(original);
        ASSERT_TRUE(other);
        const auto slot = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
        *ctx.store().element(slot, 0) = original.value.bits;
        Observer observer, dummy;
        const auto p = scheduler.addProcess(&Observer::resume, &observer, k::Group::Active);
        scheduler.waitMonitor(p, binding(slot, type.value));
        struct Walk {
            k::HeapTypeId type;
            k::ContainerToken original, other;
            k::SlotId slot;
            bool replace, once = true, valid = true;
            static void eval(kirima_context* raw, void* frame, std::uint64_t* out) {
                auto& self = *static_cast<Walk*>(frame);
                auto& context = k::fromC(raw);
                if (self.once) {
                    self.once = false;
                    if (self.replace) {
                        context.scheduler().write({.slot = self.slot}, &self.other.bits);
                        context.scheduler().write({.slot = self.slot}, &self.original.bits);
                    } else {
                        self.valid = static_cast<bool>(context.heap().write(
                            self.type, self.original, 0, std::vector<k::W4>{{9, 0}}));
                        self.valid = self.valid &&
                                     static_cast<bool>(context.heap().write(
                                         self.type, self.original, 0, std::vector<k::W4>{{1, 0}}));
                    }
                }
                out[0] = 0;
            }
        } walk{type.value, original.value, other.value, slot, replace};
        k::MonitorSpec expression;
        expression.eval = &Walk::eval;
        expression.frame = &walk;
        expression.domain = k::Domain::Two;
        const auto q = scheduler.addProcess(&Observer::resume, &dummy, k::Group::Active);
        scheduler.waitMonitor(q, expression);
        EXPECT_TRUE(walk.valid);
        EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(observer.resumes, 1U);
        scheduler.cancelWait(q);
        scheduler.cancelWait(p);
    }
}

class FailMemory final : public std::pmr::memory_resource {
public:
    int remaining = -1;

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc{};
        if (remaining > 0) --remaining;
        return std::pmr::new_delete_resource()->allocate(size, alignment);
    }
    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, size, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ImplicitContainer, AllocationFailureAndCancellationRestoreHooksWithoutAllocating) {
    for (const int allowed : {0, 1, 2, 3}) {
        FailMemory memory;
        k::Context ctx({}, nullptr, &memory);
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        auto value = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}, {2, 0}});
        ASSERT_TRUE(value);
        const auto slot = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
        *ctx.store().element(slot, 0) = value.value.bits;
        Observer observer;
        Notices previous;
        heap.setChangeHook({&Notices::record, &previous});
        const auto p = scheduler.addProcess(&Observer::resume, &observer, k::Group::Active);
        memory.remaining = allowed;
        EXPECT_NO_THROW(scheduler.waitMonitor(p, binding(slot, type.value)));
        if (!scheduler.ending()) {
            memory.remaining = 0;
            // Erasure itself requires no allocation; its committed snapshot
            // cannot be captured, so the design ends with a resource error.
            ASSERT_TRUE(heap.erase(type.value, value.value, 0));
        }
        EXPECT_EQ(scheduler.reason(), k::EndReason::ResourceLimit);
        EXPECT_EQ(observer.resumes, 0U);
        EXPECT_NO_THROW(scheduler.cancelWait(p));
        const auto old = heap.setChangeHook({});
        EXPECT_EQ(old.fn, &Notices::record);
        EXPECT_EQ(old.frame, &previous);
        memory.remaining = -1;
    }
}

TEST(ImplicitContainer, RepeatedEqualCopiesKeepWaitStorageBoundedAndContextsIndependent) {
    k::Context first, second;
    std::array<k::Context*, 2> contexts{&first, &second};
    std::array<Observer, 2> observers;
    std::array<k::HeapTypeId, 2> types;
    std::array<k::ContainerToken, 2> values;
    std::array<k::ProcessId, 2> processes;
    for (std::size_t i = 0; i < contexts.size(); ++i) {
        auto& ctx = *contexts[i];
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        auto value = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}});
        ASSERT_TRUE(value);
        const auto slot = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
        *ctx.store().element(slot, 0) = value.value.bits;
        const auto p = scheduler.addProcess(&Observer::resume, &observers[i], k::Group::Active);
        types[i] = type.value;
        processes[i] = p;
        for (unsigned n = 0; n < 1000; ++n) {
            scheduler.waitMonitor(p, binding(slot, type.value));
            const auto copy = heap.copy(type.value, value.value);
            ASSERT_TRUE(copy);
            value.value = copy.value;
            scheduler.write({.slot = slot}, &value.value.bits);
            ASSERT_TRUE(
                heap.collect(std::vector<k::HeapRoot>{k::HeapRoot::container(value.value)}));
        }
        EXPECT_LE(scheduler.waitListEntries(), 8U);
        EXPECT_EQ(heap.stats().containers, 1U);
        EXPECT_EQ(heap.stats().cells, 1U);
        values[i] = value.value;
    }
    // Both contexts have live watches with the same local ID space.
    EXPECT_EQ(observers[0].resumes, 0U);
    EXPECT_EQ(observers[1].resumes, 0U);
    for (std::size_t i = 0; i < contexts.size(); ++i) {
        auto& ctx = *contexts[i];
        ASSERT_TRUE(ctx.heap().write(types[i], values[i], 0, std::vector<k::W4>{{2, 0}}));
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(observers[i].resumes, 1U);
        if (i == 0) EXPECT_EQ(observers[1].resumes, 0U);
        ctx.scheduler().cancelWait(processes[i]);
    }
    EXPECT_EQ(observers[0].resumes, 1U);
    EXPECT_EQ(observers[1].resumes, 1U);
}

s::Design observerDesign() {
    s::Design d;
    d.sources = {"observer.sv"};
    d.instances = {{"top", "top", s::kNone}};
    s::TypeEntry row;
    row.name = "queue";
    row.kind = s::TypeEntryKind::Queue;
    row.value = s::u4(65);
    d.types.push_back(row);
    d.slots = {{"top.q", s::queueType(s::TypeId{0})}};
    s::Process process;
    process.name = "top.observer";
    process.kind = s::ProcessKind::CombAt;
    process.implicitContainers = {{s::SlotId{0}, s::TypeId{0}}};
    process.blocks.resize(1);
    process.blocks[0].instrs.push_back({s::Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}});
    d.processes.push_back(std::move(process));
    return d;
}

TEST(ImplicitContainerSir, TypedMetadataVerifiesPrintsAndArmsWithoutAUserEvaluation) {
    auto d = observerDesign();
    for (const auto& error : s::verify(d))
        ADD_FAILURE() << error.where << ": " << error.message;
    ASSERT_TRUE(s::verify(d).empty());
    EXPECT_NE(s::toText(d).find("implicit-container=%s0:!t0"), std::string::npos);
    k::Context ctx;
    const auto result = kirimasim::vm::run(d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(ctx.heap().stats().containers, 0U);
    EXPECT_EQ(ctx.heap().stats().cells, 0U);
}

TEST(ImplicitContainerSir, MalformedMetadataIsRejectedBeforeRuntimeAllocation) {
    for (unsigned fault = 0; fault < 10; ++fault) {
        SCOPED_TRACE(fault);
        auto d = observerDesign();
        ASSERT_TRUE(s::verify(d).empty());
        switch (fault) {
            case 0: d.processes[0].kind = s::ProcessKind::Initial; break;
            case 1: d.processes[0].implicitContainers[0].slot = s::SlotId{99}; break;
            case 2: d.processes[0].implicitContainers[0].type = s::TypeId{99}; break;
            case 3: d.slots[0].dims = {{0, 1}}; break;
            case 4: d.types[0].value = s::Type{s::TypeKind::Real, 0}; break;
            case 5: d.types[0].value = s::Type{s::TypeKind::String, 0}; break;
            case 6: d.slots[0].type = s::Type{s::TypeKind::Chandle, 0}; break;
            case 7: d.processes[0].triggers = {{{s::SlotId{0}}}}; break;
            case 8:
                d.processes[0].implicitContainers.push_back(d.processes[0].implicitContainers[0]);
                break;
            case 9: d.processes[0].parent = s::Owner::of(s::ProcessId{0}); break;
            default: FAIL() << "unknown descriptor fault"; break;
        }
        EXPECT_FALSE(s::verify(d).empty());
        k::Context ctx;
        const auto result = kirimasim::vm::run(d, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Internal);
        EXPECT_NE(result.error.find("invalid implicit container SIR"), std::string::npos);
        EXPECT_EQ(ctx.store().size(), 0U);
        EXPECT_EQ(ctx.heap().stats().containers, 0U);
    }
}
}  // namespace
