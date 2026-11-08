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
#include "verified_run.h"
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
            (std::filesystem::temp_directory_path() / "kirimasim-partial-inout-XXXXXX").string();
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
    const auto result = kirimasim::test::runVerified(design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), expected);
}
void noCarrier(const s::Design& d) {
    for (const auto& p : d.processes)
        EXPECT_FALSE(p.name.ends_with(".$port.p")) << p.name;
}
TEST(PartialInoutPort, RootlessIdentityDoesNotInventAProcessOrAnOrigin) {
    const auto c = compile(R"(
module leaf(inout wire p); endmodule
module top; wire [1:0] w; leaf u(w[1]);
initial begin $display("%b %b",w,u.p); #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design().strengthEdges.size(), 1U);
    EXPECT_EQ(c.design().strengthEdges.front().kind, s::StrengthEdgeKind::Identity);
    EXPECT_TRUE(c.design().strengthOrigins.empty());
    noCarrier(c.design());
    run(c.design(), "zz z\nzz z\n");
}
TEST(PartialInoutPort, ARealDriverIsImportedOnceAtItsOwnStrength) {
    const auto c = compile(R"(
module leaf(inout wire p); assign (weak1,weak0) p=0; endmodule
module top; wire [2:0] w; leaf u(w[1]);
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design().strengthOrigins.size(), 1U);
    EXPECT_EQ(c.design().strengthOrigins.front().kind, s::StrengthOriginKind::Process);
    EXPECT_NE(c.design().strengthOrigins.front().owner, s::kNone);
    EXPECT_EQ(c.design().strengthOrigins.front().initial.lo, -3);
    EXPECT_EQ(c.design().strengthOrigins.front().initial.hi, 3);
    noCarrier(c.design());
    run(c.design(), "z0z 0\n");
}
TEST(PartialInoutPort, ArrayAliasBoundsMapToPhysicalOrdinalBeforeConnection) {
    const auto c = compile(R"(
module leaf(inout wire p,input logic d); assign (weak1,weak0) p=d; endmodule
module middle(inout wire [1:0] bus[5:4],input logic d); leaf u(bus[4][1],d); endmodule
module top; logic a=0,b=1; wire [1:0] w[-1:0]; middle m(w,a);
assign (weak1,weak0) w[0][1]=b;
initial begin #1; $display("%b %b %b",w[0],m.u.p,w[-1]);
b=1'bz; #1; $display("%b %b",w[0],m.u.p);
a=1'bz; #1; $display("%b %b",w[0],m.u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design().strengthEdges.size(), 1U);
    bool mapped = false;
    const auto& edge = c.design().strengthEdges.front();
    for (const auto n : {edge.first, edge.second}) {
        const auto& at = c.design().strengthNodes[n].target;
        if (c.design().slot(at.slot).dims.empty()) continue;
        mapped = true;
        EXPECT_EQ(c.design().slot(at.slot).viewOf, s::kNone);
        EXPECT_EQ(at.firstElem, 1U);
        EXPECT_EQ(at.lsb, 1U);
    }
    EXPECT_TRUE(mapped);
    EXPECT_EQ(c.design().strengthOrigins.size(), 2U);
    noCarrier(c.design());
    run(c.design(), "xz x zz\n0z 0\nzz z\n");
}
TEST(PartialInoutPort, SupplyCrossesIdentityUnattenuatedAtTimeZero) {
    const auto c = compile(R"(
module leaf(inout supply1 p); endmodule
module top; wire [2:0] w; leaf u(w[1]);
initial begin $display("%b %b",w,u.p); #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design().strengthOrigins.size(), 1U);
    const auto& origin = c.design().strengthOrigins.front();
    EXPECT_EQ(origin.kind, s::StrengthOriginKind::Supply);
    EXPECT_EQ(origin.initial.lo, 7);
    EXPECT_EQ(origin.initial.hi, 7);
    noCarrier(c.design());
    run(c.design(), "z1z 1\nz1z 1\n");
}
TEST(PartialInoutPort, EffectivePullKindHasOneOriginPerIdentityComponent) {
    const auto c = compile(R"(
module leaf(inout tri0 p); endmodule
module top; tri1 [1:0] w; leaf u(w[1]);
initial begin $display("%b %b",w,u.p); #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design().strengthOrigins.size(), 2U);
    for (const auto& origin : c.design().strengthOrigins) {
        EXPECT_EQ(origin.kind, s::StrengthOriginKind::Pull);
        EXPECT_EQ(origin.initial.lo, 5);
        EXPECT_EQ(origin.initial.hi, 5);
    }
    run(c.design(), "11 1\n11 1\n");
}
TEST(PartialInoutPort, PassiveCycleLosesItsValueWhenBothRealOriginsWithdraw) {
    const auto c = compile(R"(
module leaf(inout wire p,input logic d); assign (weak1,weak0) p=d; endmodule
module top; logic a=0,b=1; wire [2:0] w; leaf x(w[0],a),y(w[1],b);
tran t0(w[0],w[1]),t1(w[1],w[2]),t2(w[2],w[0]);
initial begin #1; $display("%b %b %b",w,x.p,y.p);
a=1'bz; #1; $display("%b %b %b",w,x.p,y.p);
b=1'bz; #1; $display("%b %b %b",w,x.p,y.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_EQ(c.design().strengthEdges.size(), 5U);
    EXPECT_EQ(c.design().strengthOrigins.size(), 2U);
    noCarrier(c.design());
    run(c.design(), "xxx x x\n111 1 1\nzzz z z\n");
}
TEST(PartialInoutPort, InvalidMappingsRetainPositionedRejection) {
    struct Case {
        const char* source;
        const char* diagnostic;
    };
    for (const auto& item :
         {Case{R"(
module leaf(inout wire [1:0] p); endmodule
module top; wire [2:0] w; leaf u(w); endmodule
)",
               "unsupported: inout port"},
          Case{R"(
module leaf(inout wire p); endmodule
module top; wire [2:0] w; int i=1; leaf u(w[i]); endmodule
)",
               "reference to non-constant variable 'i' is not allowed in a constant expression"},
          Case{R"(
module leaf(inout wire p); endmodule
module top; wire [2:0] w; leaf u(w[1]); initial force w[1]=1; endmodule
)",
               "unsupported: inout port"},
          Case{R"(
module leaf(inout wire signed [1:0] p); endmodule
module top; wire [2:0] w; leaf u(w[1:0]); endmodule
)",
               "unsupported: inout port"}}) {
        const auto c = compile(item.source);
        EXPECT_FALSE(c.success);
        EXPECT_NE(c.diagnostics.find(item.diagnostic), std::string::npos) << c.diagnostics;
        EXPECT_FALSE(c.verified.has_value());  // no Design, so no edge reaches a run
    }
}
TEST(PartialInoutPort, UwireAndVariableActualsRemainStrictFrontendErrors) {
    for (const auto* source : {R"(
module leaf(inout uwire p); endmodule
module top; wire [1:0] w; leaf u(w[1]); endmodule
)",
                               R"(
module leaf(inout wire p); endmodule
module top; logic [1:0] w; leaf u(w[1]); endmodule
)"}) {
        const auto c = compile(source);
        EXPECT_FALSE(c.success);
        EXPECT_FALSE(c.diagnostics.empty());
    }
}
}  // namespace
