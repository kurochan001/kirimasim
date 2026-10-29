// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Independent source oracles and ownership checks for IEEE 1800 20.7.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace front = kirimasim::front;
namespace k = kirimasim::kernel;
namespace vm = kirimasim::vm;

struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-runtime-query-XXXXXX").string();
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
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file != nullptr) std::fclose(file);
    }
    std::string text() const {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string out;
        std::array<char, 512> buffer{};
        while (const auto count = std::fread(buffer.data(), 1, buffer.size(), file))
            out.append(buffer.data(), count);
        return out;
    }
};
front::CompileResult compileDetached(std::string_view text) {
    // No source file or frontend AST survives into execution.
    const Source source(text);
    EXPECT_FALSE(source.file.empty());
    if (source.file.empty()) return {};
    return front::compile(std::vector<std::string>{source.file});
}
struct Execution {
    vm::RunResult result;
    std::string output, diagnostics;
    std::uint64_t time = 0, liveActivations = 0, leases = 0;
    k::HeapStats heap;
};
Execution execute(const kirimasim::sir::Design& design) {
    Capture out, err;
    EXPECT_NE(out.file, nullptr);
    EXPECT_NE(err.file, nullptr);
    if (out.file == nullptr || err.file == nullptr)
        return {{k::EndReason::Internal, "cannot capture output"}, {}, {}, 0, 0, 0, {}};
    k::Context context(k::OutputDestinations{out.file, err.file});
    auto result = vm::run(design, context);
    context.output().flush();
    const auto activations = context.activations().stats();
    return {std::move(result),          out.text(),       err.text(),
            context.scheduler().time(), activations.live, activations.leases,
            context.heap().stats()};
}
void expectSuccess(const Execution& execution) {
    EXPECT_EQ(execution.result.reason, k::EndReason::Quiescent)
        << execution.result.error << execution.diagnostics;
    EXPECT_TRUE(execution.result.error.empty()) << execution.result.error;
    EXPECT_TRUE(execution.diagnostics.empty()) << execution.diagnostics;
    EXPECT_EQ(execution.liveActivations, 0U);
    EXPECT_EQ(execution.leases, 0U);
}
std::string tuple(std::int64_t left, std::int64_t right) {
    const auto low = left < right ? left : right;
    const auto high = left > right ? left : right;
    return std::to_string(left) + ':' + std::to_string(right) + ':' + std::to_string(low) + ':' +
           std::to_string(high) + ':' + std::to_string(high - low + 1) + ':' +
           std::to_string(left >= right ? 1 : -1) + '\n';
}
std::string containerTuple(std::int64_t count) {
    return "0:" + std::to_string(count - 1) + ":0:" + std::to_string(count - 1) + ':' +
           std::to_string(count) + ":-1\n";
}

TEST(RuntimeArrayQuery, FixedAndPackedDimensionsFollowDeclaredBoundsAfterSourceDestruction) {
    const auto compiled = compileDetached(R"sv(module top;
  logic [2:-1][0:2] data[-2:1][7:5];
  task automatic show(input int dimension);
    $display("%0d:%0d:%0d:%0d:%0d:%0d", $left(data,dimension), $right(data,dimension),
      $low(data,dimension), $high(data,dimension), $size(data,dimension),
      $increment(data,dimension));
  endtask
  initial begin
    $display("%0d:%0d", $dimensions(data), $unpacked_dimensions(data));
    for (int d=1; d<=4; d++) show(d);
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    const std::string expected = "4:2\n" + tuple(-2, 1) + tuple(7, 5) + tuple(2, -1) + tuple(0, 2);
    EXPECT_EQ(execution.output, expected);
    EXPECT_EQ(execution.time, 0U);
}

TEST(RuntimeArrayQuery, FixedValueExpressionsStayUnevaluatedWhileEachDimensionRunsOnce) {
    const auto compiled = compileDetached(R"sv(module top;
  typedef logic [8:3] word_t;
  word_t data[5:2];
  int value_calls, dimension_calls, index=2;
  function automatic word_t value();
    value_calls++;
    return '1;
  endfunction
  function automatic int choose(input int dimension);
    dimension_calls++;
    return dimension;
  endfunction
  task automatic inspect(input int dimension);
    integer left_bound, right_bound;
    left_bound=$left(value(),choose(dimension));
    right_bound=$right(data[index++],choose(dimension));
    if (dimension==1) begin
      if (left_bound!==8 || right_bound!==3) $fatal(1,"fixed query bounds");
    end else if (left_bound!=='x || right_bound!=='x) $fatal(1,"invalid fixed dimension");
    if ($bits(value())!=6 || $size(value())!=6 || $dimensions(value())!=1 ||
        $unpacked_dimensions(value())!=0) $fatal(1,"fixed type-only queries");
    if (value_calls!=0 || index!=2) $fatal(1,"fixed source expression was evaluated");
  endtask
  initial begin
    inspect(1); inspect(2);
    $display("%0d:%0d:%0d",value_calls,dimension_calls,index);
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    EXPECT_EQ(execution.output, "0:4:2\n");
}

TEST(RuntimeArrayQuery, RuntimeDimensionsUseCurrentContainerSizesAndStaticPackedRanges) {
    const auto compiled = compileDetached(R"sv(module top;
  logic [3:-2] data[];
  logic [0:4] queue[$];
  task automatic show(input int dimension);
    $display("%0d:%0d:%0d:%0d:%0d:%0d", $left(data,dimension), $right(data,dimension),
      $low(data,dimension), $high(data,dimension), $size(data,dimension),
      $increment(data,dimension));
    $display("%0d:%0d:%0d:%0d:%0d:%0d", $left(queue,dimension), $right(queue,dimension),
      $low(queue,dimension), $high(queue,dimension), $size(queue,dimension),
      $increment(queue,dimension));
  endtask
  initial begin
    if ($dimensions(data)!=2 || $unpacked_dimensions(data)!=1 ||
        $dimensions(queue)!=2 || $unpacked_dimensions(queue)!=1) $fatal(1,"dimension counts");
    show(1); show(2);
    data=new[3]; queue.push_back('0); queue.push_back('1);
    show(1); show(2);
    data=new[1](data); queue.delete();
    show(1); show(2);
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    const auto packed = tuple(3, -2) + tuple(0, 4);
    EXPECT_EQ(execution.output, containerTuple(0) + containerTuple(0) + packed + containerTuple(3) +
                                    containerTuple(2) + packed + containerTuple(1) +
                                    containerTuple(0) + packed);
}

TEST(RuntimeArrayQuery, ASelectedDynamicRowIsEvaluatedOnceOnlyForTheSelectedRuntimeDimension) {
    for (const bool dynamicOuter : {false, true}) {
        SCOPED_TRACE(dynamicOuter ? "dynamic outer" : "fixed outer");
        std::string source = "module top;\n";
        source += dynamicOuter ? "  logic [7:4] rows[][];\n" : "  logic [7:4] rows[2][];\n";
        source += R"sv(  int selections, dimensions;
  function automatic int next_row();
    int result;
    result=selections%2;
    selections++;
    return result;
  endfunction
  function automatic int choose(input int d);
    dimensions++;
    return d;
  endfunction
  task automatic inspect(input int dimension);
    if (dimension==1) begin
      if ($size(rows[next_row()],choose(dimension))!=3) $fatal(1,"row size");
      if ($right(rows[next_row()],choose(dimension))!=4) $fatal(1,"row right");
      if ($left(rows[next_row()],choose(dimension))!=0) $fatal(1,"row left");
    end else if (dimension==2) begin
      if ($size(rows[next_row()],choose(dimension))!=4) $fatal(1,"packed size");
      if ($right(rows[next_row()],choose(dimension))!=4) $fatal(1,"packed right");
      if ($left(rows[next_row()],choose(dimension))!=7) $fatal(1,"packed left");
    end else if ($size(rows[next_row()],choose(dimension))!=='x)
      $fatal(1,"invalid dimension");
  endtask
  initial begin
)sv";
        if (dynamicOuter) source += "    rows=new[2];\n";
        source += R"sv(    rows[0]=new[3]; rows[1]=new[5];
    inspect(1); inspect(2); inspect(0);
    if (selections!=3 || dimensions!=7) $fatal(1,"query expression evaluation count");
    $display("%0d:%0d",selections,dimensions);
  end
endmodule
)sv";
        const auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        const auto execution = execute(compiled.design);
        expectSuccess(execution);
        EXPECT_EQ(execution.output, "3:7\n");
    }
}

TEST(RuntimeArrayQuery, InvalidRuntimeDimensionsKeepHighBitsAndUnknownStates) {
    const auto compiled = compileDetached(R"sv(module top;
  logic [8:3] data[2:-1];
  task automatic invalid(input logic [128:0] dimension);
    if ($left(data,dimension)!=='x || $right(data,dimension)!=='x ||
        $low(data,dimension)!=='x || $high(data,dimension)!=='x ||
        $size(data,dimension)!=='x || $increment(data,dimension)!=='x)
      $fatal(1,"invalid dimension was narrowed or lost its unknown bits");
  endtask
  task automatic valid(input logic [128:0] dimension, input int expected);
    if ($size(data,dimension)!==expected) $fatal(1,"valid wide dimension");
  endtask
  initial begin
    logic [128:0] dimension;
    invalid(0); invalid('1);
    invalid((129'd1<<64)|129'd1);
    invalid((129'd1<<128)|129'd1);
    dimension=1; dimension[128]=1'bx; invalid(dimension);
    dimension=1; dimension[64]=1'bz; invalid(dimension);
    dimension=1; dimension[0]=1'bx; invalid(dimension);
    valid(1,4); valid(2,6);
    $display("seven invalid, two valid");
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    EXPECT_EQ(execution.output, "seven invalid, two valid\n");
}

TEST(RuntimeArrayQuery, StringQueriesUseCurrentLengthsAndEvaluateOnlyDimensionOneSources) {
    const auto compiled = compileDetached(R"sv(module top;
  string text;
  int calls;
  function automatic string current();
    calls++;
    return text;
  endfunction
  task automatic show(input int dimension);
    $display("%0d:%0d:%0d:%0d:%0d:%0d", $left(current(),dimension),
      $right(current(),dimension), $low(current(),dimension), $high(current(),dimension),
      $size(current(),dimension), $increment(current(),dimension));
  endtask
  task automatic invalid(input int dimension);
    if ($size(current(),dimension)!=='x) $fatal(1,"invalid string dimension");
  endtask
  initial begin
    if ($dimensions(current())!=1 || $unpacked_dimensions(current())!=0)
      $fatal(1,"string dimension counts");
    show(1);
    text="abcde"; show(1);
    text="x"; show(1);
    invalid(0); invalid(2);
    if (calls!=18) $fatal(1,"string source evaluation count");
    $display("%0d",calls);
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    EXPECT_EQ(execution.output, containerTuple(0) + containerTuple(5) + containerTuple(1) + "18\n");
}

TEST(RuntimeArrayQuery, ConcurrentAutomaticRefsRetainCurrentQueriesAcrossWaitsAndCollection) {
    const auto compiled = compileDetached(R"sv(module top;
  int completed;
  task automatic inspect(ref int values[], input int expected, input int tag);
    int dimension=1;
    #2;
    if ($size(values,dimension)!=expected || values[0]!=tag)
      $fatal(1,"ref query lost across wait");
    values=new[expected+1](values);
    #1;
    if ($right(values,dimension)!=expected || values[0]!=tag)
      $fatal(1,"query did not observe the ref's current container");
  endtask
  task automatic owner(input int tag, input int count);
    int values[];
    values=new[count]; values[0]=tag;
    fork
      inspect(values,count,tag);
      begin
        #1;
        if ($size(values,1)!=count) $fatal(1,"captured source container");
      end
    join
    if ($size(values,1)!=count+1) $fatal(1,"ref replacement did not reach owner");
    completed++;
  endtask
  initial begin
    fork
      owner(11,3);
      owner(29,6);
      begin
        int scratch[];
        for (int n=0; n<3000; n++) begin
          scratch=new[3]; scratch[0]=n; #0;
        end
      end
    join
    if (completed!=2) $fatal(1,"automatic call completion count");
    $display("passed");
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    EXPECT_EQ(execution.output, "passed\n");
    EXPECT_EQ(execution.time, 3U);
    EXPECT_LT(execution.heap.containerEntries, 3000U);
}

TEST(RuntimeArrayQuery, NestedDynamicLanguageErrorsAndRuntimeRepresentationBoundaryRemainDistinct) {
    for (const auto body : {"int data[2][]; initial $display(\"%0d\",$size(data,2));",
                            "int data[][]; initial $display(\"%0d\",$left(data,2));"}) {
        const Source source(std::string("module top; ") + body + " endmodule\n");
        ASSERT_FALSE(source.file.empty());
        const std::vector<std::string> args{source.file};
        const auto parsed = front::parseAndElaborate(args);
        const auto compiled = front::compile(args);
        EXPECT_FALSE(parsed.success);
        EXPECT_GT(parsed.numErrors, 0U);
        EXPECT_FALSE(compiled.success);
        EXPECT_FALSE(compiled.unsupported);
        EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
        EXPECT_EQ(compiled.diagnostics, parsed.diagnostics);
    }
    const Source source(R"sv(module top;
  int data[2][];
  task automatic query(input int dimension);
    $display("%0d",$size(data,dimension));
  endtask
  initial query(1);
endmodule
)sv");
    ASSERT_FALSE(source.file.empty());
    const std::vector<std::string> args{source.file};
    const auto parsed = front::parseAndElaborate(args);
    ASSERT_TRUE(parsed.success) << parsed.diagnostics;
    const auto compiled = front::compile(args);
    EXPECT_FALSE(compiled.success);
    EXPECT_TRUE(compiled.unsupported);
    EXPECT_GT(compiled.numErrors, 0U);
    EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
}
}  // namespace
