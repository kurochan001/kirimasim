// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Source-level checks for VALUE_SEMANTICS 3.9 and IEEE 1800-2023 11.4.11.
// These exercise front's captures as well as the shared VM operations.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <unistd.h>

#include <gtest/gtest.h>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;

struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "kirimasim-array-conditional-XXXXXX")
                .string();
        if (mkdtemp(pattern.data()) == nullptr) return;
        directory = pattern;
        const auto path = directory / "design.sv";
        std::ofstream stream(path);
        stream << text;
        stream.close();
        if (stream) file = path.string();
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
        if (file) std::fclose(file);
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

void expectRun(std::string_view source) {
    const Source input(source);
    ASSERT_FALSE(input.file.empty());
    for (const bool instrument : {false, true}) {
        SCOPED_TRACE(instrument ? "instrumented conditions" : "ordinary conditions");
        auto compiled =
            kirimasim::front::compile(std::vector<std::string>{input.file}, {.condX = instrument});
        ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
        ASSERT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
        Capture out, err;
        ASSERT_NE(out.file, nullptr);
        ASSERT_NE(err.file, nullptr);
        k::Context context(k::OutputDestinations{out.file, err.file});
        const auto result = kirimasim::vm::run(compiled.design, context);
        context.output().flush();
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error << err.text();
        EXPECT_TRUE(result.error.empty()) << result.error;
        EXPECT_EQ(out.text(), "passed\n");
        EXPECT_TRUE(err.text().empty()) << err.text();
    }
}

TEST(FixedArrayConditional, AmbiguousOperandsRunOnceInOrderAndRetainTheLeftSnapshot) {
    expectRun(R"sv(
module top;
  typedef int row_t[2];
  row_t left_storage, right_storage, result;
  logic choose;
  int trace, left_calls, right_calls;
  function automatic row_t left_value();
    trace = trace * 10 + 1;
    left_calls++;
    return left_storage;
  endfunction
  function automatic row_t right_value();
    trace = trace * 10 + 2;
    right_calls++;
    left_storage[0] = 99;
    return right_storage;
  endfunction
  initial begin
    left_storage = '{7, 2}; right_storage = '{7, 3};
    choose = 1'bx;
    result = choose ? left_value() : right_value();
    if (trace != 12 || left_calls != 1 || right_calls != 1)
      $fatal(1, "ambiguous operand order or cardinality");
    if (left_storage[0] != 99 || result[0] != 7 || result[1] != 0)
      $fatal(1, "left snapshot must precede the right operand effect");
    left_storage = '{88, 88}; right_storage = '{77, 77};
    if (result[0] != 7 || result[1] != 0)
      $fatal(1, "the result must own its element values");
    $display("passed");
  end
endmodule
)sv");
}

TEST(FixedArrayConditional, WideConditionsEvaluateOnceAndKnownOneSkipsTheRightOperand) {
    expectRun(R"sv(
module top;
  typedef int row_t[2];
  logic [128:0] choose;
  row_t result;
  int trace;
  function automatic logic [128:0] predicate();
    trace = trace * 10 + 9;
    return choose;
  endfunction
  function automatic row_t left_value();
    trace = trace * 10 + 1;
    return '{11, 12};
  endfunction
  function automatic row_t right_value();
    trace = trace * 10 + 2;
    return '{11, 13};
  endfunction
  initial begin
    choose = '0; choose[128] = 1'bx; choose[64] = 1'bz;
    result = predicate() ? left_value() : right_value();
    if (trace != 912 || result[0] != 11 || result[1] != 0)
      $fatal(1, "wide X/Z condition must evaluate both operands");
    trace = 0; choose[0] = 1'b1;
    result = predicate() ? left_value() : right_value();
    if (trace != 91 || result[0] != 11 || result[1] != 12)
      $fatal(1, "a known low one makes the wide condition true");
    trace = 0; choose = '0; choose[128] = 1'b1; choose[0] = 1'bz;
    result = predicate() ? left_value() : right_value();
    if (trace != 91 || result[1] != 12)
      $fatal(1, "a known high one makes the wide condition true");
    trace = 0; choose = '0;
    result = predicate() ? left_value() : right_value();
    if (trace != 92 || result[0] != 11 || result[1] != 13)
      $fatal(1, "a zero condition evaluates only the right operand");
    $display("passed");
  end
endmodule
)sv");
}

TEST(FixedArrayConditional, WideElementsAndMatchingUnknownBitsUseWholeElementDefaults) {
    expectRun(R"sv(
module top;
  logic choose;
  logic [128:0] left_values[3], right_values[3], result[3];
  initial begin
    left_values = '{129'h100000000000000000000000000000001,
                    129'h100000000000000000000000000000002, 'z};
    right_values = '{129'h100000000000000000000000000000001,
                     129'h100000000000000000000000000000003, 'z};
    choose = 1'bz;
    result = choose ? left_values : right_values;
    if (result[0] !== left_values[0] || result[1] !== {129{1'bx}})
      $fatal(1, "a one-bit difference defaults the entire wide element");
    if (result[2] !== {129{1'bx}})
      $fatal(1, "matching Z bits do not establish equality by ==");
    left_values[2] = 'x; right_values[2] = 'x;
    choose = 1'bx;
    result = choose ? left_values : right_values;
    if (result[0] !== left_values[0] || result[1] !== 'x || result[2] !== 'x)
      $fatal(1, "the X predicate follows the same default rule");
    $display("passed");
  end
endmodule
)sv");
}

TEST(FixedArrayConditional, RealEqualityHandlesNaNSignedZeroAndInfinity) {
    expectRun(R"sv(
module top;
  logic choose;
  real left_values[5], right_values[5], result[5];
  initial begin
    left_values = '{$bitstoreal(64'h7ff8000000000001),
                   $bitstoreal(64'h8000000000000000),
                   $bitstoreal(64'h7ff0000000000000), 1.25, 2.5};
    right_values = '{$bitstoreal(64'h7ff8000000000001), 0.0,
                    $bitstoreal(64'h7ff0000000000000), 1.25, 3.5};
    choose = 1'bx;
    result = choose ? left_values : right_values;
    if ($realtobits(result[0]) !== 64'h0)
      $fatal(1, "NaN is unequal even to the same NaN payload");
    if ($realtobits(result[1]) !== 64'h8000000000000000)
      $fatal(1, "equal signed zeros retain the left value");
    if ($realtobits(result[2]) !== 64'h7ff0000000000000)
      $fatal(1, "equal infinities retain their value");
    if (result[3] != 1.25 || $realtobits(result[4]) !== 64'h0)
      $fatal(1, "equal and unequal finite real elements");
    $display("passed");
  end
endmodule
)sv");
}

TEST(FixedArrayConditional, NestedRecordDefaultsRetainDeclaredMemberInitializersAndNullHandles) {
    expectRun(R"sv(
module top;
  typedef struct { bit code = 1'b1; logic [3:0] word = 4'ha; } inner_t;
  typedef struct {
    bit [7:0] tag = 8'd17;
    inner_t nested;
    logic [3:0] lanes[2] = '{4'h5, 4'h6};
    real weight = 2.5;
    string label = "default";
    chandle handle;
  } record_t;
  record_t left_values[3:2], right_values[3:2], result[3:2];
  logic choose;
  initial begin
    left_values = '{'{8'h31, '{1'b0, 4'h2}, '{4'h3, 4'h4}, 1.5, "same", null},
                    '{8'h42, '{1'b0, 4'h3}, '{4'h3, 4'h4}, 1.5, "left", null}};
    right_values = '{'{8'h31, '{1'b0, 4'h2}, '{4'h3, 4'h4}, 1.5, "same", null},
                     '{8'h42, '{1'b0, 4'h4}, '{4'h3, 4'h4}, 1.5, "left", null}};
    choose = 1'bz;
    result = choose ? left_values : right_values;
    if (result[3].tag !== 8'h31 || result[3].nested.code !== 0 ||
        result[3].nested.word !== 4'h2 || result[3].lanes[0] !== 4'h3 ||
        result[3].lanes[1] !== 4'h4 || result[3].weight != 1.5 ||
        result[3].label != "same" || result[3].handle != null)
      $fatal(1, "equal complete record is preserved");
    if (result[2].tag !== 8'd17 || result[2].nested.code !== 1 ||
        result[2].nested.word !== 4'ha || result[2].lanes[0] !== 4'h5 ||
        result[2].lanes[1] !== 4'h6 || result[2].weight != 2.5 ||
        result[2].label != "default" || result[2].handle != null)
      $fatal(1, "one differing leaf defaults the complete record by its type");
    $display("passed");
  end
endmodule
)sv");
}

TEST(FixedArrayConditional, StringsAndScalarIntegralsKeepTheirDistinctMergeRules) {
    expectRun(R"sv(
module top;
  logic choose;
  string left_strings[3], right_strings[3], strings[3];
  logic [3:0] left_values[1], right_values[1], result[1], scalar_result;
  chandle left_handles[1], right_handles[1], handles[1];
  initial begin
    left_strings = '{"same", "left", "Case"};
    right_strings = '{"same", "right", "case"};
    left_values = '{4'b1010}; right_values = '{4'b1011};
    choose = 1'bx;
    strings = choose ? left_strings : right_strings;
    result = choose ? left_values : right_values;
    scalar_result = choose ? left_values[0] : right_values[0];
    handles = choose ? left_handles : right_handles;
    if (strings[0] != "same" || strings[1] != "" || strings[2] != "")
      $fatal(1, "string equality and empty-string defaults");
    if (result[0] !== 4'bxxxx || scalar_result !== 4'b101x)
      $fatal(1, "array elements default while scalar integrals merge bits");
    if (handles[0] != null) $fatal(1, "equal null handles");
    $display("passed");
  end
endmodule
)sv");
}
}  // namespace
