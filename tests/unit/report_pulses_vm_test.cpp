// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// A run must release its diagnostic callback before the interpreter dies,
// including execution errors and allocation failures after hook installation.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
namespace vm = kirimasim::vm;

struct Program {
    s::Design design;
    Program() {
        design.sources = {"report-pulses.sv"};
        design.instances = {{"top", "top", s::kNone}};
        s::Process process;
        process.name = "top.main";
        process.loc = {0, 1, 1};
        process.blocks.resize(1);
        design.processes.push_back(std::move(process));
    }
    void emit(s::Op op, std::vector<s::Operand> operands = {}, s::Type type = s::voidType()) {
        auto& process = design.processes[0];
        const s::TempId result{type == s::voidType() ? s::kNone : process.numTemps++};
        process.blocks[0].instrs.push_back({op, type, result, std::move(operands), {0, 1, 1}});
    }
};

struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file != nullptr) std::fclose(file);
    }
};

struct PriorObserver {
    unsigned calls = 0;
    static void pulse(void* frame, k::PulseKind, std::uint32_t, std::uint64_t) noexcept {
        ++static_cast<PriorObserver*>(frame)->calls;
    }
    k::PulseHook hook() { return {&pulse, this}; }
};

void checkRestored(k::Context& context, PriorObserver& observer) {
    const auto restored = context.scheduler().setPulseHook({});
    ASSERT_EQ(restored.fn, &PriorObserver::pulse);
    ASSERT_EQ(restored.frame, &observer);
    EXPECT_EQ(observer.calls, 0U);
    // Invoke the restored callback only after both identity checks: a VM
    // callback here would retain an interpreter that has already died.
    restored.fn(restored.frame, k::PulseKind::Drive, 0, 0);
    EXPECT_EQ(observer.calls, 1U);
}

TEST(ReportPulsesVm, RestoresPriorObserverAfterNormalRunWithReportingOffAndOn) {
    Program program;
    program.emit(s::Op::Ret);
    ASSERT_TRUE(s::verify(program.design).empty());
    for (const bool reporting : {false, true}) {
        SCOPED_TRACE(reporting);
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context context({capture.file, capture.file});
        PriorObserver observer;
        context.scheduler().setPulseHook(observer.hook());
        vm::RunOptions options;
        options.reportPulses = reporting;
        const auto result = vm::run(program.design, context, options);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        checkRestored(context, observer);
    }
}

TEST(ReportPulsesVm, RestoresPriorObserverAfterDesignErrorAndStepLimit) {
    for (const bool limit : {false, true}) {
        Program program;
        if (limit) {
            program.emit(s::Op::Br, {s::Operand::block(s::BlockId{0})});
        } else {
            program.design.constants.push_back({s::u4(1), {1}, {1}});
            program.emit(s::Op::CondX,
                         {s::Operand::str("if"), s::Operand::constant(s::ConstId{0})});
            program.emit(s::Op::Ret);
        }
        ASSERT_TRUE(s::verify(program.design).empty());
        for (const bool reporting : {false, true}) {
            SCOPED_TRACE(::testing::Message() << limit << '/' << reporting);
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            k::Context context({capture.file, capture.file});
            PriorObserver observer;
            context.scheduler().setPulseHook(observer.hook());
            vm::RunOptions options;
            options.reportPulses = reporting;
            options.condX = vm::CondXMode::Error;
            options.maxSteps = 2;
            const auto result = vm::run(program.design, context, options);
            EXPECT_EQ(result.reason, limit ? k::EndReason::ZeroTimeLimit : k::EndReason::Error)
                << result.error;
            checkRestored(context, observer);
        }
    }
}

struct FailingMemory : std::pmr::memory_resource {
    bool fail = false;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (fail) throw std::bad_alloc();
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ReportPulsesVm, EventAllocationFailureDoesNotLeaveAnInterpreterCallback) {
    Program program;
    program.emit(s::Op::EventNew, {}, {s::TypeKind::Event, 0});
    program.emit(s::Op::Ret);
    ASSERT_TRUE(s::verify(program.design).empty());
    for (const bool reporting : {false, true}) {
        SCOPED_TRACE(reporting);
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        FailingMemory memory;
        k::Context context({capture.file, capture.file}, nullptr, &memory);
        PriorObserver observer;
        context.scheduler().setPulseHook(observer.hook());
        memory.fail = true;
        vm::RunOptions options;
        options.reportPulses = reporting;
        const auto result = vm::run(program.design, context, options);
        EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
        EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
        checkRestored(context, observer);
    }
}
}  // namespace
