// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The VM takes only a Design that has passed the gate
// (docs/notes/verified-design.md): a plain Design cannot be given to
// vm::run, which is checked at compile time, and a verified one runs.

#include <gtest/gtest.h>
#include <utility>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/types.h"
#include "kirimasim/sir/verified.h"
#include "kirimasim/vm/vm.h"

namespace {

namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;

template <class D>
concept Runnable =
    requires(const D& design, k::Context& context) { kirimasim::vm::run(design, context); };

static_assert(!Runnable<s::Design>, "a plain Design cannot be run");
static_assert(Runnable<s::VerifiedDesign>, "a verified Design can be run");

TEST(VmGate, AVerifiedDesignRuns) {
    // One initial process that returns at once, as in verified_design_test.cpp.
    s::Design d;
    d.sources = {"gate.sv"};
    d.instances = {{"top", "top", s::kNone}};
    s::Process p;
    p.name = "top.initial0";
    p.loc = {0, 1, 1};
    p.blocks.resize(1);
    p.blocks[0].instrs = {{s::Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}}};
    d.processes.push_back(std::move(p));
    auto outcome = s::verifyDesign(std::move(d));
    if (!outcome.verified.has_value()) {
        FAIL() << "a minimal Design does not pass the gate";
    }
    k::Context context;
    const auto result = kirimasim::vm::run(*outcome.verified, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
}

}  // namespace
