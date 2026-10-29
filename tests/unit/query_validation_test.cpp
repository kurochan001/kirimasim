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
        auto pattern = (std::filesystem::temp_directory_path() / "kirimasim-query-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) return;
        directory = pattern;
        const auto path = directory / "design.sv";
        std::ofstream stream(path);
        stream << text;
        stream.close();
        if (stream) file = std::filesystem::relative(std::filesystem::canonical(path)).string();
    }
    ~Source() {
        if (directory.empty()) return;
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};

constexpr std::string_view diagnostic =
    ": error: $bits cannot use a function call with a dynamically sized return type "
    "(IEEE 1800-2023 20.6.2)\n";

TEST(QueryValidation, DynamicFunctionSizeIsALanguageErrorInBothApis) {
    const Source source(R"sv(module top;
  typedef logic [8:0] samples_t[];
  function automatic samples_t acquire();
    samples_t items = new[5];
    return items;
  endfunction
  localparam int width = $bits(acquire());
endmodule
)sv");
    ASSERT_FALSE(source.file.empty());
    const std::vector<std::string> args{source.file};
    const auto parsed = kirimasim::front::parseAndElaborate(args);
    EXPECT_FALSE(parsed.success);
    EXPECT_EQ(parsed.numErrors, 1);
    EXPECT_EQ(parsed.diagnostics, source.file + ":7:26" + std::string(diagnostic));
    const auto compiled = kirimasim::front::compile(args);
    EXPECT_FALSE(compiled.success);
    EXPECT_FALSE(compiled.unsupported);
    EXPECT_EQ(compiled.numErrors, 1);
    EXPECT_TRUE(compiled.verifyErrors.empty());
    EXPECT_EQ(compiled.diagnostics, parsed.diagnostics);
}

TEST(QueryValidation, FoldedInitializersAndUnusedBodiesAreCheckedOncePerSourceSite) {
    const Source source(R"sv(module item;
  typedef int words_t[];
  function automatic words_t words();
    words_t value = new[3];
    return value;
  endfunction
  localparam int width = $bits(words());
  function automatic int never_called();
    return $bits(words());
  endfunction
endmodule
module top;
  item first();
  item second();
endmodule
)sv");
    ASSERT_FALSE(source.file.empty());
    const std::vector<std::string> args{source.file};
    const auto parsed = kirimasim::front::parseAndElaborate(args);
    EXPECT_FALSE(parsed.success);
    EXPECT_EQ(parsed.numErrors, 2);
    EXPECT_EQ(parsed.diagnostics, source.file + ":7:26" + std::string(diagnostic) + source.file +
                                      ":9:12" + std::string(diagnostic));
    const auto compiled = kirimasim::front::compile(args);
    EXPECT_FALSE(compiled.success);
    EXPECT_FALSE(compiled.unsupported);
    EXPECT_EQ(compiled.numErrors, 2);
    EXPECT_EQ(compiled.diagnostics, parsed.diagnostics);
}

TEST(QueryValidation, FixedReturnsAndDynamicVariablesRemainLegalToParse) {
    const Source source(R"sv(module top;
  typedef byte bytes_t[];
  bytes_t values;
  function automatic int fixed_width(input bytes_t data);
    return data.size();
  endfunction
  function automatic chandle opaque();
    return null;
  endfunction
  function automatic int handle_width(input chandle handle);
    return handle == null;
  endfunction
  localparam int first = $bits(fixed_width(values));
  localparam int second = $bits(handle_width(opaque()));
  initial $display("%0d", $bits(values));
endmodule
)sv");
    ASSERT_FALSE(source.file.empty());
    const auto parsed = kirimasim::front::parseAndElaborate(std::vector<std::string>{source.file});
    EXPECT_TRUE(parsed.success) << parsed.diagnostics;
    EXPECT_EQ(parsed.numErrors, 0);
}

}  // namespace
