// The kernel's dump (WAVEFORM.md): what the suite's designs cannot see from
// inside, as they read their dump while it runs: the end of the run, and a
// file that cannot be opened. And the patterns of --wave-scope.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/dump.h"

namespace {

using kirimasim::kernel::Context;
using kirimasim::kernel::DumpCall;
using kirimasim::kernel::DumpScope;
using kirimasim::kernel::DumpScopeKind;
using kirimasim::kernel::DumpSetup;
using kirimasim::kernel::DumpType;
using kirimasim::kernel::DumpVar;
using kirimasim::kernel::globMatch;
using kirimasim::kernel::OutputDestinations;

struct CloseFile {
    void operator()(std::FILE* f) const { std::fclose(f); }
};
using File = std::unique_ptr<std::FILE, CloseFile>;

std::string contents(const std::filesystem::path& p) {
    std::ifstream in(p);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

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

/// A design of one 4-bit variable top.v, and where its dump goes.
struct OneVariable {
    File out{std::tmpfile()};
    File err{std::tmpfile()};
    Context ctx{OutputDestinations{out.get(), err.get()}};
    std::filesystem::path file;

    explicit OneVariable(const std::string& name) {
        file = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove(file);
        const auto v =
            ctx.store().add(kirimasim::kernel::Domain::Four, 4, 1, kirimasim::kernel::Init::X);
        DumpSetup s;
        s.scopes.push_back(DumpScope{"top", "top", DumpScopeKind::Module, UINT32_MAX});
        s.vars.push_back(DumpVar{0, "v", "[3:0]", DumpType::Reg, 4, v, false});
        s.date = "2026-10-21 00:00:00 UTC";
        s.version = "kirimasim test";
        s.timescale = "1 ns";
        ctx.dump().setup(std::move(s));
    }
    ~OneVariable() { std::filesystem::remove(file); }
};

constexpr const char* kHeader =
    "$date\n\t2026-10-21 00:00:00 UTC\n$end\n$version\n\tkirimasim test\n\t$dumpfile(\"x\")\n"
    "$end\n$timescale\n\t1 ns\n$end\n$scope module top $end\n$var reg 4 ! v [3:0] $end\n"
    "$upscope $end\n$enddefinitions $end\n";

// A run that ends in the time slot of $dumpvars, as by $finish, still
// starts the dump at its end, and the file is complete (WAVEFORM.md 4).
TEST(Dump, TheEndOfTheRunStartsADumpItsSlotDidNotEnd) {
    OneVariable d("kirimasim_dump_test_start.vcd");
    ASSERT_EQ(d.ctx.dump().file(d.file.string(), "$dumpfile(\"x\")"), DumpCall::Done);
    ASSERT_EQ(d.ctx.dump().vars(0, 0, {}, {}, true), DumpCall::Done);
    d.ctx.dump().finish(d.ctx.scheduler());
    EXPECT_EQ(contents(d.file), std::string(kHeader) + "#0\n$dumpvars\nbx !\n$end\n");
}

// What changed after the last end of a time slot is written when the run
// ends (by final procedures, or in a slot cut short), with no second time
// line for the same time.
TEST(Dump, TheEndOfTheRunWritesTheLastChanges) {
    OneVariable d("kirimasim_dump_test_last.vcd");
    d.ctx.dump().file(d.file.string(), "$dumpfile(\"x\")");
    d.ctx.dump().vars(0, 0, {}, {}, true);
    d.ctx.dump().endSlot(d.ctx.scheduler());
    std::uint64_t* w = d.ctx.store().element(kirimasim::kernel::SlotId{0}, 0);
    w[0] = 0b0101;  // a: 0101, b: 0
    w[1] = 0;
    d.ctx.dump().finish(d.ctx.scheduler());
    EXPECT_EQ(contents(d.file), std::string(kHeader) + "#0\n$dumpvars\nbx !\n$end\nb101 !\n");
    // Nothing after the end.
    EXPECT_EQ(d.ctx.dump().vars(0, 0, {}, {}, true), DumpCall::Late);
}

// A file that cannot be opened is a warning, and nothing is dumped.
TEST(Dump, AFileThatCannotBeOpenedIsAWarning) {
    OneVariable d("kirimasim_dump_test_unused.vcd");
    const std::string bad = (d.file.parent_path() / "no such directory" / "x.vcd").string();
    d.ctx.dump().file(bad, "$dumpfile(\"x\")");
    d.ctx.dump().vars(0, 0, {}, {}, true);
    d.ctx.dump().endSlot(d.ctx.scheduler());
    d.ctx.dump().finish(d.ctx.scheduler());
    const std::string err = contents(d.err.get());
    EXPECT_EQ(err.rfind("kirimasim: warning: the dump file \"" + bad + "\" cannot be opened (", 0),
              0U)
        << err;
    EXPECT_NE(err.find("); nothing is dumped\n"), std::string::npos) << err;
    EXPECT_FALSE(d.ctx.dump().busy());
}

// --wave-scope: * any characters, ? one (WAVEFORM.md 5).
TEST(Dump, Patterns) {
    EXPECT_TRUE(globMatch("top", "top"));
    EXPECT_FALSE(globMatch("top", "top.a"));
    EXPECT_TRUE(globMatch("top.*", "top.a.b"));
    EXPECT_TRUE(globMatch("*", ""));
    EXPECT_TRUE(globMatch("t?p", "tap"));
    EXPECT_FALSE(globMatch("t?p", "tp"));
    EXPECT_TRUE(globMatch("*.g[?]*", "top.g[1].c"));
    EXPECT_TRUE(globMatch("a*b*c", "axxbyybzc"));
    EXPECT_FALSE(globMatch("a*b*c", "axxcyyb"));
    EXPECT_FALSE(globMatch("", "a"));
}

}  // namespace
