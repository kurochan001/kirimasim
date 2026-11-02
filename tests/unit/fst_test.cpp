// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original FST acceptance traces; libfst only decodes the observed file.
#include <algorithm>
#include <array>
#include <bit>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <fstapi.h>
#include <gtest/gtest.h>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/dump.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
struct Temporary {
    std::filesystem::path directory;
    Temporary() {
        auto pattern = (std::filesystem::temp_directory_path() / "kirimasim-fst-XXXXXX").string();
        if (mkdtemp(pattern.data())) directory = pattern;
    }
    ~Temporary() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
};
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() {
        if (!file) return {};
        std::fflush(file);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string result;
        std::array<char, 512> buffer{};
        while (const auto n = std::fread(buffer.data(), 1, buffer.size(), file))
            result.append(buffer.data(), n);
        return result;
    }
};
struct Variable {
    std::string name;
    fstHandle handle = 0;
    unsigned width = 0;
    unsigned type = 0;
    bool alias = false;
};
struct Change {
    std::uint64_t time = 0;
    fstHandle handle = 0;
    std::string bits;
    std::uint64_t real = 0;
};
struct Decoded {
    bool good = false;
    int precision = 0;
    std::uint64_t aliases = 0;
    std::string date, version;
    std::vector<Variable> variables;
    std::vector<Change> changes;
    std::vector<std::pair<std::string, unsigned>> scopes;
    std::vector<std::pair<std::uint64_t, bool>> activity;
    std::vector<std::string> comments;
    explicit Decoded(const std::filesystem::path& path) {
        auto* reader = fstReaderOpen(path.c_str());
        if (!reader) return;
        // FST returns a signed decimal exponent, not a character code.
        // NOLINTNEXTLINE(bugprone-signed-char-misuse)
        precision = static_cast<int>(fstReaderGetTimescale(reader));
        aliases = fstReaderGetAliasCount(reader);
        if (const auto* s = fstReaderGetDateString(reader)) date = s;
        if (const auto* s = fstReaderGetVersionString(reader)) version = s;
        std::vector<std::string> parents;
        const auto full = [&] {
            std::string result;
            for (const auto& part : parents) {
                if (!result.empty()) result += '.';
                result += part;
            }
            return result;
        };
        while (auto* row = fstReaderIterateHier(reader)) {
            if (row->htyp == FST_HT_SCOPE) {
                parents.emplace_back(row->u.scope.name);
                scopes.emplace_back(full(), row->u.scope.typ);
            } else if (row->htyp == FST_HT_UPSCOPE) {
                if (!parents.empty()) parents.pop_back();
            } else if (row->htyp == FST_HT_VAR) {
                std::string name = row->u.var.name;
                if (const auto space = name.find(' '); space != std::string::npos)
                    name.resize(space);
                auto qualified = full();
                if (!qualified.empty()) qualified += '.';
                qualified += name;
                variables.push_back({std::move(qualified), row->u.var.handle, row->u.var.length,
                                     row->u.var.typ, row->u.var.is_alias != 0});
            } else if (row->htyp == FST_HT_ATTRBEGIN && row->u.attr.typ == FST_AT_MISC &&
                       row->u.attr.subtype == FST_MT_COMMENT && row->u.attr.name) {
                comments.emplace_back(row->u.attr.name);
            }
        }
        for (std::uint32_t i = 0; i < fstReaderGetNumberDumpActivityChanges(reader); ++i)
            activity.emplace_back(fstReaderGetDumpActivityChangeTime(reader, i),
                                  fstReaderGetDumpActivityChangeValue(reader, i) != 0);
        fstReaderSetFacProcessMaskAll(reader);
        fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 1);
        good = fstReaderIterBlocks(reader, &changed, this, nullptr) != 0;
        fstReaderClose(reader);
    }
    static void changed(void* pointer, std::uint64_t time, fstHandle handle,
                        const unsigned char* value) {
        auto& decoded = *static_cast<Decoded*>(pointer);
        const auto found = std::find_if(decoded.variables.begin(), decoded.variables.end(),
                                        [&](const auto& v) { return v.handle == handle; });
        if (found == decoded.variables.end()) {
            ADD_FAILURE() << "unknown FST handle";
            return;
        }
        Change change{time, handle, {}, 0};
        if (found->type == FST_VT_VCD_REAL || found->type == FST_VT_VCD_REALTIME)
            std::memcpy(&change.real, value, sizeof change.real);
        else
            change.bits = reinterpret_cast<const char*>(value);
        decoded.changes.push_back(std::move(change));
    }
    Variable variable(std::string_view suffix) const {
        const auto found = std::find_if(variables.begin(), variables.end(),
                                        [&](const auto& v) { return v.name.ends_with(suffix); });
        if (found == variables.end()) {
            ADD_FAILURE() << "missing " << suffix;
            return {};
        }
        return *found;
    }
    std::vector<std::pair<std::uint64_t, std::string>> trace(fstHandle handle) const {
        std::vector<std::pair<std::uint64_t, std::string>> result;
        for (const auto& c : changes)
            if (c.handle == handle) result.emplace_back(c.time, c.bits);
        return result;
    }
};
struct Design {
    Temporary temporary;
    Capture output, errors;
    explicit Design(std::string_view source) {
        EXPECT_FALSE(temporary.directory.empty());
        std::ofstream(temporary.directory / "design.sv") << source;
    }
    std::filesystem::path wave() const { return temporary.directory / "wave.fst"; }
    kirimasim::vm::RunResult run(kirimasim::vm::RunOptions options = {}, k::Limits limits = {},
                                 const volatile std::sig_atomic_t* interrupt = nullptr) {
        const std::vector<std::string> inputs{(temporary.directory / "design.sv").string()};
        auto compiled = kirimasim::front::compile(inputs, {});
        EXPECT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        if (!compiled.success) return {k::EndReason::Internal, compiled.diagnostics};
        k::Context context({output.file, errors.file});
        context.scheduler().setLimits(limits);
        context.scheduler().setInterruptFlag(interrupt);
        if (options.waveFile.empty()) options.waveFile = wave().string();
        options.date = "2026-11-03 00:00:00 UTC";
        options.version = "kirimasim fst test";
        return kirimasim::vm::run(compiled.design, context, options);
    }
};

struct Matrix {
    unsigned width;
    bool four;
    bool glitches;
    unsigned home;
};
std::vector<Matrix> matrix() {
    std::vector<Matrix> result;
    for (unsigned width : {1U, 32U, 65U, 129U, 257U})
        for (bool four : {false, true})
            for (bool glitches : {false, true})
                for (unsigned home = 0; home < 6; ++home)
                    result.push_back({width, four, glitches, home});
    return result;
}
class FstMatrix : public testing::TestWithParam<Matrix> {};
TEST_P(FstMatrix, FullWidthValuesShareTheSourceContextRules) {
    const auto p = GetParam();
    const auto declaration = std::string(p.four ? "logic" : "bit") + "[" +
                             std::to_string(p.width - 1) + ":0] value='0;\n";
    const auto body = "#1 value='1; #1 value='x; #1 value='z; #1 value='0; value[" +
                      std::to_string(p.width - 1) + "]=1; #1;";
    std::string source = "`timescale 1ns/1ps\n";
    switch (p.home) {
        case 0:
            source += "module top; " + declaration + "initial begin " + body + " end endmodule";
            break;
        case 1:
            source += "interface box; " + declaration + "initial begin " + body +
                      " end endinterface module top; box u(); endmodule";
            break;
        case 2:
            source += "program box; " + declaration + "initial begin " + body +
                      " end endprogram module top; box u(); endmodule";
            break;
        case 3:
            source += "module top; if(1) begin:g " + declaration + "initial begin " + body +
                      " end end endmodule";
            break;
        case 4:
            source += "module top; initial begin:region " + declaration + body + " end endmodule";
            break;
        case 5:
            source +=
                "module top; task run; " + declaration + body + " endtask initial run(); endmodule";
            break;
        default: FAIL() << "invalid source context";
    }
    Design design(source);
    const auto run = design.run({.waveGlitches = p.glitches});
    ASSERT_NE(run.reason, k::EndReason::Internal) << run.error << design.errors.text();
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good) << design.errors.text();
    EXPECT_EQ(decoded.precision, -12);
    EXPECT_EQ(decoded.date, "2026-11-03 00:00:00 UTC");
    EXPECT_EQ(decoded.version, "kirimasim fst test");
    const auto variable = decoded.variable(".value");
    ASSERT_EQ(variable.width, p.width);
    const auto zero = std::string(p.width, '0');
    std::vector<std::pair<std::uint64_t, std::string>> expected{{0, zero},
                                                                {1000, std::string(p.width, '1')}};
    if (p.four) {
        expected.emplace_back(2000, std::string(p.width, 'x'));
        expected.emplace_back(3000, std::string(p.width, 'z'));
        if (p.glitches) expected.emplace_back(4000, zero);
    } else {
        expected.emplace_back(2000, zero);
    }
    expected.emplace_back(4000, "1" + std::string(p.width - 1, '0'));
    EXPECT_EQ(decoded.trace(variable.handle), expected);
}
INSTANTIATE_TEST_SUITE_P(WidthsDomainsModesAndScopes, FstMatrix, testing::ValuesIn(matrix()),
                         [](const auto& info) {
                             const auto p = info.param;
                             return "W" + std::to_string(p.width) + (p.four ? "Four" : "Two") +
                                    (p.glitches ? "Glitch" : "Slot") + "Home" +
                                    std::to_string(p.home);
                         });

TEST(Fst, EventsDoNotInventAnInitialTriggerAndKeepRepeatedSameTickTriggers) {
    for (bool glitches : {false, true}) {
        Design design("`timescale 1ns/1ps\nmodule top; event event_value; initial begin #1 "
                      "->event_value; ->event_value; #1 ->event_value; end endmodule");
        ASSERT_NE(design.run({.waveGlitches = glitches}).reason, k::EndReason::Internal);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const auto event = decoded.variable(".event_value");
        EXPECT_EQ(event.type, FST_VT_VCD_EVENT);
        std::vector<std::pair<std::uint64_t, std::string>> expected{{1000, "1"}};
        if (glitches) expected.emplace_back(1000, "1");
        expected.emplace_back(2000, "1");
        EXPECT_EQ(decoded.trace(event.handle), expected);
    }
}
TEST(Fst, RealValuesPreserveNativeBitsIncludingSignedZero) {
    for (const std::string type : {"real", "realtime", "shortreal"}) {
        Design design(
            "`timescale 1ns/1ps\nmodule top; " + type +
            " value=0.0; initial begin #1 value=-0.0; #1 value=0.1; #1 value=1.5; end endmodule");
        ASSERT_NE(design.run().reason, k::EndReason::Internal);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const auto variable = decoded.variable(".value");
        std::vector<std::pair<std::uint64_t, std::uint64_t>> observed;
        for (const auto& c : decoded.changes)
            if (c.handle == variable.handle) observed.emplace_back(c.time, c.real);
        const double tenth = type == "shortreal" ? static_cast<double>(0.1F) : 0.1;
        const std::vector<std::pair<std::uint64_t, std::uint64_t>> expected{
            {0, std::bit_cast<std::uint64_t>(0.0)},
            {1000, std::bit_cast<std::uint64_t>(-0.0)},
            {2000, std::bit_cast<std::uint64_t>(tenth)},
            {3000, std::bit_cast<std::uint64_t>(1.5)}};
        EXPECT_EQ(observed, expected) << type;
    }
}
TEST(Fst, DumpControlsUseSlotEndSnapshotsAndActivityMetadata) {
    Design design("`timescale 1ns/1ps\nmodule top; logic[3:0] value=0; initial begin #1 value=1; "
                  "$dumpoff; #1 value=2; #1 $dumpon; #1 value=3; $dumpall; end endmodule");
    ASSERT_NE(design.run().reason, k::EndReason::Internal);
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    const auto variable = decoded.variable(".value");
    const std::vector<std::pair<std::uint64_t, std::string>> expected{
        {0, "0000"}, {1000, "0001"}, {1000, "xxxx"}, {3000, "0010"}, {4000, "0011"}};
    auto observed = decoded.trace(variable.handle);
    observed.erase(std::unique(observed.begin(), observed.end()), observed.end());
    EXPECT_EQ(observed, expected);
    const std::vector<std::pair<std::uint64_t, bool>> activity{{1000, false}, {3000, true}};
    EXPECT_EQ(decoded.activity, activity);
}
TEST(Fst, CollapsedPortsUseOnePhysicalAliasHandle) {
    Design design("`timescale 1ns/1ps\nmodule leaf(input wire[3:0] port); endmodule module top; "
                  "reg[3:0] source=0; wire[3:0] line=source; leaf u(line); initial begin #1 "
                  "source=9; end endmodule");
    ASSERT_NE(design.run().reason, k::EndReason::Internal);
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    EXPECT_EQ(decoded.variable("top.line").handle, decoded.variable("top.u.port").handle);
    EXPECT_GT(decoded.aliases, 0U);
}
TEST(Fst, FiltersIncludeDescendantsAndKeepAnEmptySelectionReadable) {
    const char* source = "module child; int value=3; endmodule module top; int value=7; child "
                         "u(); initial #1; endmodule";
    for (const std::string pattern : {"top.u", "nothing"}) {
        Design design(source);
        ASSERT_NE(design.run({.waveScopes = {pattern}}).reason, k::EndReason::Internal);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        if (pattern == "nothing")
            EXPECT_TRUE(decoded.variables.empty());
        else {
            ASSERT_EQ(decoded.variables.size(), 1U);
            EXPECT_EQ(decoded.variables[0].name, "top.u.value");
        }
    }
}
TEST(Fst, FileCreationFailureWarnsAndSimulationContinues) {
    Design design("module top; initial $display(\"done\"); endmodule");
    const auto path = design.temporary.directory / "absent" / "wave.fst";
    EXPECT_NE(design.run({.waveFile = path.string()}).reason, k::EndReason::Internal);
    EXPECT_EQ(design.output.text(), "done\n");
    EXPECT_NE(design.errors.text().find("cannot be opened ("), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(path));
}
TEST(Fst, LimitsCutOffAtCompressedBlocksAndKeepAReadableFinalizedFile) {
    Design design(
        "`timescale 1ns/1ps\nmodule top; logic[128:0] value=0; initial begin $dumplimit(32); "
        "for(int i=0;i<200;i++) begin #1 value=i; $dumpflush; end end endmodule");
    ASSERT_NE(design.run().reason, k::EndReason::Internal);
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    ASSERT_FALSE(decoded.changes.empty());
    EXPECT_LT(decoded.changes.back().time, 200000U);
    EXPECT_GT(std::filesystem::file_size(design.wave()), 32U);
    EXPECT_TRUE(std::any_of(decoded.comments.begin(), decoded.comments.end(), [](const auto& s) {
        return s.find("the dump limit of 32 bytes was reached") != std::string::npos;
    }));
}
TEST(Fst, FinishAndZeroTimeLimitsFinalizeTheCutShortInitialSlot) {
    for (bool limit : {false, true}) {
        Design design(std::string("module top; logic[3:0] value; initial begin value=7; ") +
                      (limit ? "forever begin end" : "$finish;") + " end endmodule");
        auto result = design.run({.maxSteps = limit ? 40U : 0U});
        EXPECT_EQ(result.reason, limit ? k::EndReason::ZeroTimeLimit : k::EndReason::Finish);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const std::vector<std::pair<std::uint64_t, std::string>> expected{{0, "0111"}};
        EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
    }
}

TEST(Fst, DeclaredKindsRangesAndPackedStructuresKeepTheirMetadata) {
    Design design(R"(`timescale 1ns/1ps
module top;
 reg [3:0] reg_value=0;
 integer integer_value=3;
 time time_value=4;
 real real_value=1.5;
 realtime realtime_value=2.5;
 event event_value;
 wire wire_value=1'b1;
 tri tri_value=1'bz;
 wand wand_value=1'b0;
 triand triand_value=1'b1;
 wor wor_value=1'b1;
 trior trior_value=1'b0;
 tri0 tri0_value;
 tri1 tri1_value;
 supply0 supply0_value;
 supply1 supply1_value;
 struct packed {logic[3:0] word;bit flag;} packed_value='{4'b10xz,1'b1};
 initial begin #1 packed_value.word='z; end
endmodule
)");
    ASSERT_NE(design.run().reason, k::EndReason::Internal);
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    const std::vector<std::pair<std::string, unsigned>> kinds{
        {"reg", FST_VT_VCD_REG},           {"integer", FST_VT_VCD_INTEGER},
        {"time", FST_VT_VCD_TIME},         {"real", FST_VT_VCD_REAL},
        {"realtime", FST_VT_VCD_REALTIME}, {"event", FST_VT_VCD_EVENT},
        {"wire", FST_VT_VCD_WIRE},         {"tri", FST_VT_VCD_TRI},
        {"wand", FST_VT_VCD_WAND},         {"triand", FST_VT_VCD_TRIAND},
        {"wor", FST_VT_VCD_WOR},           {"trior", FST_VT_VCD_TRIOR},
        {"tri0", FST_VT_VCD_TRI0},         {"tri1", FST_VT_VCD_TRI1},
        {"supply0", FST_VT_VCD_SUPPLY0},   {"supply1", FST_VT_VCD_SUPPLY1}};
    for (const auto& [name, kind] : kinds)
        EXPECT_EQ(decoded.variable("." + name + "_value").type, kind) << name;
    EXPECT_EQ(decoded.variable(".time_value").width, 64U);
    const auto packed = decoded.variable(".packed_value");
    EXPECT_EQ(packed.width, 5U);
    const std::vector<std::pair<std::uint64_t, std::string>> expected{{0, "10xz1"},
                                                                      {1000, "zzzz1"}};
    EXPECT_EQ(decoded.trace(packed.handle), expected);
}
TEST(Fst, DynamicAutomaticAndParameterStorageStayOutOfTheDump) {
    Design design(R"(module top;
 parameter int N=4;
 int array_value[2];
 string text_value="kept";
 class Box; int field_value; endclass
 Box handle_value;
 int public_value=7;
 task automatic run; int scratch=13; #1; endtask
 initial run();
 endmodule)");
    ASSERT_NE(design.run().reason, k::EndReason::Internal) << design.errors.text();
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    ASSERT_EQ(decoded.variables.size(), 1U);
    EXPECT_EQ(decoded.variables[0].name, "top.public_value");
}
TEST(Fst, StrengthResolutionAndRegisteredNetForceUseThePublicValue) {
    Design design(R"(`timescale 1ns/1ps
module top;
 reg a=1,b=0;
 tri line;
 assign (weak1,weak0) line=a;
 assign (strong1,strong0) line=b;
 initial begin #1 force line=1'bz; #1 release line; #1 b=1'bz; end
endmodule)");
    ASSERT_NE(design.run().reason, k::EndReason::Internal) << design.errors.text();
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    const std::vector<std::pair<std::uint64_t, std::string>> expected{
        {0, "0"}, {1000, "z"}, {2000, "0"}, {3000, "1"}};
    EXPECT_EQ(decoded.trace(decoded.variable(".line").handle), expected);
}
TEST(Fst, SpecifyDeadlinesContinueUnderForceAndPrivateCarriersAreAbsent) {
    Design design(R"(`timescale 1ns/1ps
module delayed(input wire a,output wire y);
 assign y=a;
 specify (a=>y)=2; endspecify
endmodule
module top;
 reg a=0;wire y;delayed u(a,y);
 initial begin #1 a=1; #1 force y=0; #3 release y; #1; end
endmodule)");
    ASSERT_NE(design.run().reason, k::EndReason::Internal) << design.errors.text();
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    const std::vector<std::pair<std::uint64_t, std::string>> expected{
        {0, "x"}, {2000, "0"}, {5000, "1"}};
    EXPECT_EQ(decoded.trace(decoded.variable("top.y").handle), expected);
    const std::vector<std::string> allowed{"top.a", "top.y", "top.u.a", "top.u.y"};
    for (const auto& variable : decoded.variables)
        EXPECT_NE(std::find(allowed.begin(), allowed.end(), variable.name), allowed.end())
            << variable.name;
}
TEST(Fst, FinalProceduresPublishTheirLastValueBeforeClose) {
    Design design("module top;int value=0; initial #1 $finish; final value=7; endmodule");
    ASSERT_EQ(design.run().reason, k::EndReason::Finish);
    Decoded decoded(design.wave());
    ASSERT_TRUE(decoded.good);
    const std::vector<std::pair<std::uint64_t, std::string>> expected{
        {0, "00000000000000000000000000000000"}, {1, "00000000000000000000000000000111"}};
    EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
}
TEST(Fst, ExistingRejectedCliInputRunsNativelyBeforeSeparateMigrationReview) {
    Temporary temporary;
    ASSERT_FALSE(temporary.directory.empty());
    const auto source =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "cli" / "display_finish.sv";
    const std::vector<std::string> inputs{source.string()};
    auto compiled = kirimasim::front::compile(inputs, {});
    ASSERT_TRUE(compiled.success) << compiled.diagnostics;
    Capture output, errors;
    k::Context context({output.file, errors.file});
    const auto wave = temporary.directory / "w.fst";
    const auto result = kirimasim::vm::run(compiled.design, context, {.waveFile = wave.string()});
    EXPECT_EQ(result.reason, k::EndReason::Finish) << result.error;
    EXPECT_EQ(output.text(), "hello\na");
    Decoded decoded(wave);
    ASSERT_TRUE(decoded.good);
    EXPECT_TRUE(decoded.variables.empty());
}
TEST(Fst, ParallelIndependentContextsKeepTheirWritersAndValuesSeparate) {
    Temporary temporary;
    ASSERT_FALSE(temporary.directory.empty());
    const auto source = temporary.directory / "design.sv";
    std::ofstream(source) << "`timescale 1ns/1ps\nmodule top;int value; initial begin "
                             "value=$test$plusargs(\"one\")?1:2;#1 value+=1;end endmodule";
    const std::vector<std::string> inputs{source.string()};
    auto compiled = kirimasim::front::compile(inputs, {});
    ASSERT_TRUE(compiled.success) << compiled.diagnostics;
    std::array<kirimasim::vm::RunResult, 4> results;
    std::array<std::thread, 4> threads;
    for (unsigned i = 0; i < threads.size(); ++i)
        threads[i] = std::thread([&, i] {
            Capture output, errors;
            k::Context context({output.file, errors.file});
            kirimasim::vm::RunOptions options;
            options.waveFile =
                (temporary.directory / ("wave" + std::to_string(i) + ".fst")).string();
            if (i % 2) options.plusargs = {"one"};
            results[i] = kirimasim::vm::run(compiled.design, context, options);
        });
    for (auto& thread : threads)
        thread.join();
    for (unsigned i = 0; i < threads.size(); ++i) {
        ASSERT_EQ(results[i].reason, k::EndReason::Quiescent) << results[i].error;
        Decoded decoded(temporary.directory / ("wave" + std::to_string(i) + ".fst"));
        ASSERT_TRUE(decoded.good);
        const std::vector<std::pair<std::uint64_t, std::string>> expected =
            i % 2 ? std::vector<std::pair<std::uint64_t,
                                          std::string>>{{0, "00000000000000000000000000000001"},
                                                        {1000, "00000000000000000000000000000010"}}
                  : std::vector<std::pair<std::uint64_t, std::string>>{
                        {0, "00000000000000000000000000000010"},
                        {1000, "00000000000000000000000000000011"}};
        EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
    }
}

TEST(Fst, TimeResourceFatalAndInterruptPathsFinalizeTheirWaveforms) {
    {
        Design design("`timescale 1ns/1ps\nmodule top;bit[3:0]value=1;initial begin #1 value=2;#1 "
                      "value=3;end endmodule");
        k::Limits limits;
        limits.maxTime = 1500;
        EXPECT_EQ(design.run({}, limits).reason, k::EndReason::TimeLimit);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const std::vector<std::pair<std::uint64_t, std::string>> expected{{0, "0001"},
                                                                          {1000, "0010"}};
        EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
    }
    {
        Design design("module top;bit[3:0]value=7;initial fork #1; #1; join endmodule");
        k::Limits limits;
        limits.maxProcs = 1;
        EXPECT_EQ(design.run({}, limits).reason, k::EndReason::ResourceLimit);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const std::vector<std::pair<std::uint64_t, std::string>> expected{{0, "0111"}};
        EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
    }
    {
        Design design("module top;bit[3:0]value=7;initial $fatal(1,\"stop\");endmodule");
        EXPECT_EQ(design.run().reason, k::EndReason::Fatal);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const std::vector<std::pair<std::uint64_t, std::string>> expected{{0, "0111"}};
        EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
    }
    {
        Design design("module top;bit[3:0]value;initial forever #1 value+=1;endmodule");
        const volatile std::sig_atomic_t flag = 1;
        EXPECT_EQ(design.run({}, {}, &flag).reason, k::EndReason::Interrupted);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        const std::vector<std::pair<std::uint64_t, std::string>> expected{{0, "0000"}};
        EXPECT_EQ(decoded.trace(decoded.variable(".value").handle), expected);
    }
}
TEST(Fst, FstAndVcdUseTheSameControlAndGlitchScheduling) {
    for (bool glitches : {false, true}) {
        Design design("`timescale 1ns/1ps\nmodule top;logic[3:0]value=0; initial begin #1 "
                      "value=1;value=2;value<=3;#1 $dumpoff;#1 value=4;#1 $dumpon;#1 "
                      "value=5;$dumpall;end endmodule");
        ASSERT_EQ(design.run({.waveGlitches = glitches}).reason, k::EndReason::Quiescent);
        Decoded decoded(design.wave());
        ASSERT_TRUE(decoded.good);
        auto fst = decoded.trace(decoded.variable(".value").handle);
        const auto vcd = design.temporary.directory / "wave.vcd";
        ASSERT_EQ(design.run({.waveFile = vcd.string(), .waveGlitches = glitches}).reason,
                  k::EndReason::Quiescent);
        std::ifstream file(vcd);
        std::string line, identifier;
        std::uint64_t time = 0;
        std::vector<std::pair<std::uint64_t, std::string>> text;
        while (std::getline(file, line)) {
            if (line.starts_with("$var ")) {
                std::istringstream row(line);
                std::string marker, type, name;
                unsigned width = 0;
                row >> marker >> type >> width >> identifier >> name;
                ASSERT_EQ(name, "value");
                ASSERT_EQ(width, 4U);
            } else if (line.starts_with('#'))
                time = std::stoull(line.substr(1));
            else if (line.starts_with('b')) {
                const auto space = line.find(' ');
                ASSERT_NE(space, std::string::npos);
                if (line.substr(space + 1) != identifier) continue;
                auto bits = line.substr(1, space - 1);
                const char fill = bits[0] == 'x' || bits[0] == 'z' ? bits[0] : '0';
                text.emplace_back(time, std::string(4 - bits.size(), fill) + bits);
            }
        }
        const auto canonical = [](auto rows) {
            decltype(rows) result;
            for (auto& row : rows)
                if (result.empty() || result.back().second != row.second)
                    result.push_back(std::move(row));
            return result;
        };
        EXPECT_EQ(canonical(fst), canonical(text));
    }
}
TEST(Fst, ASourceDumpfileSuffixAloneDoesNotSwitchAwayFromVcd) {
    Temporary temporary;
    ASSERT_FALSE(temporary.directory.empty());
    const auto wave = temporary.directory / "source.fst";
    const auto source = temporary.directory / "design.sv";
    std::ofstream(source) << "module top;bit value=1;initial begin $dumpfile(\"" << wave.string()
                          << "\");$dumpvars;#1;end endmodule";
    const std::vector<std::string> inputs{source.string()};
    auto compiled = kirimasim::front::compile(inputs, {});
    ASSERT_TRUE(compiled.success) << compiled.diagnostics;
    Capture output, errors;
    k::Context context({output.file, errors.file});
    ASSERT_EQ(kirimasim::vm::run(compiled.design, context).reason, k::EndReason::Quiescent);
    std::ifstream file(wave);
    std::string first;
    std::getline(file, first);
    EXPECT_EQ(first, "$date");
}
}  // namespace
