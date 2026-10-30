// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Fixed array leaves share the approved automatic activation and copy rules.
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
            (std::filesystem::temp_directory_path() / "kirimasim-record-array-XXXXXX").string();
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
    const Source source(text);
    EXPECT_FALSE(source.file.empty());
    if (source.file.empty()) return {};
    return front::compile(std::vector<std::string>{source.file});
}
struct Execution {
    vm::RunResult result;
    std::string output, diagnostics;
    std::uint64_t time = 0;
    std::size_t events = 0, allocatedEvents = 0, scopes = 0, leases = 0;
};
Execution execute(const kirimasim::sir::Design& design) {
    Capture out, err;
    EXPECT_NE(out.file, nullptr);
    EXPECT_NE(err.file, nullptr);
    if (out.file == nullptr || err.file == nullptr)
        return {{k::EndReason::Internal, "cannot capture output"}, {}, {}};
    k::Context context(k::OutputDestinations{out.file, err.file});
    auto result = vm::run(design, context);
    context.output().flush();
    const auto scopes = context.activations().stats();
    return {std::move(result),
            out.text(),
            err.text(),
            context.scheduler().time(),
            context.managedEvents().liveObjects(),
            context.managedEvents().allocatedObjects(),
            scopes.live,
            scopes.leases};
}
void success(const Execution& result) {
    EXPECT_EQ(result.result.reason, k::EndReason::Quiescent)
        << result.result.error << result.diagnostics;
    EXPECT_TRUE(result.result.error.empty()) << result.result.error;
    EXPECT_TRUE(result.diagnostics.empty()) << result.diagnostics;
    EXPECT_EQ(result.events, 0U);
    EXPECT_EQ(result.scopes, 0U);
    EXPECT_EQ(result.leases, 0U);
}

TEST(StructArrayMember, RetainedIterationsShareArrayLeavesAndObserveSiblingChanges) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct {int words[2]; logic [128:0] wide[-1:0];} inner_t;
typedef struct {inner_t inner; string labels[2];} T;
int completed,total;
initial begin
  for (int n=0;n<12;n++) begin
    automatic int expected=n;
    automatic T value;
    value.inner.words[0]=n; value.inner.wide[-1]='1; value.labels[1]="retained";
    fork
      begin #1; value.inner.words[1]=value.inner.words[0]+1; end
      begin
        wait(value.inner.words[1]!=0);
        if (value.inner.words[1]!=expected+1 || value.inner.wide[-1]!=={129{1'b1}} ||
            value.labels[1]!="retained") $fatal(1,"record array capture");
        total+=value.inner.words[0]; completed++;
      end
    join_none
  end
  wait fork;
  $display("%0d:%0d",completed,total);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    std::size_t capturedArrays = 0;
    for (const auto& process : compiled.design.processes)
        for (const auto& field : process.frame)
            if (field.home == kirimasim::sir::FrameHome::Capture && !field.dims.empty())
                ++capturedArrays;
    EXPECT_GE(capturedArrays, 3U);
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "12:66\n");
    EXPECT_EQ(result.time, 1U);
}

TEST(StructArrayMember, ReturnedOwnersRetainContainerArrayRootsAndIndependentEvents) {
    const auto compiled = compileDetached(R"sv(module top;
typedef int queue_t[$];
typedef struct {queue_t queues[2]; string labels[2]; event ready;} T;
int completed,total;
task automatic launch(input int n);
  T value;
  value.queues[1].push_back(n); value.labels[0]="kept";
  ->> #2 value.ready;
  fork
    begin
      @(value.ready);
      if (value.queues[1].size()!=1 || value.queues[1][0]!=n || value.labels[0]!="kept")
        $fatal(1,"retained roots");
      total+=value.queues[1][0]; completed++;
    end
  join_none
endtask
task automatic churn();
  T value;
  value.queues[0].push_back(99);
endtask
initial begin
  launch(11); launch(29);
  for (int n=0;n<4000;n++) churn();
  #3; $display("%0d:%0d",completed,total);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "2:40\n");
    EXPECT_LT(result.allocatedEvents, 64U);
}

TEST(StructArrayMember, OutputDefaultsCopyBackOnceAndWaitingInoutKeepsArrayShape) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct {int words[2]='{3,5};} T;
T actual;
task automatic put(output T value);
  if (value.words[0]!=3 || value.words[1]!=5) $fatal(1,"output defaults");
  value.words[0]=7;
  fork begin #1; value.words[0]=99; end join_none
endtask
task automatic change(inout T value); #1; value.words[1]++; endtask
task automatic take(input T value);
  if (value.words[0]!=7 || value.words[1]!=6) $fatal(1,"input shape");
  value.words[0]=101;
endtask
initial begin
  put(actual);
  if (actual.words[0]!=7) $fatal(1,"early copyback");
  change(actual); #1; take(actual);
  $display("%0d:%0d",actual.words[0],actual.words[1]);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "7:6\n");
    EXPECT_EQ(result.time, 2U);
}

TEST(StructArrayMember, NestedContainerCopiesAndReturnedArraysAreIndependentAcrossContexts) {
    const auto compiled = compileDetached(R"sv(module top;
typedef int queue_t[$];
typedef struct {queue_t queues[2]; int words[3:1];} T;
function automatic T make(input int n);
  T value;
  value.words='{n,n+1,n+2}; value.queues[1].push_back(n);
  return value;
endfunction
task automatic check_value();
  T a=make(7);
  T b=a;
  b.queues[1].push_back(11); b.words[3]=13;
  if (a.queues[1].size()!=1 || a.words[3]!=7 || b.queues[1][1]!=11 || b.words[3]!=13)
    $fatal(1,"deep copy");
endtask
initial begin for (int n=0;n<100;n++) check_value(); $display("passed"); end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (int repetition = 0; repetition < 2; ++repetition) {
        const auto result = execute(compiled.design);
        success(result);
        EXPECT_EQ(result.output, "passed\n");
    }
}

TEST(StructArrayMember, EventArraysKeepTheirReviewedBoundary) {
    for (const auto declaration : {"struct {event ready[2];} value;"}) {
        const Source file(std::string("module top; typedef struct {int words[2];} T; ") +
                          "task automatic check_value(); " + declaration +
                          " endtask initial check_value(); endmodule\n");
        ASSERT_FALSE(file.file.empty());
        const std::vector<std::string> files{file.file};
        const auto parsed = front::parseAndElaborate(files);
        ASSERT_TRUE(parsed.success) << parsed.diagnostics;
        const auto compiled = front::compile(files);
        EXPECT_FALSE(compiled.success);
        EXPECT_TRUE(compiled.unsupported);
        EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    }
}

TEST(StructArrayMember, AutomaticArraysOfStructuresCompile) {
    // A frame variable per member with the array's dimensions (SIR_SPEC.md
    // 4.2, amended 2026-10-30).
    const Source file(std::string("module top; typedef struct {int words[2];} T; ") +
                      "task automatic check_value(); T values[2];" +
                      " endtask initial check_value(); endmodule\n");
    ASSERT_FALSE(file.file.empty());
    const std::vector<std::string> files{file.file};
    const auto parsed = front::parseAndElaborate(files);
    ASSERT_TRUE(parsed.success) << parsed.diagnostics;
    const auto compiled = front::compile(files);
    EXPECT_TRUE(compiled.success) << compiled.diagnostics;
    EXPECT_FALSE(compiled.unsupported);
    EXPECT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
}
}  // namespace
