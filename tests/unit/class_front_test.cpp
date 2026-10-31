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
    std::string file;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-class-front-XXXXXX").string();
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
f::CompileResult compile(std::string_view text, bool initialValues = false) {
    Source source(text);
    EXPECT_FALSE(source.file.empty());
    if (source.file.empty()) return {};
    f::CompileOptions options;
    options.initValues = initialValues;
    return f::compile(std::vector<std::string>{source.file}, options);
}
unsigned slot(const s::Design& d, std::string_view suffix) {
    for (unsigned i = 0; i < d.slots.size(); ++i)
        if (d.slots[i].name.ends_with(suffix)) return i;
    ADD_FAILURE() << suffix;
    return 0;
}
TEST(ClassFront, CanonicalSpecializationsAndInterfacesSurviveDetachedSource) {
    auto compiled = compile(R"(
interface class Reader; pure virtual function int read_value(); endclass
class Box #(int W=32) implements Reader;
 bit[W-1:0] data;
 virtual function int read_value(); return W; endfunction
endclass
module top;
 typedef Box#(65) Alias;
 Box#(65) left; Alias same; Box#(129) wide; Reader view;
 initial begin left=new;same=left;wide=new;view=left;
 $display("%0d %0d",view.read_value(),wide.read_value());end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design.classes.size(), 3U);
    const auto& d = compiled.design;
    EXPECT_EQ(d.slots[slot(d, "left")].type, d.slots[slot(d, "same")].type);
    EXPECT_NE(d.slots[slot(d, "left")].type, d.slots[slot(d, "wide")].type);
    unsigned interfaces = 0;
    for (unsigned i = 0; i < d.classes.size(); ++i)
        if (d.classes[i].interfaceClass) {
            ++interfaces;
            EXPECT_EQ(d.classes[i].constructor, s::kNone);
            for (const auto& sub : d.subroutines)
                EXPECT_NE(sub.classOwner, s::ClassId{i});
        }
    EXPECT_EQ(interfaces, 1U);
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "65 129\n");
}
TEST(ClassFront, SourceSeedsAndCopyRetainObjectStateWithoutConstructorReplay) {
    auto compiled = compile(R"(
class Box;
 static int count=0;
 function new();count++;endfunction
endclass
module top;Box original,copied;
 initial begin original=new;original.srandom(123);copied=new original;
 $display("%0d",Box::count);end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "1\n");
    const auto a = ctx.heap().objectInfo(
        {*ctx.store().element(k::SlotId{slot(compiled.design, "original")}, 0), &ctx.heap()});
    const auto b = ctx.heap().objectInfo(
        {*ctx.store().element(k::SlotId{slot(compiled.design, "copied")}, 0), &ctx.heap()});
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(a.value.randomState, 123U);
    EXPECT_EQ(b.value.randomState, a.value.randomState);
    EXPECT_NE(a.value.identity, b.value.identity);
}
TEST(ClassFront, UnsupportedClassBoundariesAreSourceDiagnostics) {
    for (const auto* source :
         {"class Box;int data;endclass module top;Box a;int v;initial a=new;always_comb "
          "v=a.data;endmodule",
          "class Box;int data;endclass module top;Box a;initial begin a=new;wait(a.data);end "
          "endmodule",
          "class Box;event e;endclass module top;Box a;initial a=new;endmodule",
          "class Box;int data;endclass module top;Box a;initial begin a=new;a.data<=3;end "
          "endmodule",
          "class Box;int data;endclass module top;Box a;initial begin a=new;a.data[1]=1;end "
          "endmodule"}) {
        const auto compiled = compile(source);
        EXPECT_FALSE(compiled.success) << source;
        EXPECT_TRUE(compiled.unsupported) << compiled.diagnostics << compiled.verifyErrors;
        EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    }
    const auto initialized =
        compile("class Box;int data;endclass module top;Box a;initial a=new;endmodule", true);
    EXPECT_FALSE(initialized.success);
    EXPECT_TRUE(initialized.unsupported);
    EXPECT_TRUE(initialized.verifyErrors.empty());
}
TEST(ClassFront, NullMethodChecksFollowArgumentEffectsAndPrecedeTheBody) {
    auto compiled = compile(R"(
class Box;
 static int effects=0;
 function void change(int n); effects+=100;endfunction
endclass
module top;Box object;
 function int step();Box::effects++;return 1;endfunction
 initial object.change(step());
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Error);
    EXPECT_EQ(*ctx.store().element(k::SlotId{slot(compiled.design, "effects")}, 0), 1U);
    EXPECT_NE(output.text().find("null class handle access"), std::string::npos);
    EXPECT_NE(output.text().find("design.sv:8: error:"), std::string::npos);
}
TEST(ClassFront, StaticOnlyCallsDoNotCreateObjectDescriptorsForUnusedFields) {
    for (const auto* source :
         {"class Box;event unused;static function int read_value();return 9;endfunction endclass "
          "module top;initial $display(\"%0d\",Box::read_value());endmodule",
          "class Box;event unused;endclass module top;function automatic Box never();return "
          "null;endfunction initial $display(\"%0d\",9);endmodule"}) {
        auto compiled = compile(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        EXPECT_TRUE(compiled.design.classes.empty());
        Capture output;
        ASSERT_NE(output.file, nullptr);
        k::Context ctx({output.file, output.file});
        ASSERT_EQ(kirimasim::vm::run(compiled.design, ctx).reason, k::EndReason::Quiescent);
        EXPECT_EQ(output.text(), "9\n");
    }
}
// SIR_SPEC 7.2 requires explicit normal scope exits, including return.
// Runtime call cleanup can hide a producer violation from output-only tests.
TEST(ClassFront, ReturnsCloseNamedScopesInTheProducedControlFlow) {
    auto compiled = compile(R"(
module top;
 function automatic int scoped(input bit stop);
  begin: region
   fork begin #1;$display("child");end join_none
   if(stop) disable region;
   return 7;
  end
  return 9;
 endfunction
 initial begin int n;n=scoped(0);#2;end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_GT(compiled.design.numScopes, 0U);
    bool checked = false;
    for (const auto& sub : compiled.design.subroutines) {
        if (!sub.name.ends_with("scoped")) continue;
        checked = true;
        std::vector<std::array<bool, 2>> seen(sub.blocks.size());
        std::vector<std::pair<unsigned, bool>> pending{{0, false}};
        unsigned returns = 0;
        while (!pending.empty()) {
            const auto [block, incoming] = pending.back();
            pending.pop_back();
            if (seen[block][incoming ? 1 : 0]) continue;
            seen[block][incoming ? 1 : 0] = true;
            bool active = incoming;
            for (const auto& instruction : sub.blocks[block].instrs) {
                if (instruction.op == s::Op::ScopeEnter) active = true;
                if (instruction.op == s::Op::ScopeLeave || instruction.op == s::Op::DisableScope)
                    active = false;
                if (instruction.op == s::Op::Ret) {
                    ++returns;
                    EXPECT_FALSE(active) << sub.name << " block " << block;
                }
                if (s::isTerminator(instruction.op))
                    for (const auto& operand : instruction.operands)
                        if (operand.kind == s::Operand::Kind::Block)
                            pending.emplace_back(operand.id, active);
            }
        }
        EXPECT_GE(returns, 1U);
    }
    EXPECT_TRUE(checked);
}
TEST(ClassFront, VMStillRejectsImplicitSensitivityBeforeRuntimeStorage) {
    auto compiled =
        compile("class Box;int data;endclass module top;Box object;initial object=new;endmodule");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_FALSE(compiled.design.processes.empty());
    compiled.design.processes[0].kind = s::ProcessKind::Comb;
    ASSERT_TRUE(s::verify(compiled.design).empty());
    k::Context ctx;
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    EXPECT_NE(result.error.find("observation"), std::string::npos);
    EXPECT_EQ(ctx.store().size(), 0U);
    EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
}
}  // namespace
