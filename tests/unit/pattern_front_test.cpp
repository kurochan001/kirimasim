// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"
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

namespace {
namespace f = kirimasim::front;
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto name = (std::filesystem::temp_directory_path() / "kirimasim-pattern-XXXXXX").string();
        if (mkdtemp(name.data()) == nullptr) return;
        directory = name;
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
        std::array<char, 1024> bytes{};
        while (const auto n = std::fread(bytes.data(), 1, bytes.size(), file))
            result.append(bytes.data(), n);
        return result;
    }
};
f::CompileResult compile(std::string_view text) {
    Source source(text);
    EXPECT_FALSE(source.file.empty());
    return f::compile(std::vector<std::string>{source.file});
}
void expectOutput(std::string_view source, std::string_view expected) {
    auto compiled = compile(source);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    EXPECT_FALSE(compiled.design.patterns.empty());
    Capture output;
    ASSERT_NE(output.file, nullptr);
    k::Context context({output.file, output.file});
    const auto result = kirimasim::vm::run(compiled.design, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(output.text(), expected);
}

TEST(PatternFront, WideEnumerationAndPackedMemberWindows) {
    expectOutput(R"(
module top;
 typedef enum logic[128:0] { HIGH=(129'(1)<<128)|1, LOW=2 } mode_t;
 struct packed { mode_t mode; byte count; logic[2:0] state; } value;
 initial begin
  value.mode=HIGH; value.count=-3; value.state=3'bzzz;
  $display("%p|%0p",value,value);
  value.mode=mode_t'('x); $display("%p",value.mode);
 end
endmodule
)",
                 "'{mode:HIGH, count:-3, state:z}|'{HIGH, -3, z}\nx\n");
}
TEST(PatternFront, FixedArraysAndRecordsRetainDeclaredOrderAndTypes) {
    expectOutput(R"(
module top;
 typedef struct { int code; string name; } record_t;
 record_t value;
 int matrix[2:1][-1:0]='{'{1,2},'{3,4}};
 initial begin value.code=-7;value.name="a b";
  $display("%p|%0p|%p",value,value,matrix);
 end
endmodule
)",
                 "'{code:-7, name:\"a b\"}|'{-7, \"a b\"}|'{'{1, 2}, '{3, 4}}\n");
}
TEST(PatternFront, RuntimeFormatsSelectPresentationsAndKeepRawScalarValues) {
    expectOutput(R"(
module top;
 struct packed { byte code; } value;
 string format,text;
 initial begin value.code=7;format="%p|%0p|%s|%0d|%.1f";
  text=$sformatf(format,value,value,"raw",5,1.5);
  $display("%s",text);
 end
endmodule
)",
                 "'{code:7}|'{7}|raw|5|1.5\n");
}
TEST(PatternFront, EveryArgumentIsEvaluatedOnceBeforeLaterMutation) {
    expectOutput(R"(
module top;
 int values[$]='{1,2}; int calls=0; string format,text;
 function automatic int change();
  calls++;values[0]=9;values.push_back(3);return 42;
 endfunction
 initial begin
  $display("%p|%0d|%p",values,change(),values);
  values='{1,2};format="%p|%0d|%0p";
  text=$sformatf(format,values,change(),values);
  $display("%s|%0d",text,calls);
 end
endmodule
)",
                 "'{1, 2}|42|'{9, 2, 3}\n'{1, 2}|42|'{9, 2, 3}|2\n");
}
TEST(PatternFront, PostponedBodiesObserveHeapChangesAndReplacements) {
    expectOutput(R"(
module top;
 int values[$]='{1};
 initial begin
  $strobe("S%p",values);values[0]=2;
  #1;$monitor("M%p",values);
  #1;values[0]=3;#1;values='{9,10};#1;$monitoroff;
 end
endmodule
)",
                 "S'{2}\nM'{2}\nM'{3}\nM'{9, 10}\n");
}
TEST(PatternFront, MonitorComparesFourStateValuesAndIgnoresTimeAlone) {
    expectOutput(R"(
module top;
 logic[1:0] values[$]='{2'bxx};int wake=0;
 initial begin
  $monitor("%p|%0d|%0d",values,wake%2,$time);
  #1;wake=2;#1;values[0]=2'bxz;#1;wake=4;#1;values[0]=2'bzx;#1;$monitoroff;
 end
endmodule
)",
                 "'{x}|0|0\n'{X}|0|2\n'{X}|0|4\n");
}
TEST(PatternFront, AggregateReturnsAndRefArgumentsAreCapturedOnce) {
    expectOutput(R"(
module top;
 typedef int array_t[2:1];int calls=0;array_t a='{7,8};
 function automatic array_t make();calls++;return '{3,4};endfunction
 task automatic show(ref array_t x);$display("%p|%p",x,make());endtask
 initial begin show(a);$display("%0d",calls);end
endmodule
)",
                 "'{7, 8}|'{3, 4}\n1\n");
}
TEST(PatternFront, EscapedNamesArePresentationMetadata) {
    expectOutput(R"(
module top;
 struct packed { byte \module ; byte \a.b ; } value;
 initial begin value=16'h0102;$display("%p|%0p",value,value);end
endmodule
)",
                 "'{\\module :1, \\a.b :2}|'{1, 2}\n");
}
TEST(PatternFront, ScalarPatternsCoexistWithObjectsAndOrdinaryMonitors) {
    expectOutput(R"(
class item;int value;endclass
module top;
 item object;int value=5;
 initial begin object=new;$monitor("%p",value);
  #1;object.value=3;#1;value=7;#1;$monitoroff;
 end
endmodule
)",
                 "5\n7\n");
}
TEST(PatternFront, RuntimeMetadataRejectsMalformedNativeDesigns) {
    auto compiled = compile(R"(
module top;
 string format,text;int value=7;
 initial begin format="%p";text=$sformatf(format,value);$display("%s",text);end
endmodule
)");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (unsigned fault = 0; fault < 6; ++fault) {
        auto design = compiled.design;
        s::Instr* call = nullptr;
        for (auto& process : design.processes)
            for (auto& block : process.blocks)
                for (auto& in : block.instrs)
                    if (in.op == s::Op::Syscall && !in.operands.empty() &&
                        in.operands[0].text == "$sformatf.runtime.pattern")
                        call = &in;
        ASSERT_NE(call, nullptr);
        ASSERT_EQ(call->operands.size(), 9U);
        switch (fault) {
            case 0: call->operands.pop_back(); break;
            case 1: call->operands[5] = s::Operand::str("not-empty"); break;
            case 2: call->operands[5] = s::Operand::pattern(s::PatternId{99999}); break;
            case 3: call->operands[6] = s::Operand::str(""); break;
            case 4: call->operands[7] = call->operands[6]; break;
            case 5: call->operands[8] = call->operands[6]; break;
            default: FAIL() << "unregistered metadata fault " << fault;
        }
        EXPECT_FALSE(s::verify(design).empty()) << fault;
    }
}
TEST(PatternFront, ScalarPatternCallsAreSingleEvaluations) {
    expectOutput(R"(
module top;
 int calls=0;string format,text;
 function automatic int next_value();calls++;return calls;endfunction
 initial begin
  $display("%p|%0d",next_value(),calls);
  format="%p|%0d";text=$sformatf(format,next_value(),calls);
  $display("%s|%0d",text,calls);
 end
endmodule
)",
                 "1|1\n2|2|2\n");
}
TEST(PatternFront, NestedContainersUseTheirCurrentShape) {
    expectOutput(R"(
module top;
 typedef int inner_t[$];inner_t nested[$];inner_t a,b;string format,text;
 initial begin
  a='{1,2};b='{3};nested.push_back(a);nested.push_back(b);
  $display("%p",nested);
  nested[0][0]=4;format="%0p";text=$sformatf(format,nested);
  $display("%s",text);
 end
endmodule
)",
                 "'{'{1, 2}, '{3}}\n'{'{4, 2}, '{3}}\n");
}
TEST(PatternFront, SourceBridgeDoesNotTruncateLongPatterns) {
    std::string pattern = "'{";
    for (unsigned i = 0; i < 1200; ++i) {
        if (i) pattern += ", ";
        pattern += '1';
    }
    pattern += '}';
    expectOutput(R"(
module top;
 int values[1200]='{default:1};string format,text;
 initial begin
  $display("%p",values);format="%0p";text=$sformatf(format,values);
  $display("%s",text);
 end
endmodule
)",
                 pattern + "\n" + pattern + "\n");
}
TEST(PatternFront, RuntimeKernelRequiresTypedPresentationsAndPreservesRawValues) {
    const std::uint64_t word = 7;
    k::FormatValue value;
    value.integral = {&word, 32, false, false};
    value.hasPattern = true;
    value.namedPattern = "FIRST";
    value.positionalPattern = "FIRST";
    const std::array args{value, value, value};
    const auto result = k::formatStringCall("%p|%0p|%0d", args, "", "", 1, -9, {});
    ASSERT_TRUE(result.error.empty()) << result.error;
    EXPECT_EQ(result.text, "FIRST|FIRST|7");
    value.kind = k::FormatValue::Kind::Pattern;
    EXPECT_FALSE(k::formatStringCall("%0d", std::span(&value, 1), "", "", 1, -9, {}).error.empty());
    value.hasPattern = false;
    EXPECT_FALSE(k::formatStringCall("%p", std::span(&value, 1), "", "", 1, -9, {}).error.empty());
}
TEST(PatternFront, CollectionsOfRecordsAndRecordsOfCollections) {
    expectOutput(R"(
module top;
 typedef struct packed { byte code; } item_t;
 typedef struct { int id;int values[$];string label; } record_t;
 item_t items[$];item_t item;record_t record_value;string format,text;
 initial begin
  item.code=-3;items.push_back(item);
  record_value.id=7;record_value.values='{1,2};record_value.label="tag";
  format="%p";text=$sformatf(format,record_value);
  $display("%p|%s",items,text);
 end
endmodule
)",
                 "'{'{code:-3}}|'{id:7, values:'{1, 2}, label:\"tag\"}\n");
}
TEST(PatternFront, UnsupportedHandlePresentationKeepsAPositionedBoundary) {
    const auto result = compile(R"(
class item;int value;endclass
module top;
 item object;
 initial begin object=new;$display("%p",object);end
endmodule
)");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.unsupported);
    EXPECT_TRUE(result.verifyErrors.empty());
    EXPECT_NE(result.diagnostics.find("assignment-pattern formatting of this type"),
              std::string::npos);
    EXPECT_NE(result.diagnostics.find(":5:"), std::string::npos);
}
TEST(PatternFront, ActiveObjectFieldsFormatWithoutEnablingObjectObservation) {
    expectOutput(R"(
class item;
 int value=7;
 function string render();return $sformatf("%p",value);endfunction
endclass
module top;
 item object;
 initial begin object=new;$display("%p|%s",object.value,object.render());end
endmodule
)",
                 "7|7\n");
    const auto result = compile(R"(
class item;int value;endclass
module top;
 item object;
 function int read_value();return object.value;endfunction
 initial begin object=new;$monitor("%p",read_value());end
endmodule
)");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.unsupported);
    EXPECT_TRUE(result.verifyErrors.empty());
}
TEST(PatternFront, DefaultEmptyAndShrunkCollectionsHaveEmptyPatterns) {
    expectOutput(R"(
module top;
 int values[],queue_values[$];string format,text;
 initial begin
  $display("%p|%0p",values,queue_values);
  values=new[2];values[0]=7;queue_values.push_back(8);
  values=new[0];queue_values.delete();format="%p|%0p";
  text=$sformatf(format,values,queue_values);$display("%s",text);
 end
endmodule
)",
                 "'{}|'{}\n'{}|'{}\n");
}
TEST(PatternFront, ImplicitStringsKeepTheirRawIntegralValues) {
    expectOutput(R"(
module top;
 logic[23:0] packed_value="a b";string format,text;
 initial begin
  $display("%p|%p|%0d","a b",string'("a b"),"a b");
  format="%p|%0d|%p|%p";
  text=$sformatf(format,"a b","a b",{"a","b"},int'("ab"));
  $display("%s|%p",text,packed_value);
  $display("%p|%p","A\000B",string'("A\000B"));
 end
endmodule
)",
                 "\"a b\"|\"a b\"|6365282\n\"a b\"|6365282|\"ab\"|24930|6365282\n\"AB\"|\"AB\"\n");
}
TEST(PatternFront, ImplicitStringMonitorsCompareTheOriginalPackedValue) {
    expectOutput(R"(
module top;
 logic[15:0] bytes_value=16'h0042;
 initial begin
  $monitor("%p",{"A",bytes_value});
  #1;bytes_value=16'h4200;#1;$monitoroff;
 end
endmodule
)",
                 "\"AB\"\n\"AB\"\n");
}
}  // namespace
