// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original typed view/coverage checks of SIR_SPEC 3.5, 10 and the approved
// fixed-array-port contract. Views add no physical kernel slot.
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;

s::Design views() {
    s::Design d;
    d.sources = {"view.sv"};
    d.instances = {{"top", "top", s::kNone}};
    d.slots = {{"top.net", s::u4(65), s::SlotKind::Wire, {{4, 5}, {2, 0}}},
               {"top.child.p", s::u4(65), s::SlotKind::Wire, {{-2, -3}, {7, 9}}},
               {"top.out", s::u4(65)}};
    d.slots[1].viewOf = s::SlotId{0};
    return d;
}

void valid(const s::Design& d) {
    const auto errors = s::verify(d);
    for (const auto& e : errors)
        ADD_FAILURE() << e.where << ": " << e.message;
    ASSERT_TRUE(errors.empty());
}

TEST(ArrayPortSir, ViewsShareStorageAndDeclareCanonicalWrites) {
    auto d = views();
    d.constants = {{s::u4(65), {0x123456789abcdef0ULL, 1}, {0, 0}}};
    auto to = s::Operand::slot(s::SlotId{1});
    to.firstElem = 5;
    to.numElems = 1;
    auto from = to;
    from.id = 0;
    s::Process p;
    p.name = "top.init";
    p.numTemps = 1;
    p.writes = {{s::SlotId{0}, 5, 1}, {s::SlotId{2}}};
    p.blocks.resize(1);
    p.blocks[0].instrs = {
        {s::Op::Store, s::voidType(), s::kNone, {to, s::Operand::constant(s::ConstId{0})}},
        {s::Op::Load, s::u4(65), s::TempId{0}, {from}},
        {s::Op::Store,
         s::voidType(),
         s::kNone,
         {s::Operand::slot(s::SlotId{2}), s::Operand::temp(s::TempId{0})}},
        {s::Op::Ret, s::voidType(), s::kNone, {}}};
    p.loc = {0, 1, 1};
    for (auto& in : p.blocks[0].instrs)
        in.loc = p.loc;
    d.processes.push_back(p);
    valid(d);
    const auto text = s::toText(d);
    EXPECT_NE(text.find("view-of=%s0"), std::string::npos);
    EXPECT_EQ(text, s::toText(d));
    k::Context a, b;
    for (auto* context : {&a, &b}) {
        const auto result = kirimasim::vm::run(d, *context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        ASSERT_EQ(context->store().size(), 2U);
        const auto* value = context->store().element(k::SlotId{1}, 0);
        EXPECT_EQ(value[0], 0x123456789abcdef0ULL);
        EXPECT_EQ(value[1], 0U);
        EXPECT_EQ(value[2], 1U);
        EXPECT_EQ(value[3], 0U);
    }
    EXPECT_NE(a.store().element(k::SlotId{0}, 0), b.store().element(k::SlotId{0}, 0));
}

TEST(ArrayPortSir, RejectsMalformedViewsAndOverflowWithoutFollowingChains) {
    const auto good = views();
    valid(good);
    for (unsigned bad = 0; bad < 12; ++bad) {
        SCOPED_TRACE(bad);
        auto d = good;
        auto& view = d.slots[1];
        switch (bad) {
            case 0: view.viewOf = s::SlotId{1}; break;
            case 1: view.viewOf = s::SlotId{999}; break;
            case 2: view.viewOf = s::SlotId{2}; break;
            case 3: d.slots[0].viewOf = s::SlotId{1}; break;
            case 4: view.type = s::u4(64); break;
            case 5: view.kind = s::SlotKind::Var; break;
            case 6: d.slots[0].kind = s::SlotKind::Var; break;
            case 7: view.dims = {{0, 5}}; break;  // same count, wrong rank
            case 8: view.dims[0].right = -4; break;
            case 9: view.driverElements = {0}; break;
            case 10:
                view.dims = {{std::numeric_limits<std::int64_t>::min(),
                              std::numeric_limits<std::int64_t>::max()}};
                break;
            case 11: view.dims = {{0, 0xffffffffLL}, {0, 0xffffffffLL}}; break;
            default: FAIL() << "unexpected malformed view case " << bad;
        }
        EXPECT_FALSE(s::verify(d).empty());
    }
}

TEST(ArrayPortSir, CompactDriverCoverageIsCheckedAgainstPhysicalElements) {
    auto good = views();
    s::Slot driver{"top.driver", s::u4(65), s::SlotKind::Wire, {{0, 1}}};
    driver.driverOf = s::SlotId{0};
    driver.driverElements = {1, 5};
    good.slots.push_back(driver);
    valid(good);
    EXPECT_NE(s::toText(good).find("driver-elements=[1,5]"), std::string::npos);
    for (unsigned bad = 0; bad < 7; ++bad) {
        SCOPED_TRACE(bad);
        auto d = good;
        auto& slot = d.slots.back();
        switch (bad) {
            case 0: slot.driverElements = {1}; break;
            case 1: slot.driverElements = {5, 1}; break;
            case 2: slot.driverElements = {1, 1}; break;
            case 3: slot.driverElements = {1, 6}; break;
            case 4: slot.driverOf = s::SlotId{1}; break;
            case 5: slot.dims = {{2, 3}}; break;
            case 6: slot.type = s::u2(65); break;
            default: FAIL() << "unexpected malformed driver case " << bad;
        }
        EXPECT_FALSE(s::verify(d).empty());
    }
}

TEST(ArrayPortSir, AliasViewsCannotHideUwireWriterConflicts) {
    auto d = views();
    d.slots[0].kind = s::SlotKind::Uwire;
    d.constants = {{s::u4(65), {1, 0}, {0, 0}}};
    for (std::uint32_t i = 0; i < 2; ++i) {
        s::Process p;
        p.name = "driver" + std::to_string(i);
        p.kind = s::ProcessKind::Cont;
        p.writes = {{s::SlotId{i}, 0, 1}};
        p.blocks.resize(1);
        auto to = s::Operand::slot(s::SlotId{i});
        to.numElems = 1;
        p.blocks[0].instrs = {
            {s::Op::Store, s::voidType(), s::kNone, {to, s::Operand::constant(s::ConstId{0})}},
            {s::Op::Ret, s::voidType(), s::kNone, {}}};
        p.loc = {0, 1, 1};
        for (auto& in : p.blocks[0].instrs)
            in.loc = p.loc;
        d.processes.push_back(p);
    }
    bool conflict = false;
    for (const auto& error : s::verify(d))
        conflict = conflict || error.message.find("also writes") != std::string::npos;
    EXPECT_TRUE(conflict);
}
}  // namespace
