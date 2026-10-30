// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Source ownership checks for approved automatic-capture contract 5/6.
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
            (std::filesystem::temp_directory_path() / "kirimasim-struct-event-XXXXXX").string();
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
    std::uint64_t time = 0;
    std::size_t live = 0, allocated = 0, activations = 0, leases = 0;
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
    EXPECT_EQ(result.live, 0U);
    EXPECT_EQ(result.activations, 0U);
    EXPECT_EQ(result.leases, 0U);
}

TEST(StructEvent, ReenteredLexicalRecordsHaveIndependentEventsAndSurviveSourceDestruction) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct { event ready; event spare; int tag; } inner_t;
typedef struct { int prefix; inner_t inner; logic [128:0] wide; } record_t;
int completed, total;
initial begin
  for (int n=0;n<12;n++) begin
    automatic int expected=n;
    automatic record_t value;
    value.prefix=71; value.inner.tag=n; value.wide='1;
    fork
      begin #2; ->value.inner.ready; end
      begin
        @(value.inner.ready);
        if (value.inner.tag!=expected || value.inner.spare.triggered || value.prefix!=71 ||
            value.wide!=={129{1'b1}}) $fatal(1,"shared record activation");
        completed++; total+=value.inner.tag;
      end
    join_none
  end
  wait fork;
  $display("%0d:%0d",completed,total);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_TRUE(compiled.verifyErrors.empty()) << compiled.verifyErrors;
    std::size_t capturedEvents = 0;
    for (const auto& process : compiled.design.processes)
        for (const auto& field : process.frame)
            if (field.type.kind == kirimasim::sir::TypeKind::Event &&
                field.home == kirimasim::sir::FrameHome::Capture)
                ++capturedEvents;
    EXPECT_GE(capturedEvents, 2U);
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "12:66\n");
    EXPECT_EQ(result.time, 2U);
}

TEST(StructEvent, ReturnedOwnersPendingTriggersAndCollectionRetainCapturedFields) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct { event ready; event spare; int tag; } record_t;
int completed, total;
task automatic launch(input int tag);
  record_t value;
  value.tag=tag;
  ->> #2 value.ready;
  fork
    begin
      @(value.ready);
      if (value.spare.triggered || value.tag!=tag) $fatal(1,"retained event record");
      completed++; total+=value.tag;
    end
  join_none
endtask
task automatic churn();
  record_t value;
  if (value.ready.triggered || value.spare.triggered) $fatal(1,"stale event state");
  ->value.ready;
endtask
initial begin
  launch(11); launch(29);
  for (int n=0;n<6000;n++) churn();
  #3;
  $display("%0d:%0d",completed,total);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "2:40\n");
    EXPECT_EQ(result.time, 3U);
    EXPECT_LT(result.allocated, 64U);
}

TEST(StructEvent, NullAndCopiedInitializersDoNotConstructUnusedEventObjects) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct {event first; event second; int tag=7;} record_t;
typedef struct {event empty=null; event fresh; int tag=9;} nullable_t;
task automatic check_value();
  record_t original;
  record_t copied=original;
  record_t patterned='{original.first,original.second,23};
  nullable_t value;
  ->copied.first;
  if (!original.first.triggered || !patterned.first.triggered || original.second.triggered)
    $fatal(1,"initialization must share event identity");
  if (value.empty.triggered || value.fresh.triggered || value.tag!=9) $fatal(1,"null default");
  copied.tag=41;
  if (original.tag!=7 || patterned.tag!=23) $fatal(1,"ordinary fields must copy");
endtask
initial begin check_value(); check_value(); $display("passed"); end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    std::size_t constructors = 0;
    for (const auto& subroutine : compiled.design.subroutines)
        for (const auto& block : subroutine.blocks)
            for (const auto& instruction : block.instrs)
                if (instruction.op == kirimasim::sir::Op::EventNew) ++constructors;
    EXPECT_EQ(constructors, 3U);
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "passed\n");
    EXPECT_LE(result.allocated, 6U);
}

TEST(StructEvent, WaitingInputRecordsShareOnlyEventIdentitiesAndOrdinaryFieldsCopy) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct {event ready; event spare; int tag;} record_t;
int completed;
task automatic inner(input record_t value);
  event shared=value.ready;
  #2; ->shared;
  if (!value.ready.triggered || value.spare.triggered) $fatal(1,"forwarded event");
  value.tag=99;
endtask
task automatic outer(input record_t value); inner(value); endtask
task automatic owner(input int tag);
  record_t value;
  value.tag=tag;
  outer(value);
  if (!value.ready.triggered || value.tag!=tag) $fatal(1,"record input copy");
  completed++;
endtask
initial begin
  fork owner(11); owner(29); join
  $display("%0d",completed);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "2\n");
    EXPECT_EQ(result.time, 2U);
}

TEST(StructEvent, DisabledWaitersReleaseTheirRecordsAndPendingRegistrations) {
    const auto compiled = compileDetached(R"sv(module top;
typedef struct {event ready; event spare; int tag;} record_t;
task automatic owner();
  record_t value;
  fork
    begin @(value.ready); $fatal(1,"cancelled waiter resumed"); end
    begin #5; ->value.ready; end
  join_none
  #1; disable fork;
endtask
initial begin
  for (int n=0;n<8;n++) owner();
  $display("passed %0d",$time);
end
endmodule
)sv");
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = execute(compiled.design);
    success(result);
    EXPECT_EQ(result.output, "passed 8\n");
    EXPECT_LT(result.allocated, 32U);
}

TEST(StructEvent, RebindingCopybackAndReturningEventRecordsRemainOutsideTheContract) {
    for (const auto source :
         {"typedef struct {event e; int n;} T; T a,b; initial a=b;",
          "typedef struct {event e; int n;} T; task automatic set(output T a); endtask "
          "T value; initial set(value);",
          "typedef struct {event e; int n;} T; function automatic T make(); T value; "
          "return value; endfunction initial make();"}) {
        const Source file(std::string("module top; ") + source + " endmodule\n");
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
}  // namespace
