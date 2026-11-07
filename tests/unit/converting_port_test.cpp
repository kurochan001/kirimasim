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
            (std::filesystem::temp_directory_path() / "kirimasim-converting-port-XXXXXX").string();
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
TEST(ConvertingPort, OutputConversionUsesAnOrdinaryStrongDriver) {
    const auto c = compile(R"(
module leaf(output wire [1:0] p); assign (weak1,weak0) p=0; endmodule
module top; wire [3:0] w; leaf u(w); assign (weak1,weak0) w='1;
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_TRUE(c.design().strengthEdges.empty());
    std::size_t carriers = 0;
    for (const auto& p : c.design().processes) {
        if (!p.name.ends_with(".$port.p")) continue;
        ++carriers;
        ASSERT_EQ(p.drives.size(), 1U);
        const auto& slot = c.design().slot(p.drives.front().slot);
        EXPECT_NE(slot.driverOf, s::kNone);
        EXPECT_EQ(slot.strength0, 6);
        EXPECT_EQ(slot.strength1, 6);
    }
    EXPECT_EQ(carriers, 1U);
    run(c.design(), "0000 00\n");
}
TEST(ConvertingPort, InputConversionDoesNotSendLocalDriversBackToTheActual) {
    const auto c = compile(R"(
module leaf(input wire [3:0] p); assign (weak1,weak0) p=0; endmodule
module top; wire [1:0] w; leaf u(w); assign (weak1,weak0) w='1;
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_TRUE(c.design().strengthEdges.empty());
    run(c.design(), "11 0011\n");
}
TEST(ConvertingPort, ExplicitSameWidthCastRetainsAValueCarrier) {
    const auto c = compile(R"(
module leaf(input wire [2:0] p); assign (weak1,weak0) p=0; endmodule
module top; wire [2:0] w; leaf u(3'(w)); assign (weak1,weak0) w='1;
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_TRUE(c.design().strengthEdges.empty());
    run(c.design(), "111 111\n");
}
TEST(ConvertingPort, NestedTwoStateCastConvertsUnknownBeforeWidening) {
    const auto c = compile(R"(
module leaf(input wire [2:0] p); assign (weak1,weak0) p='1; endmodule
module top; wire [2:0] w; leaf u({bit'(w)}); assign (weak1,weak0) w='x;
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_TRUE(c.design().strengthEdges.empty());
    run(c.design(), "xxx 000\n");
}
TEST(ConvertingPort, SupplyThroughAConversionEntersObservedStorageAtStrong) {
    const auto c = compile(R"(
module leaf(output supply1 [1:0] p); endmodule
module top; wire [3:0] w; tri1 q; leaf u(w);
assign (supply1,supply0) w='0;
nmos t(q,w[0],1'b1);
initial begin #1; $display("%b %b %b",w,u.p,q); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    EXPECT_TRUE(std::ranges::none_of(c.design().strengthEdges, [](const auto& edge) {
        return edge.kind == s::StrengthEdgeKind::Identity;
    }));
    run(c.design(), "0000 11 0\n");
}
TEST(ConvertingPort, SameTypeStructuralPortsStillPreservePhysicalOrigins) {
    const auto c = compile(R"(
module leaf(output wire p); assign (weak1,weak0) p=0; endmodule
module top; wire [2:0] w; leaf u(w[1]); assign (weak1,weak0) w[1]=1;
initial begin #1; $display("%b %b",w,u.p); end
endmodule
)");
    ASSERT_TRUE(c.success) << c.diagnostics << c.verifyErrors;
    ASSERT_EQ(c.design().strengthEdges.size(), 1U);
    EXPECT_EQ(c.design().strengthEdges.front().kind, s::StrengthEdgeKind::Identity);
    EXPECT_EQ(c.design().strengthOrigins.size(), 2U);
    run(c.design(), "zxz x\n");
}
TEST(ConvertingPort, TwoStateNetDeclarationsRemainStrictFrontendErrors) {
    for (const auto* direction : {"input", "output"}) {
        const auto c =
            compile(std::string("module leaf(") + direction + R"( wire bit [1:0] p); endmodule
module top; wire [2:0] w; leaf u(w); endmodule
)");
        EXPECT_FALSE(c.success);
        EXPECT_NE(c.diagnostics.find("not a valid type for a net"), std::string::npos)
            << c.diagnostics;
    }
}
TEST(ConvertingPort, ExplicitPartialInoutRetainsItsDiagnostic) {
    const auto c = compile(R"(
module leaf(inout wire [1:0] p); endmodule
module top; wire [2:0] w; leaf u(w[2:0]); endmodule
)");
    EXPECT_FALSE(c.success);
    EXPECT_NE(c.diagnostics.find("unsupported: inout port"), std::string::npos) << c.diagnostics;
}
}  // namespace
