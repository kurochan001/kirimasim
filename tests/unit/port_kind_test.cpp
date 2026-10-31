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
            (std::filesystem::temp_directory_path() / "kirimasim-port-kind-XXXXXX").string();
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
TEST(PortKind, UnwrittenSupplyInitializesBothReadoutsWithoutAProcess) {
    const auto c = compile(R"(
module leaf(output supply1 p); endmodule
module top; wire [2:0] w; leaf u(w[1]);
initial begin $display("%b %b",w,u.p); #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design.strengthOrigins.size(), 1U);
    const auto& origin = c.design.strengthOrigins.front();
    EXPECT_EQ(origin.kind, s::StrengthOriginKind::Supply);
    EXPECT_EQ(origin.owner, s::kNone);
    EXPECT_EQ(origin.initial.lo, 7);
    EXPECT_EQ(origin.initial.hi, 7);
    ASSERT_EQ(c.design.strengthEdges.size(), 1U);
    const auto& edge = c.design.strengthEdges.front();
    for (const auto n : {edge.first, edge.second}) {
        EXPECT_EQ(c.design.strengthNodes[n].initial.lo, 7);
        EXPECT_EQ(c.design.strengthNodes[n].initial.hi, 7);
    }
    run(c.design, "z1z 1\nz1z 1\n");
}
TEST(PortKind, ExternalOppositePullWinsAndIsMaterializedOnce) {
    const auto c = compile(R"(
module leaf(output tri0 p); endmodule
module top; tri1 [2:0] w; leaf u(w[1]);
initial begin $display("%b %b",w,u.p); #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design.strengthOrigins.size(), 3U);
    for (const auto& origin : c.design.strengthOrigins) {
        EXPECT_EQ(origin.kind, s::StrengthOriginKind::Pull);
        EXPECT_EQ(origin.initial.lo, 5);
        EXPECT_EQ(origin.initial.hi, 5);
    }
    run(c.design, "111 1\n111 1\n");
}
TEST(PortKind, WiredKindAppliesToBothEndpointsAndOnlySelectedBits) {
    const auto c = compile(R"(
module leaf(output wand p); assign (weak1,weak0) p=0; endmodule
module top; wire [2:0] w; leaf u(w[1]); assign (weak1,weak0) w[1]=1;
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design.strengthEdges.size(), 1U);
    const auto& edge = c.design.strengthEdges.front();
    for (const auto n : {edge.first, edge.second})
        EXPECT_EQ(c.design.strengthNodes[n].kind, s::SlotKind::Wand);
    std::size_t untouched = 0;
    for (const auto& node : c.design.strengthNodes)
        if (node.target.lsb != 1 && c.design.slot(node.target.slot).type.width == 3) {
            EXPECT_EQ(node.kind, s::SlotKind::Wire);
            ++untouched;
        }
    EXPECT_EQ(untouched, 2U);
    EXPECT_EQ(c.design.strengthOrigins.size(), 2U);
    for (const auto& origin : c.design.strengthOrigins)
        EXPECT_EQ(origin.kind, s::StrengthOriginKind::Process);
    run(c.design, "z0z 0\n");
}
TEST(PortKind, ArrayAliasMapsDistinctImplicitKindsToPhysicalOrdinals) {
    const auto c = compile(R"(
module leaf(output tri0 lo,output supply1 hi); endmodule
module middle(inout wire [2:0] bus[5:4]); leaf u(bus[4][1],bus[5][2]); endmodule
module top; wire [2:0] w[-1:0]; middle m(w);
initial begin $display("%b %b %b%b",w[-1],w[0],m.u.hi,m.u.lo);
#1; $display("%b %b %b%b",w[-1],w[0],m.u.hi,m.u.lo); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design.strengthOrigins.size(), 2U);
    for (const auto& origin : c.design.strengthOrigins) {
        const auto& node = c.design.strengthNodes[origin.node];
        EXPECT_EQ(c.design.slot(node.target.slot).viewOf, s::kNone);
        if (origin.kind == s::StrengthOriginKind::Pull) {
            EXPECT_EQ(node.target.firstElem, 1U);
            EXPECT_EQ(node.target.lsb, 1U);
            EXPECT_EQ(origin.initial.lo, -5);
        } else {
            EXPECT_EQ(origin.kind, s::StrengthOriginKind::Supply);
            EXPECT_EQ(node.target.firstElem, 0U);
            EXPECT_EQ(node.target.lsb, 2U);
            EXPECT_EQ(origin.initial.lo, 7);
        }
    }
    run(c.design, "1zz z0z 10\n1zz z0z 10\n");
}
TEST(PortKind, RepeatedConnectionsShareOneImplicitRoot) {
    const auto c = compile(R"(
module down(output tri0 p); endmodule
module up(output tri1 p); endmodule
module top; wire [2:0] w; down a(w[1]); up b(w[1]);
initial begin #1; $display("%b %b %b",w,a.p,b.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_EQ(c.design.strengthOrigins.size(), 1U);
    EXPECT_EQ(c.design.strengthEdges.size(), 2U);
    run(c.design, "z0z 0 0\n");
}
TEST(PortKind, TranRetainsTheDistinctNetsImplicitRoot) {
    const auto c = compile(R"(
module leaf(output tri0 p); endmodule
module top; tri0 [2:0] w; tri1 q; leaf u(w[1]); tran t(q,w[1]);
initial begin #1; $display("%b %b %b",w,u.p,q); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_EQ(c.design.strengthOrigins.size(), 4U);
    EXPECT_EQ(c.design.strengthEdges.size(), 2U);
    run(c.design, "0x0 x x\n");
}
TEST(PortKind, SupplyIsNotAttenuatedUntilItCrossesAMosDevice) {
    const auto c = compile(R"(
module leaf(output supply1 p); endmodule
module top; wire [2:0] w; wire q; leaf u(w[1]); nmos t(q,w[1],1'b1);
assign (supply1,supply0) w[1]=0;
assign (supply1,supply0) q=0;
initial begin #1; $display("%b %b %b",w[1],u.p,q); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    run(c.design, "x x 0\n");
}
TEST(PortKind, WrittenCollapsedBitsRetainInitialXBeforeResolution) {
    const auto c = compile(R"(
module leaf(output supply1 p); assign p=0; endmodule
module top; wire [2:0] w; leaf u(w[1]);
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design.strengthEdges.size(), 1U);
    const auto& edge = c.design.strengthEdges.front();
    for (const auto n : {edge.first, edge.second}) {
        EXPECT_EQ(c.design.strengthNodes[n].initial.lo, -6);
        EXPECT_EQ(c.design.strengthNodes[n].initial.hi, 6);
    }
    run(c.design, "z1z 1\n");
}
}  // namespace
