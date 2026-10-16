#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {

using kirimasim::kernel::Context;

TEST(Counters, KeepInsertionOrderInJson) {
    Context context;
    auto& c = context.counters();
    const auto b = c.add("b.second");
    const auto a = c.add("a.first");
    c.increment(b, 3);
    c.increment(a);
    EXPECT_EQ(c.toJson(), "{\"b.second\": 3, \"a.first\": 1}\n");
}

TEST(Counters, AddingAnExistingNameReturnsTheSameId) {
    Context context;
    auto& c = context.counters();
    EXPECT_EQ(c.add("x"), c.add("x"));
    EXPECT_EQ(c.size(), 1U);
}

TEST(Counters, EscapeNamesInJson) {
    Context context;
    auto& c = context.counters();
    c.add("quote\"back\\slash");
    EXPECT_EQ(c.toJson(), "{\"quote\\\"back\\\\slash\": 0}\n");
}

TEST(Counters, ContextsAreIndependent) {
    Context one;
    Context two;
    one.counters().increment(one.counters().add("n"), 5);
    EXPECT_EQ(two.counters().size(), 0U);
}

}  // namespace
