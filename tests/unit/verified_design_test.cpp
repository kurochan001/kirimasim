// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "kirimasim/sir/pattern.h"
#include "kirimasim/sir/verified.h"

namespace {
using namespace kirimasim::sir;

// Only verifyDesign makes a VerifiedDesign (docs/notes/verified-design.md).
static_assert(!std::is_constructible_v<VerifiedDesign, Design>);
static_assert(!std::is_constructible_v<VerifiedDesign, const Design&>);
static_assert(!std::is_default_constructible_v<VerifiedDesign>);
static_assert(
    std::is_same_v<decltype(std::declval<const VerifiedDesign&>().design()), const Design&>);

Instr instruction(Op op, std::vector<Operand> operands = {}) {
    return {op, voidType(), kNone, std::move(operands), {0, 1, 1}};
}

/// One initial process that returns at once.
Design minimal() {
    Design d;
    d.sources = {"verified.sv"};
    d.instances = {{"top", "top", kNone}};
    Process p;
    p.name = "top.initial0";
    p.loc = {0, 1, 1};
    p.blocks.resize(1);
    p.blocks[0].instrs = {instruction(Op::Ret)};
    d.processes.push_back(std::move(p));
    return d;
}

/// The gate gives exactly the errors of the three checks it joins, and no
/// VerifiedDesign; `which` is the check expected to report.
void rejected(const Design& design, int which) {
    const auto control = verify(design).size();
    const auto scopes = verifyActivationScopes(design).size();
    const auto patterns = validatePatterns(design).errors.size();
    const std::size_t counts[] = {control, scopes, patterns};
    EXPECT_GT(counts[which], 0U) << "check " << which << " does not report";
    auto outcome = verifyDesign(design);
    EXPECT_FALSE(outcome.verified.has_value());
    EXPECT_EQ(outcome.errors.size(), control + scopes + patterns);
}

TEST(VerifiedDesign, AValidDesignPassesTheGate) {
    auto outcome = verifyDesign(minimal());
    ASSERT_TRUE(outcome.errors.empty()) << outcome.errors.front().message;
    if (!outcome.verified.has_value()) {
        FAIL() << "a valid Design gives no VerifiedDesign";
    }
    EXPECT_EQ(outcome.verified->design().processes.size(), 1U);
    EXPECT_EQ(outcome.verified->design().processes[0].name, "top.initial0");
}

TEST(VerifiedDesign, AReleasedDesignIsVerifiedAgain) {
    auto outcome = verifyDesign(minimal());
    if (!outcome.verified.has_value()) {
        FAIL() << "a valid Design gives no VerifiedDesign";
    }
    Design released = std::move(*outcome.verified).release();
    released.processes[0].blocks[0].instrs.clear();  // no terminator any more
    auto again = verifyDesign(std::move(released));
    EXPECT_FALSE(again.verified.has_value());
    EXPECT_FALSE(again.errors.empty());
}

TEST(VerifiedDesign, EachKindOfCheckStopsAtTheGate) {
    // The verifier: a block with no terminator.
    Design control = minimal();
    control.processes[0].blocks[0].instrs.clear();
    rejected(control, 0);

    // Activation scopes: a capture from a frame variable that does not exist.
    Design scopes = minimal();
    scopes.processes[0].frame = {{"value", u4(32)}};
    scopes.processes[0].frame[0].captureSource = FrameVarId{99};
    rejected(scopes, 1);

    // Pattern layouts: an array whose element is no layout.
    Design patterns = minimal();
    PatternLayout layout;
    layout.kind = PatternLayout::Kind::Array;
    layout.element = PatternId{99};
    layout.count = 1;
    patterns.patterns.push_back(layout);
    rejected(patterns, 2);
}

}  // namespace
