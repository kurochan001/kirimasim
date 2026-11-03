// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/front/front.h"

namespace {
struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-drivers-XXXXXX").string();
        if (!mkdtemp(pattern.data())) return;
        directory = pattern;
        const auto path = directory / "design.sv";
        std::ofstream stream(path);
        stream << text;
        stream.close();
        if (stream) file = std::filesystem::relative(std::filesystem::canonical(path)).string();
    }
    ~Source() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
};
constexpr std::string_view input =
    "module top; bit clk,q; initial q=0; always_ff @(posedge clk) q<=1; endmodule\n";

TEST(DriverChecks, RelaxedParseAndStrictExecutableAreSeparateApis) {
    const Source source(input);
    ASSERT_FALSE(source.file.empty());
    const std::vector<std::string> args{"--compat=all", source.file};
    EXPECT_TRUE(kirimasim::front::parseAndElaborate(args).success);
    const auto compiled = kirimasim::front::compile(args);
    EXPECT_FALSE(compiled.success);
    EXPECT_TRUE(compiled.unsupported);
    EXPECT_GT(compiled.numErrors, 0);
    EXPECT_TRUE(compiled.verifyErrors.empty());
    EXPECT_NE(compiled.diagnostics.find("unsupported: conflicting source drivers"),
              std::string::npos);
    EXPECT_NE(compiled.diagnostics.find(": note:"), std::string::npos);
}
TEST(DriverChecks, SuppressingAWarningDoesNotCreateInvalidExecutableSir) {
    const Source source(input);
    ASSERT_FALSE(source.file.empty());
    const std::vector<std::string> args{"-Wno-multiple-always-assigns", source.file};
    EXPECT_TRUE(kirimasim::front::parseAndElaborate(args).success);
    const auto compiled = kirimasim::front::compile(args);
    EXPECT_FALSE(compiled.success);
    EXPECT_TRUE(compiled.verifyErrors.empty());
    EXPECT_GT(compiled.numErrors, 0);
    EXPECT_NE(compiled.diagnostics.find("unsupported: conflicting source drivers"),
              std::string::npos);
}
TEST(DriverChecks, NonoverlappingStaticBitRangesRemainExecutable) {
    const Source source("module top; bit clk; logic[128:0] q; initial q[128]=0; "
                        "always_ff @(posedge clk) q[0]<=1; endmodule\n");
    ASSERT_FALSE(source.file.empty());
    const std::vector<std::string> args{"--compat=all", source.file};
    const auto compiled = kirimasim::front::compile(args);
    EXPECT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    EXPECT_EQ(compiled.numErrors, 0);
}
TEST(DriverChecks, RepeatedCompilationDoesNotRetainCompatibilityState) {
    const Source source(input);
    ASSERT_FALSE(source.file.empty());
    for (const auto* flag : {"--compat=all", "--compat=default", "--compat=vcs", "--compat=all"}) {
        const std::vector<std::string> args{flag, source.file};
        const auto compiled = kirimasim::front::compile(args);
        EXPECT_FALSE(compiled.success);
        EXPECT_GT(compiled.numErrors, 0);
        EXPECT_TRUE(compiled.verifyErrors.empty());
    }
}

TEST(DriverChecks, DynamicArrayCompatibilityCannotOmitExecutableWriterChecks) {
    for (const auto* declaration : {"int values[];", "int values[$];", "int values[int];"}) {
        for (const auto* writers :
             {"always_ff @(posedge clk) values[0]=1; always_ff @(negedge clk) values[0]=2;",
              "always_comb values[0]=1; initial values[0]=2;",
              "always_latch if(clk) values[0]=1; initial values[0]=2;"}) {
            const Source source(std::string("module top;bit clk;") + declaration + writers +
                                "endmodule\n");
            ASSERT_FALSE(source.file.empty());
            for (const auto* mode : {"--compat=vcs", "--compat=all"}) {
                SCOPED_TRACE(::testing::Message() << declaration << ' ' << writers << ' ' << mode);
                const std::vector<std::string> args{mode, source.file};
                EXPECT_TRUE(kirimasim::front::parseAndElaborate(args).success);
                const auto compiled = kirimasim::front::compile(args);
                EXPECT_FALSE(compiled.success);
                EXPECT_TRUE(compiled.unsupported);
                EXPECT_TRUE(compiled.verifyErrors.empty());
                EXPECT_NE(compiled.diagnostics.find("unsupported: conflicting source drivers"),
                          std::string::npos);
            }
        }
        const Source single(std::string("module top;bit clk;") + declaration +
                            "always_ff @(posedge clk) values[0]=1;endmodule\n");
        const std::vector<std::string> args{"--compat=all", single.file};
        const auto compiled = kirimasim::front::compile(args);
        if (std::string_view(declaration) == "int values[int];") {
            EXPECT_FALSE(compiled.success);
            EXPECT_TRUE(compiled.unsupported);
            EXPECT_TRUE(compiled.verifyErrors.empty());
            EXPECT_NE(compiled.diagnostics.find("unsupported: variable of type 'int$[int]'"),
                      std::string::npos);
        } else {
            EXPECT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        }
    }
}
}  // namespace
