// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <cstdint>
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
#include <algorithm>
namespace {
namespace f = kirimasim::front;
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-array-element-port-XXXXXX")
                .string();
        if (mkdtemp(pattern.data()) == nullptr) return;
        directory = pattern;
        const auto path = directory / "design.sv";
        std::ofstream stream(path);
        stream << text;
        stream.close();
        if (stream) file = std::filesystem::relative(std::filesystem::canonical(path)).string();
    }
    ~Source() {
        if (!directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
    }
};
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
        while (const auto count = std::fread(buffer.data(), 1, buffer.size(), file))
            result.append(buffer.data(), count);
        return result;
    }
};
f::CompileResult compile(std::string_view text) {
    Source source(text);
    EXPECT_FALSE(source.file.empty());
    if (source.file.empty()) return {};
    f::CompileOptions options;
    return f::compile(std::vector<std::string>{source.file}, options);
}

void run(const s::Design& design, std::string_view expected) {
    EXPECT_TRUE(s::verify(design).empty());
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), expected);
}
TEST(ArrayElementPort, PhysicalOrdinalAndStrengthOriginsSurviveSourceRemoval) {
    auto compiled = compile(R"(
module leaf(output wire p); assign (weak1,weak0) p=0; endmodule
module top;
wire [128:0] w[2:1][-1:1];
leaf u(w[1][0][65]);
assign (strong1,strong0) w[1][0][65]=1;
initial begin #1; $display("%b %b %b",w[1][0][65],u.p,w[2][-1][65]); end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto& d = compiled.design;
    ASSERT_EQ(d.strengthEdges.size(), 1U);
    const auto& edge = d.strengthEdges.front();
    EXPECT_EQ(edge.kind, s::StrengthEdgeKind::Identity);
    bool array = false;
    for (const auto n : {edge.first, edge.second}) {
        const auto& at = d.strengthNodes[n].target;
        if (d.slot(at.slot).dims.empty()) continue;
        array = true;
        EXPECT_EQ(at.firstElem, 4U);
        EXPECT_EQ(at.lsb, 65U);
        EXPECT_EQ(at.width, 1U);
    }
    EXPECT_TRUE(array);
    EXPECT_EQ(d.strengthOrigins.size(), 2U);
    run(d, "1 1 z\n");
}
TEST(ArrayElementPort, AliasUsesTheSourceBoundsBeforeThePhysicalSlot) {
    auto compiled = compile(R"(
module leaf(output wire p); assign (weak1,weak0) p=0; endmodule
module middle(inout wire [1:0] bus[5:4]); leaf u(bus[4][1]); endmodule
module top;
wire [1:0] w[-1:0]; middle m(w);
assign (weak1,weak0) w[0][1]=1;
initial begin #1; $display("%b %b %b",w[0][1],m.u.p,w[-1][1]); end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design.strengthEdges.size(), 1U);
    const auto& edge = compiled.design.strengthEdges.front();
    bool mapped = false;
    for (const auto n : {edge.first, edge.second}) {
        const auto& at = compiled.design.strengthNodes[n].target;
        if (compiled.design.slot(at.slot).dims.empty()) continue;
        mapped = true;
        EXPECT_EQ(at.firstElem, 1U);
        EXPECT_EQ(at.lsb, 1U);
        EXPECT_EQ(compiled.design.slot(at.slot).viewOf, s::kNone);
    }
    EXPECT_TRUE(mapped);
    run(compiled.design, "x x z\n");
}
TEST(ArrayElementPort, VariablePortsKeepDirectedValueCarriers) {
    auto compiled = compile(R"(
module leaf(output var logic p,input logic d); assign p=d; endmodule
module top;
wire w[2]; logic d=0; leaf u(w[1],d); assign w[1]=1;
initial begin #1; $display("%b %b",w[1],u.p); end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    EXPECT_TRUE(compiled.design.strengthEdges.empty());
    run(compiled.design, "x 0\n");
}
TEST(ArrayElementPort, ClockingStillRequiresScalarPackedStorage) {
    for (const auto* expression : {"w[1]", "{w[0],w[1]}"}) {
        const auto compiled = compile(std::string(R"(
module top;
bit clk; logic w[2];
clocking cb @(posedge clk); input p=)") +
                                      expression + R"(; endclocking
endmodule
)");
        EXPECT_FALSE(compiled.success);
        EXPECT_NE(compiled.diagnostics.find("clocking source without static packed storage"),
                  std::string::npos)
            << compiled.diagnostics << compiled.verifyErrors;
    }
}
TEST(ArrayElementPort, PartialInoutKeepsItsPositionedBoundary) {
    const auto compiled = compile(R"(
module leaf(inout wire p); endmodule
module top; wire w[2]; leaf u(w[1]); endmodule
)");
    EXPECT_FALSE(compiled.success);
    EXPECT_NE(compiled.diagnostics.find("unsupported: inout port"), std::string::npos);
}
TEST(ArrayElementPort, MovingArraySourcesCannotBecomeIdentityEdges) {
    const auto compiled = compile(R"(
module leaf(input wire p); assign p=0; endmodule
module top; wire w[2]; int index=1; assign w[1]=1; leaf u(w[index]); endmodule
)");
    EXPECT_FALSE(compiled.success);
    EXPECT_TRUE(compiled.design.strengthEdges.empty());
}
TEST(ArrayElementPort, InvalidStaticArrayIndicesCannotBecomeIdentityEdges) {
    for (const auto* index : {"3", "-1", "1'bx", "129'h100000000000000000000000000000001"}) {
        const auto compiled = compile(std::string(R"(
module leaf(input wire p); assign p=0; endmodule
module top; wire w[0:1]; leaf u(w[)") +
                                      index + R"(]); endmodule
)");
        EXPECT_FALSE(compiled.success) << index;
        EXPECT_TRUE(compiled.design.strengthEdges.empty()) << index;
    }
}
}  // namespace
