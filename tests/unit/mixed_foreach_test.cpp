// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Independent source equivalence and ownership checks for IEEE 1800 12.7.3.
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
            (std::filesystem::temp_directory_path() / "kirimasim-mixed-foreach-XXXXXX").string();
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
    // Neither the source file nor the frontend's AST survives into execution.
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

TEST(MixedForeach, DynamicOuterAndPackedDimensionsMatchHandwrittenLoopsAfterSourceDestruction) {
    std::string expected;
    for (int i = 0; i < 2; ++i)
        for (int j = 2; j >= -1; --j)
            for (int bit = 1; bit >= 0; --bit)
                expected +=
                    std::to_string(i) + ':' + std::to_string(j) + ':' + std::to_string(bit) + "=1;";
    expected += "end\n";
    std::string baseline;
    for (const bool mixed : {false, true}) {
        SCOPED_TRACE(mixed ? "foreach" : "handwritten nested for");
        std::string source = R"sv(module top;
  logic [2:-1][1:0] a[];
  initial begin
    a = new[2];
    for (int index = 0; index < 2; index++) a[index] = '1;
)sv";
        source += mixed ? "    foreach (a[i,j,bit_index]) begin\n"
                        : "    for (int i=0; i<2; i++) for (int j=2; j>=-1; j--) "
                          "for (int bit_index=1; bit_index>=0; bit_index--) begin\n";
        source += R"sv(      $write("%0d:%0d:%0d=%0d;", i, j, bit_index, a[i][j][bit_index]);
      #0;
    end
    $display("end");
  end
endmodule
)sv";
        const auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        ASSERT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
        const auto execution = execute(compiled.design);
        expectSuccess(execution);
        EXPECT_EQ(execution.output, expected);
        EXPECT_EQ(execution.time, 0U);
        if (mixed)
            EXPECT_EQ(execution.output, baseline);
        else
            baseline = execution.output;
    }
}

TEST(MixedForeach, BreakAndContinueHaveTheSameScopeAsAnIndependentNamedLoopOracle) {
    std::string baseline;
    for (const bool mixed : {false, true}) {
        SCOPED_TRACE(mixed ? "foreach" : "nested for with named exit");
        std::string source = R"sv(module top;
  int a[2:-1][];
  int inner;
  initial begin
    a[2]=new[2]; a[1]=new[0]; a[0]=new[3]; a[-1]=new[1];
    for (int pass=0; pass<2; pass++) begin
      begin : visit
)sv";
        source += mixed ? "        foreach (a[i,j]) begin\n"
                        : "        for (int i=2; i>=-1; i--) "
                          "for (int j=0; j<a[i].size(); j++) begin\n";
        source += "          if (j==1) continue;\n          if (i==0 && j==2) ";
        source += mixed ? "break;\n" : "disable visit;\n";
        source += R"sv(          inner=0;
          while (inner<3) begin
            inner++;
            if (inner==2) break;
          end
          $write("%0d:%0d:%0d:%0d;", pass, i, j, inner);
          #1;
        end
      end
      $write("|");
    end
    $display("end");
  end
endmodule
)sv";
        const auto compiled = compileDetached(source);
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        ASSERT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
        const auto execution = execute(compiled.design);
        expectSuccess(execution);
        EXPECT_EQ(execution.output, "0:2:0:2;0:0:0:2;|1:2:0:2;1:0:0:2;|end\n");
        EXPECT_EQ(execution.time, 4U);
        if (mixed)
            EXPECT_EQ(execution.output, baseline);
        else
            baseline = execution.output;
    }
}

TEST(MixedForeach, ConcurrentAutomaticBoundsAndCapturedContainerRowsSurviveWaitsAndCollection) {
    const auto compiled = compileDetached(R"sv(module top;
  int completed;
  task automatic walk(input int tag, input int count);
    int a[1:0][];
    int seen;
    a[1]=new[count]; a[0]=new[count+1];
    for (int row=1; row>=0; row--)
      for (int item=0; item<a[row].size(); item++)
        a[row][item]=tag*100+row*10+item;
    fork
      begin
        #0;
        if (a[0][0] != tag*100) $fatal(1, "captured container row changed");
      end
    join
    seen=0;
    foreach (a[i,j]) begin
      #1;
      if (a[i][j] != tag*100+i*10+j) $fatal(1, "container lost across wait");
      seen++;
    end
    if (seen != count*2+1) $fatal(1, "per-call bounds were overwritten");
    completed++;
  endtask
  initial begin
    fork
      walk(3,2);
      walk(7,4);
      begin
        int scratch[];
        for (int n=0; n<3000; n++) begin
          scratch=new[3];
          scratch[0]=n;
          #0;
        end
      end
    join
    if (completed != 2) $fatal(1, "task completion count");
    $display("passed");
  end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    const auto execution = execute(compiled.design);
    expectSuccess(execution);
    EXPECT_EQ(execution.output, "passed\n");
    EXPECT_EQ(execution.time, 9U);
    // Thousands of abandoned constructions must have reused collected entries.
    EXPECT_LT(execution.heap.containerEntries, 3000U);
}

TEST(MixedForeach, SkippedLeadingDimensionAndLoopVariableWritesRemainLanguageErrors) {
    for (const auto body : {"foreach (a[,j]) $display(\"%0d\", j);", "foreach (a[i,j]) i++;"}) {
        const Source source(std::string("module top; int a[2][]; initial ") + body +
                            " endmodule\n");
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
}

TEST(MixedForeach, AFixedUnpackedHeapElementRemainsOutsideTheExistingRepresentation) {
    const auto compiled = compileDetached(R"sv(module top;
  typedef int row_t[2];
  row_t a[];
  initial foreach (a[i,j]) $display("%0d:%0d", i, j);
endmodule
)sv");
    EXPECT_FALSE(compiled.success);
    EXPECT_TRUE(compiled.unsupported);
    EXPECT_GT(compiled.numErrors, 0U);
    EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    EXPECT_NE(compiled.diagnostics.find("unsupported: type '"), std::string::npos)
        << compiled.diagnostics;
}
}  // namespace
