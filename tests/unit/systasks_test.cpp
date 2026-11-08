// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The table of system tasks and built-in methods (systasks.def) and its two
// lookups: every name is found with what the file says, names are unique,
// every kind has a name, and a name SIR does not have is not found.

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/sir/systasks.h"

namespace {

using kirimasim::sir::BuiltinMethod;
using kirimasim::sir::builtinMethod;
using kirimasim::sir::sysTask;
using kirimasim::sir::SysTaskKind;

struct Row {
    SysTaskKind kind;
    std::string_view name;
    std::uint32_t minOperands, formatStart;
};

/// The row of a name; a name that is not found fails the test and gives a
/// default row.
kirimasim::sir::SysTaskInfo infoOf(std::string_view name) {
    const auto found = sysTask(name);
    EXPECT_TRUE(found.has_value()) << name;
    return found.value_or(kirimasim::sir::SysTaskInfo{});
}

const std::vector<Row>& rows() {
    static const std::vector<Row> all = {
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)                                \
    {SysTaskKind::Kind, name, minOperands, formatStart},
#define KIRIMASIM_SIR_METHOD(Kind, name)
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
    };
    return all;
}

TEST(SysTasks, EveryNameIsFoundWithItsRow) {
    std::set<std::string_view> names;
    for (const Row& row : rows()) {
        SCOPED_TRACE(std::string(row.name));
        EXPECT_TRUE(row.name.starts_with('$'));
        EXPECT_TRUE(names.insert(row.name).second) << "listed twice";
        EXPECT_GE(row.minOperands, 1U);
        EXPECT_LT(row.formatStart, 4U);
        const auto found = infoOf(row.name);
        EXPECT_EQ(found.kind, row.kind);
        EXPECT_EQ(found.minOperands, row.minOperands);
        EXPECT_EQ(found.formatStart, row.formatStart);
    }
}

TEST(SysTasks, EveryKindHasAName) {
    std::set<SysTaskKind> named;
    for (const Row& row : rows())
        named.insert(row.kind);
    std::size_t kinds = 0;
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)                                                           \
    ++kinds;                                                                                       \
    EXPECT_TRUE(named.contains(SysTaskKind::Kind)) << #Kind;
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)
#define KIRIMASIM_SIR_METHOD(Kind, name)
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
    EXPECT_EQ(named.size(), kinds);
}

// Rows that a handler depends on, written out here so that a change of the
// file shows in this test too.
TEST(SysTasks, RowsTheHandlersReadByPosition) {
    const auto display = infoOf("$displayh");
    EXPECT_EQ(display.kind, SysTaskKind::Display);
    EXPECT_EQ(display.formatStart, 1U);
    const auto fatal = infoOf("$fatal");
    EXPECT_EQ(fatal.minOperands, 3U);
    EXPECT_EQ(fatal.formatStart, 3U);
    for (const std::string_view two : {"$dist_uniform", "$dist_normal", "$dist_erlang"}) {
        const auto dist = infoOf(two);
        EXPECT_EQ(dist.kind, SysTaskKind::Dist) << two;
        EXPECT_EQ(dist.minOperands, 4U) << two;
    }
    for (const std::string_view one :
         {"$dist_exponential", "$dist_poisson", "$dist_chi_square", "$dist_t"}) {
        const auto dist = infoOf(one);
        EXPECT_EQ(dist.kind, SysTaskKind::Dist) << one;
        EXPECT_EQ(dist.minOperands, 3U) << one;
    }
    EXPECT_EQ(infoOf("$vpi.call").minOperands, 7U);
}

TEST(SysTasks, NamesSirDoesNotHaveAreNotFound) {
    for (const std::string_view name : {"", "$", "display", "$Display", "$display ", "$dist_",
                                        "$dist_gamma", "$stime", "$psprintf", "$countones"})
        EXPECT_FALSE(sysTask(name).has_value()) << name;
    for (const std::string_view name : {"", "Len", "length", "$len", "size"})
        EXPECT_FALSE(builtinMethod(name).has_value()) << name;
}

TEST(SysTasks, EveryMethodIsFound) {
    std::set<std::string_view> names;
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)
#define KIRIMASIM_SIR_METHOD(Kind, name)                                                           \
    EXPECT_TRUE(names.insert(name).second) << (name);                                              \
    EXPECT_EQ(builtinMethod(name), BuiltinMethod::Kind) << (name);
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
    EXPECT_EQ(names.size(), 19U);
}

}  // namespace
