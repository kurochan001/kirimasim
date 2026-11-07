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
// The option structures of a covergroup have no storage yet: each way to reach
// one is a diagnostic at the source, never a slot number past the last slot.
TEST(ClassFront, CovergroupOptionStructuresAreSourceDiagnostics) {
    // Two literals written next to each other would read as a missing comma.
    const std::vector<std::string> bodies = {
        "initial $display(\"%0d\",cg::type_option.weight);",
        "initial $display(\"%s\",cg::type_option.comment);",
        "int w;initial w=cg::type_option.goal;",
        "initial cg::type_option.weight=2;",
        "initial begin cg::type_option.goal=1;$finish;end",
        std::string("function automatic int f();return cg::type_option.weight;endfunction ") +
            "initial $display(\"%0d\",f());",
        "task automatic t();$display(\"%0d\",cg::type_option.weight);endtask initial t();",
        "initial fork $display(\"%0d\",cg::type_option.weight);join",
        "if(1) begin:g initial $display(\"%0d\",cg::type_option.weight);end",
        std::string(
            "class Box;function int f();return cg::type_option.weight;endfunction endclass ") +
            "Box b;initial begin b=new;$display(\"%0d\",b.f());end",
    };
    for (const auto& body : bodies) {
        const std::string source =
            std::string("module top;covergroup cg;endgroup ") + body + " endmodule";
        const auto compiled = compile(source.c_str());
        EXPECT_FALSE(compiled.success) << source;
        EXPECT_TRUE(compiled.unsupported)
            << source << compiled.diagnostics << compiled.verifyErrors;
        EXPECT_TRUE(compiled.verifyErrors.empty()) << source << compiled.verifyErrors;
    }
}
TEST(ClassFront, UnsupportedClassBoundariesAreSourceDiagnostics) {
    for (const auto* source :
         {"class Box;int data;endclass module top;Box a;initial begin a=new;wait(a.data);end "
          "endmodule",
          "class Box;event e;endclass module top;Box a;initial a=new;endmodule",
          "class Box;int data;endclass module top;Box a;initial begin a=new;a.data<=3;end "
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
TEST(ClassFront, FailedTaskCastPreservesDestinationAndArgumentEffects) {
    auto compiled = compile(R"(
class Base; endclass
class Derived extends Base; endclass
module top;
 Derived saved,dest[2]; Base source;
 int selected=0,calls=0;
 function int index_value();calls++;return selected;endfunction
 function Base source_value();calls++;selected=1;return source;endfunction
 initial begin
  saved=new;dest[0]=saved;source=new;
  $cast(dest[index_value()],source_value());
  $display("unreachable");
 end
 final $display("%b %b %0d %0d",dest[0]==saved,dest[1]==null,calls,selected);
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Error);
    const auto text = output.text();
    EXPECT_NE(text.find("class $cast failed"), std::string::npos);
    EXPECT_NE(text.find("design.sv:11: error:"), std::string::npos);
    EXPECT_NE(text.find("1 1 2 1\n"), std::string::npos);
    EXPECT_EQ(text.find("unreachable"), std::string::npos);
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
TEST(ClassFront, VMStillRejectsAtStarObjectObservationBeforeRuntimeStorage) {
    auto compiled =
        compile("class Box;int data;endclass module top;Box object;initial object=new;endmodule");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_FALSE(compiled.design.processes.empty());
    compiled.design.processes[0].kind = s::ProcessKind::CombAt;
    ASSERT_TRUE(s::verify(compiled.design).empty());
    k::Context ctx;
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    EXPECT_NE(result.error.find("observation"), std::string::npos);
    EXPECT_EQ(ctx.store().size(), 0U);
    EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
}
TEST(ClassFront, IndependentStaticClassCallsKeepOrdinarySensitivity) {
    auto compiled = compile(R"(
class Box;
 static int signal=0;
 int value=29;
 static function int inner();return signal;endfunction
 static function int outer();return inner();endfunction
endclass
module top;
 Box object;int observed;
 always_comb observed=Box::outer();
 initial begin object=new;#1;Box::signal=7;#1;
 $display("%0d %0d",object.value,observed);end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "29 7\n");
}
TEST(ClassFront, SelectedPropertyWritesKeepTheirSourceBoundaries) {
    for (const auto* statement :
         {"a.data[1]<=1;", "a.data[1]<=#1 1;", "a.data[1]<=@(go) 1;", "a.data[2:1]<=1;",
          "a.data[at]<=1;", "a.data[at+:2]<=1;", "a.pair.hi<=1;", "a.pair.hi[1]<=#1 1;"}) {
        const auto source =
            std::string("typedef struct packed {logic [3:0] hi;logic [3:0] lo;} pair_t;"
                        "class Box;int data;pair_t pair;endclass "
                        "module top;Box a;event go;int at=1;initial begin a=new;") +
            statement + "end endmodule";
        const auto compiled = compile(source);
        EXPECT_FALSE(compiled.success) << statement;
        EXPECT_TRUE(compiled.unsupported) << compiled.diagnostics << compiled.verifyErrors;
        EXPECT_NE(compiled.diagnostics.find("class property nonblocking or delayed target"),
                  std::string::npos)
            << compiled.diagnostics;
        EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    }
    const auto constant =
        compile("class Box;const logic [7:0] data;function new();data[1]=1;endfunction endclass "
                "module top;Box a;initial a=new;endmodule");
    EXPECT_FALSE(constant.success);
    EXPECT_TRUE(constant.unsupported) << constant.diagnostics << constant.verifyErrors;
    EXPECT_TRUE(constant.verifyErrors.empty()) << constant.verifyErrors;
}
TEST(ClassFront, ObjectObservationThroughOrdinaryCallsKeepsItsSourceBoundary) {
    for (const auto* observer : {"wait(outer());", "@(outer());", "$monitor(\"%0d\",outer());",
                                 "result<=@(outer()) 7;", "result<=@(signal iff outer()) 7;",
                                 "fork begin wait(outer());end begin #1;signal=1;end join"}) {
        const auto source =
            std::string("class Box;int value=7;endclass module top;Box object;int result,signal;"
                        "function int inner();return object.value;endfunction "
                        "function int outer();if(signal)return outer();return inner();endfunction "
                        "initial begin object=new;") +
            observer + " end endmodule";
        const auto compiled = compile(source);
        EXPECT_FALSE(compiled.success) << observer;
        EXPECT_TRUE(compiled.unsupported) << compiled.diagnostics << compiled.verifyErrors;
        EXPECT_NE(compiled.diagnostics.find("dependency observation"), std::string::npos)
            << compiled.diagnostics;
        EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    }
}
TEST(ClassFront, VMChecksObserverCallClosureBeforeAnyRuntimeStorage) {
    for (const auto* observer : {"wait(middle());", "$monitor(\"%0d\",middle());",
                                 "signal<=@(middle()) 7;", "signal<=@(signal iff middle()) 7;"}) {
        const auto source =
            std::string("class Box;int value=7;endclass module top;Box object;int signal,scratch;"
                        "function int numeric();return signal;endfunction "
                        "function int object_value();return object.value;endfunction "
                        "function int middle();return numeric();endfunction "
                        "initial begin object=new;scratch=object_value();") +
            observer + " end endmodule";
        auto compiled = compile(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        s::SubroutineId numeric = s::kNone, property = s::kNone;
        for (std::size_t i = 0; i < compiled.design.subroutines.size(); ++i) {
            const auto& sub = compiled.design.subroutines[i];
            if (sub.name.ends_with(".numeric")) numeric = s::SubroutineId{static_cast<unsigned>(i)};
            if (sub.name.ends_with(".object_value"))
                property = s::SubroutineId{static_cast<unsigned>(i)};
        }
        ASSERT_NE(numeric, s::kNone);
        ASSERT_NE(property, s::kNone);
        unsigned changed = 0;
        for (auto& sub : compiled.design.subroutines)
            for (auto& block : sub.blocks)
                for (auto& in : block.instrs)
                    if (in.op == s::Op::Call && in.operands[0].asSubroutine() == numeric) {
                        in.operands[0] = s::Operand::subroutine(property);
                        ++changed;
                    }
        ASSERT_EQ(changed, 1U);
        ASSERT_TRUE(s::verify(compiled.design).empty());
        k::Context ctx;
        const auto result = kirimasim::vm::run(compiled.design, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Internal) << observer;
        EXPECT_NE(result.error.find("observation"), std::string::npos) << result.error;
        EXPECT_EQ(ctx.store().size(), 0U);
        EXPECT_EQ(ctx.heap().stats().cells, 0U);
        EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
        EXPECT_EQ(ctx.activations().stats().live, 0U);
    }
}
TEST(ClassFront, ExplicitClocksExecuteObjectBodiesWithVerifiedNumericTriggers) {
    for (const auto* keyword : {"always", "always_ff"}) {
        const auto source =
            std::string("class Box;int value=0;function void bump();value++;endfunction endclass "
                        "module top;Box object;bit clock=0;") +
            keyword +
            " @(posedge clock) object.bump();initial begin object=new;"
            "#1 clock=1;#1 clock=0;#1 clock=1;#1 "
            "$display(\"%0d\",object.value);end endmodule";
        auto compiled = compile(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        ASSERT_TRUE(s::verify(compiled.design).empty());
        unsigned clocks = 0;
        for (const auto& process : compiled.design.processes)
            if (process.kind == s::ProcessKind::Ff || process.kind == s::ProcessKind::FfPlain) {
                ++clocks;
                ASSERT_EQ(process.triggers.size(), 1U);
                EXPECT_NE(process.triggers[0].range.slot, s::kNone);
                EXPECT_EQ(process.triggers[0].guard, s::kNone);
                EXPECT_EQ(process.triggers[0].watch, s::kNone);
            }
        EXPECT_EQ(clocks, 1U);
        Capture output;
        k::Context ctx({output.file, output.file});
        const auto result = kirimasim::vm::run(compiled.design, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(output.text(), "2\n");
    }
}
TEST(ClassFront, ClockedBodiesRetainReceiversAndVirtualDispatchAcrossAllocationPressure) {
    for (const auto* keyword : {"always", "always_ff"}) {
        const auto source =
            std::string("class Box;int value;virtual function int read_value();return -1;"
                        "endfunction endclass class Child extends Box;"
                        "function int read_value();repeat(300)begin automatic Box junk;"
                        "junk=new;end return value;endfunction endclass "
                        "module top;Box object;bit clock=0;int sampled,hits;"
                        "function automatic Box select_object();Box saved=object;"
                        "object=null;return saved;endfunction ") +
            keyword +
            " @(posedge clock)begin automatic Child child;child=new;child.value=29;"
            "object=child;sampled=select_object().read_value();hits++;end "
            "initial begin repeat(20)begin #1 clock=1;#1;"
            "if(sampled!=29)$fatal(1,\"receiver lost\");clock=0;end "
            "$display(\"%0d %0d\",sampled,hits);end endmodule";
        auto compiled = compile(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        Capture output;
        k::Context ctx({output.file, output.file});
        const auto result = kirimasim::vm::run(compiled.design, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(output.text(), "29 20\n");
        EXPECT_LT(ctx.heap().objectStats().live, 400U);
    }
}
TEST(ClassFront, ClockedObjectBodyDoesNotAuthorizeObjectDependentTriggerCallbacks) {
    for (const auto* keyword : {"always", "always_ff"})
        for (const auto* event : {"posedge clock iff middle()", "posedge (clock ^ middle())"}) {
            const auto source =
                std::string("class Box;int value=7;function void bump();value++;endfunction "
                            "endclass module top;Box object;bit clock=0,gate=1;int scratch;"
                            "function bit numeric();return gate;endfunction "
                            "function bit property_value();return object.value==7;endfunction "
                            "function bit middle();return numeric();endfunction ") +
                keyword + " @(" + event +
                ") object.bump();initial begin object=new;"
                "scratch=property_value();#1 clock=1;#1;end endmodule";
            auto compiled = compile(source);
            ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
            auto design = compiled.design;
            s::SubroutineId numeric = s::kNone, property = s::kNone;
            for (std::size_t i = 0; i < design.subroutines.size(); ++i) {
                const auto& sub = design.subroutines[i];
                if (sub.name.ends_with(".numeric"))
                    numeric = s::SubroutineId{static_cast<unsigned>(i)};
                if (sub.name.ends_with(".property_value"))
                    property = s::SubroutineId{static_cast<unsigned>(i)};
            }
            ASSERT_NE(numeric, s::kNone);
            ASSERT_NE(property, s::kNone);
            unsigned changed = 0;
            for (auto& sub : design.subroutines)
                for (auto& block : sub.blocks)
                    for (auto& in : block.instrs)
                        if (in.op == s::Op::Call && in.operands[0].asSubroutine() == numeric) {
                            in.operands[0] = s::Operand::subroutine(property);
                            ++changed;
                        }
            ASSERT_EQ(changed, 1U);
            ASSERT_TRUE(s::verify(design).empty());
            k::Context ctx;
            const auto result = kirimasim::vm::run(design, ctx);
            EXPECT_EQ(result.reason, k::EndReason::Internal) << event;
            EXPECT_NE(result.error.find("observation"), std::string::npos) << result.error;
            EXPECT_EQ(ctx.store().size(), 0U);
            EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
            EXPECT_EQ(ctx.heap().stats().cells, 0U);
            EXPECT_EQ(ctx.activations().stats().live, 0U);
        }
}
TEST(ClassFront, ClockedNullPropertyAccessRetainsThePositionedRuntimeError) {
    for (const auto* keyword : {"always", "always_ff"}) {
        const auto source = std::string("class Box;int value=7;endclass module top;Box object;"
                                        "bit clock=0;int sampled;") +
                            keyword +
                            " @(posedge clock)sampled=object.value;"
                            "initial #1 clock=1;endmodule";
        auto compiled = compile(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        Capture output;
        k::Context ctx({output.file, output.file});
        const auto result = kirimasim::vm::run(compiled.design, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Error) << result.error;
        EXPECT_NE(output.text().find("null"), std::string::npos) << output.text();
        EXPECT_EQ(ctx.heap().objectStats().live, 0U);
    }
}
TEST(ClassFront, ClockedObjectReadsCaptureOrdinaryNbaPayloadBeforeBodyMutation) {
    for (const auto* keyword : {"always", "always_ff"}) {
        const auto source =
            std::string("class Box;logic[128:0] data;endclass module top;Box object;"
                        "bit clock=0;logic[128:0] sampled;") +
            keyword +
            " @(posedge clock)begin object.data='z;sampled<=object.data;object.data='0;end "
            "initial begin object=new;#1 clock=1;#1;"
            "if(sampled!=='z || object.data!=='0)$fatal(1,\"payload not captured\");"
            "$display(\"captured\");end endmodule";
        auto compiled = compile(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        Capture output;
        k::Context ctx({output.file, output.file});
        const auto result = kirimasim::vm::run(compiled.design, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(output.text(), "captured\n");
    }
}
TEST(ClassFront, ImplicitClassBodiesObserveArgumentsAndStaticMethodsSeparately) {
    auto compiled = compile(R"(
class Box;
 static int hidden=11;
 int value=7;
 function int read_value(int n);return value+n+hidden;endfunction
 static function int read_hidden();return hidden;endfunction
endclass
module top;
 Box object=new;
 int input_value=1,arg_bias=2,observed,static_result;
 function int argument_value();return input_value+arg_bias;endfunction
 function int outer();return object.read_value(argument_value());endfunction
 always_comb observed=outer();
 always_comb static_result=Box::read_hidden();
 initial begin
 #1;$display("%0d",observed);
 Box::hidden=100;object.value=20;
 #1;$display("%0d %0d",observed,static_result);
 object=new;#1;$display("%0d",observed);
 arg_bias=3;#1;$display("%0d",observed);
 input_value=2;#1;$display("%0d",observed);
 end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    unsigned watched = 0;
    for (const auto& process : compiled.design.processes)
        if (process.kind == s::ProcessKind::Comb) {
            for (const auto& trigger : process.triggers) {
                const auto& name = compiled.design.slot(trigger.range.slot).name;
                EXPECT_TRUE(name.ends_with("input_value") || name.ends_with("arg_bias") ||
                            name.ends_with("hidden"))
                    << name;
                ++watched;
            }
        }
    EXPECT_EQ(watched, 3U);
    Capture output;
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "21\n21 100\n21\n111\n112\n");
}
TEST(ClassFront, ImplicitVirtualCallsExcludeReceiverAndOverrideBodyReads) {
    auto compiled = compile(R"(
class Box;
 int value=7;
 virtual function int read_value(int n);return value+n;endfunction
endclass
class Child extends Box;
 static int extra=3;
 function new();value=29;endfunction
 function int read_value(int n);
 repeat(300)begin automatic Box junk;junk=new;end
 return value+n+extra;
 endfunction
endclass
module top;
 Box base=new;Child child=new;
 bit select_child=1;int input_value=1,observed,calls=0;
 function automatic Box receiver();calls++;return select_child?child:base;endfunction
 always_comb observed=receiver().read_value(input_value);
 initial begin
 #1;$display("%0d %0d",observed,calls);
 Child::extra=10;select_child=0;
 #1;$display("%0d %0d",observed,calls);
 input_value=2;#1;$display("%0d %0d",observed,calls);
 end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (const auto& process : compiled.design.processes)
        if (process.kind == s::ProcessKind::Comb) {
            ASSERT_EQ(process.triggers.size(), 1U);
            EXPECT_TRUE(
                compiled.design.slot(process.triggers[0].range.slot).name.ends_with("input_value"));
        }
    Capture output;
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "33 1\n33 1\n9 2\n");
    EXPECT_LT(ctx.heap().objectStats().live, 305U);
}
TEST(ClassFront, ImplicitConstructionObservesActualsWithoutImportingConstructorReads) {
    auto compiled = compile(R"(
class Box;
 static int hidden=7;
 int value;
 function new(int n);value=n+hidden;endfunction
endclass
module top;
 Box object,saved;int input_value=1,hits=0;
 always_comb begin object=new(input_value);hits++;end
 initial begin
 #1;saved=object;$display("%0d %0d",object.value,hits);
 Box::hidden=20;#1;
 if(object!=saved)$fatal(1,"constructor body became sensitivity");
 $display("%0d %0d",object.value,hits);
 input_value=2;#1;
 if(object==saved)$fatal(1,"new did not allocate");
 $display("%0d %0d",object.value,hits);
 end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), "8 1\n8 1\n22 2\n");
}
TEST(ClassFront, ImplicitClassBodiesRetainUnsupportedObjectObserverBoundaries) {
    for (const auto* process :
         {"always_comb observed=(object==null);", "always @* observed=object.value;",
          "function int read_object();return object.value;endfunction wire driven=read_object();",
          "always_comb observed=object.read_value(other);"}) {
        const auto source =
            std::string(
                "class Box;int value=7;function int read_value(Box n);return n.value;endfunction "
                "endclass module top;Box object=new,other=new;int observed;") +
            process + "endmodule";
        const auto compiled = compile(source);
        EXPECT_FALSE(compiled.success) << process;
        EXPECT_TRUE(compiled.unsupported) << compiled.diagnostics << compiled.verifyErrors;
        EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    }
}
TEST(ClassFront, VMRejectsForgedImplicitClassHandleTriggersBeforeStorage) {
    auto compiled = compile(R"(
class Box;int value=7;endclass
module top;Box object=new;int input_value=1,observed;
always_comb observed=object.value+input_value;
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    unsigned changed = 0;
    for (auto& process : compiled.design.processes)
        if (process.kind == s::ProcessKind::Comb) {
            ASSERT_EQ(process.triggers.size(), 1U);
            process.triggers[0].range = {s::SlotId{slot(compiled.design, "object")}, 0, 0, 0, 0};
            ++changed;
        }
    ASSERT_EQ(changed, 1U);
    ASSERT_TRUE(s::verify(compiled.design).empty());
    k::Context ctx;
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    EXPECT_NE(result.error.find("observation"), std::string::npos);
    EXPECT_EQ(ctx.store().size(), 0U);
    EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
    EXPECT_EQ(ctx.activations().stats().live, 0U);
}
TEST(ClassFront, ImplicitNullMethodPreservesArgumentEffectsAndRuntimeError) {
    auto compiled = compile(R"(
class Box;function int read_value(int n);return n;endfunction endclass
module top;Box object;int input_value=1,observed,calls=0;
function int argument_value();calls++;return input_value;endfunction
always_comb observed=object.read_value(argument_value());
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    Capture output;
    k::Context ctx({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Error);
    EXPECT_EQ(*ctx.store().element(k::SlotId{slot(compiled.design, "calls")}, 0), 1U);
    EXPECT_NE(output.text().find("null class handle access"), std::string::npos);
    EXPECT_NE(output.text().find("design.sv:5: error:"), std::string::npos);
}
TEST(ClassFront, PlainClassPropertyReadHasNoImplicitPropertySensitivity) {
    auto compiled =
        compile("class Box;int data;endclass module top;Box a;int v;initial a=new;always_comb "
                "v=a.data;endmodule");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (const auto& process : compiled.design.processes)
        if (process.kind == s::ProcessKind::Comb) {
            EXPECT_TRUE(process.triggers.empty());
        }
    k::Context ctx;
    const auto result = kirimasim::vm::run(compiled.design, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(*ctx.store().element(k::SlotId{slot(compiled.design, "v")}, 0), 0U);
    EXPECT_EQ(ctx.heap().objectStats().live, 1U);
}
}  // namespace
