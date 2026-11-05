// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kirimasim/front/front.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace f = kirimasim::front;
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
struct Source {
    std::filesystem::path directory;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-implicit-front-XXXXXX").string();
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
f::CompileResult compile(std::string_view text) {
    Source source(text);
    EXPECT_FALSE(source.directory.empty());
    if (source.directory.empty()) return {};
    return f::compile(std::vector<std::string>{(source.directory / "design.sv").string()});
}
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() const {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string result;
        std::array<char, 512> buffer{};
        while (const auto size = std::fread(buffer.data(), 1, buffer.size(), file))
            result.append(buffer.data(), size);
        return result;
    }
};

TEST(ImplicitFront, WildcardFunctionBodiesKeepOnlyActualArgumentTriggers) {
    auto compiled = compile(R"(
module top;
int q[$];int input_value=0,observed=-99;
function int inner(input int n);return q.size()+n;endfunction
function int outer(input int n);return inner(n);endfunction
always @* observed=outer(input_value);
initial begin #1;q.push_back(7);#1;$display("%0d",observed);
input_value=1;#1;$display("%0d",observed);q.push_back(9);#1;$display("%0d",observed);
input_value=2;#1;$display("%0d",observed);end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    bool found = false;
    for (const auto& process : compiled.design.processes) {
        if (process.kind != s::ProcessKind::CombAt) continue;
        found = true;
        ASSERT_EQ(process.triggers.size(), 1U);
        const auto& trigger = process.triggers.front();
        ASSERT_NE(trigger.range.slot, s::kNone);
        EXPECT_TRUE(compiled.design.slot(trigger.range.slot).name.ends_with("input_value"));
        EXPECT_EQ(trigger.guard, s::kNone);
        EXPECT_EQ(trigger.watch, s::kNone);
    }
    EXPECT_TRUE(found);
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "-99\n2\n2\n4\n");
}

TEST(ImplicitFront, TypedContainerBindingsKeepStaticClosureAndOrdinaryInputsSeparate) {
    auto compiled = compile(R"(
module top;
logic[64:0] q[$];int input_value=0,observed;
function int inner();return q.size();endfunction
function int outer();return inner()+inner();endfunction
always_comb observed=outer()+q.size()+input_value;
initial begin #1;q.push_back(1);#1;input_value=2;#1;$display("%0d",observed);end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_TRUE(s::verify(compiled.design).empty());
    bool found = false;
    for (const auto& process : compiled.design.processes) {
        if (process.kind != s::ProcessKind::Comb) continue;
        found = true;
        ASSERT_EQ(process.implicitContainers.size(), 1U);
        EXPECT_TRUE(compiled.design.slot(process.implicitContainers[0].slot).name.ends_with(".q"));
        ASSERT_EQ(process.triggers.size(), 1U);
        EXPECT_TRUE(
            compiled.design.slot(process.triggers[0].range.slot).name.ends_with("input_value"));
    }
    EXPECT_TRUE(found);
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "5\n");
}

TEST(ImplicitFront, CalledPrivateContainersAndWrittenContainersDoNotBecomeObservers) {
    auto compiled = compile(R"(
module top;
int observed;logic[31:0] q[$];
function int private_read();logic[31:0] local_q[$];local_q.push_back(1);return local_q[0];endfunction
always_comb begin q='{1,2};observed=q.size()+private_read();end
initial begin #1;$display("%0d",observed);end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (const auto& process : compiled.design.processes)
        if (process.kind == s::ProcessKind::Comb) {
            EXPECT_TRUE(process.implicitContainers.empty());
            EXPECT_TRUE(process.triggers.empty());
        }
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "3\n");
}

TEST(ImplicitFront, WildcardEventFunctionBoundariesRemainDiagnosed) {
    for (const auto source :
         {"module top;event pulse;bit observed;"
          "function bit sample();return pulse.triggered;endfunction "
          "always @* observed=sample();endmodule",
          "module top;event pulse;bit observed;"
          "function bit sample(input event value);return value.triggered;endfunction "
          "always @* observed=sample(pulse);endmodule"}) {
        auto compiled = compile(source);
        EXPECT_FALSE(compiled.success);
        EXPECT_NE(compiled.diagnostics.find("implicit sensitivity to named-event triggered values"),
                  std::string::npos)
            << compiled.diagnostics;
    }
}

TEST(ImplicitFront, ActualImplicitContainerReadsRemainDiagnosed) {
    for (const auto statement :
         {"always @* observed=q.size();", "always @* observed=read_argument(q);",
          "always_comb observed=read_global();", "always_latch observed=read_global();",
          "assign observed=read_global();"}) {
        const std::string declaration = std::string_view(statement).starts_with("assign")
                                            ? "wire [31:0] observed;"
                                            : "int observed;";
        auto compiled =
            compile("module top;int q[$];" + declaration +
                    "function int read_global();return q.size();endfunction "
                    "function int read_argument(input int a[$]);return a.size();endfunction " +
                    statement + "endmodule");
        EXPECT_FALSE(compiled.success) << statement;
        EXPECT_NE(compiled.diagnostics.find("implicit sensitivity to dynamic container values"),
                  std::string::npos)
            << compiled.diagnostics;
    }
}

TEST(ImplicitFront, WildcardObjectObservationBoundaryIsPreserved) {
    auto compiled = compile(R"(
class Box;int value;endclass
module top;
Box object=new;int input_value=0,observed;
function int sample(input int n);return object.value+n;endfunction
always @* observed=sample(input_value);
endmodule
)");
    EXPECT_FALSE(compiled.success);
    EXPECT_NE(compiled.diagnostics.find("dependency observation"), std::string::npos)
        << compiled.diagnostics;
}
}  // namespace
