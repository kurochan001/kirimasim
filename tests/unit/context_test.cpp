// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The Context's own rules (RUNTIME_ABI.md 9.3): it cannot be copied or
// moved, and it writes only to the destinations it was made with.

#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>

#include <sys/stat.h>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {

using kirimasim::kernel::Context;
using kirimasim::kernel::OutputDestinations;

static_assert(!std::is_copy_constructible_v<Context>);
static_assert(!std::is_copy_assignable_v<Context>);
static_assert(!std::is_move_constructible_v<Context>);
static_assert(!std::is_move_assignable_v<Context>);

struct CloseFile {
    void operator()(std::FILE* f) const { std::fclose(f); }
};
using File = std::unique_ptr<std::FILE, CloseFile>;

std::string contents(std::FILE* f) {
    std::fflush(f);
    std::fseek(f, 0, SEEK_SET);
    std::string text;
    char buffer[256];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0)
        text.append(buffer, n);
    return text;
}

// Each of two Contexts writes its output and its messages, one after the
// other, to its own pair of files; each file holds only what its Context
// wrote, in order.
TEST(Context, WritesOnlyWhereItWasToldTo) {
    const File out1(std::tmpfile()), err1(std::tmpfile()), out2(std::tmpfile()),
        err2(std::tmpfile());
    ASSERT_TRUE(out1 && err1 && out2 && err2);
    Context one(OutputDestinations{out1.get(), err1.get()});
    Context two(OutputDestinations{out2.get(), err2.get()});
    for (int i = 0; i < 2; ++i) {
        for (Context* c : {&one, &two}) {
            const std::string name = c == &one ? "one" : "two";
            c->output().write(name + " out " + std::to_string(i) + "\n");
            c->output().message(name + " message " + std::to_string(i));
        }
    }
    two.output().write("two no newline");
    two.output().flush();
    EXPECT_EQ(contents(out1.get()), "one out 0\none out 1\n");
    EXPECT_EQ(contents(err1.get()), "one message 0\none message 1\n");
    EXPECT_EQ(contents(out2.get()), "two out 0\ntwo out 1\ntwo no newline");
    EXPECT_EQ(contents(err2.get()), "two message 0\ntwo message 1\n");
    EXPECT_TRUE(one.output().atLineStart());
    EXPECT_FALSE(two.output().atLineStart());
}

// A message flushes the output's destination first, so that a reader of
// both sees the output that came before it (CLI.md 3).
TEST(Context, MessageFlushesTheOutputFirst) {
    const File out(std::tmpfile()), err(std::tmpfile());
    ASSERT_TRUE(out && err);
    Context c(OutputDestinations{out.get(), err.get()});
    struct stat st{};
    c.output().write("before");
    ASSERT_EQ(fstat(fileno(out.get()), &st), 0);
    EXPECT_EQ(st.st_size, 0);  // still in the stream's buffer
    c.output().message("m");
    ASSERT_EQ(fstat(fileno(out.get()), &st), 0);
    EXPECT_EQ(st.st_size, 6);
    EXPECT_EQ(contents(err.get()), "m\n");
}

}  // namespace
