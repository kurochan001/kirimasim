// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kirimasim/front/front.h"

namespace {
namespace f = kirimasim::front;

struct Source {
    std::filesystem::path directory;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-inside-front-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) return;
        directory = pattern;
        std::ofstream stream(directory / "design.sv");
        stream << text;
    }
    ~Source() {
        if (!directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
    }
};

f::CompileResult compile(std::string_view body) {
    const std::string text = "module top;int x;int y;logic r;"
                             "function automatic int f();y=y+1;return y;endfunction " +
                             std::string(body) + " endmodule\n";
    Source source(text);
    EXPECT_FALSE(source.directory.empty());
    if (source.directory.empty()) return {};
    return f::compile(std::vector<std::string>{(source.directory / "design.sv").string()});
}

// The standard leaves the order of the members of the operator open
// (11.4.13), so a member with side effects stays out, whatever its form.
TEST(InsideFront, OperatorMembersWithSideEffectsStayUnsupported) {
    for (const auto* body : {
             "initial r=x inside {f()};",
             "initial r=x inside {y++};",
             "initial r=x inside {1,f()};",
             "initial r=x inside {[f():5]};",
             "initial r=x inside {[1:f()]};",
             "initial r=x inside {[f() +/- 1]};",
             "initial r=x inside {[1 +%- f()]};",
             "initial begin if (x inside {f()}) r=1; end",
         }) {
        const auto compiled = compile(body);
        EXPECT_FALSE(compiled.success) << body;
        EXPECT_TRUE(compiled.unsupported) << body << compiled.diagnostics;
        EXPECT_NE(compiled.diagnostics.find("set member with side effects"), std::string::npos)
            << body << compiled.diagnostics;
    }
}

// A case inside item has the order of its case (12.5.4).
TEST(InsideFront, CaseInsideItemsWithSideEffectsCompile) {
    for (const auto* body : {
             "initial case (x) inside f(): r=1; default: r=0; endcase",
             "initial case (x) inside [f():5]: r=1; default: r=0; endcase",
             "initial priority case (x) inside f(),y++: r=1; default: r=0; endcase",
             "initial unique case (x) inside f(): r=1; default: r=0; endcase",
         }) {
        const auto compiled = compile(body);
        EXPECT_TRUE(compiled.success) << body << compiled.diagnostics;
        EXPECT_FALSE(compiled.unsupported) << body << compiled.diagnostics;
    }
}
}  // namespace
