// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The stop before the stack of the running thread runs out (RUNTIME_ABI.md
// 5; #35, #38). Each case compiles and runs a design on a new thread with
// the stack size it names, and checks how the run ends; a crash fails it:
//
//   kirimasim_stack_tests <case>               one case of the matrix
//   kirimasim_stack_tests probe <file> <MiB>   any design, printing how
//                                              it ended
//
// The matrix crosses where the recursion is with the size of the thread.
// The recursion is the design's own calls in the VM: a function, a task
// that does not wait, a function that writes what a guard reads, so that
// the kernel runs the guard in the middle of the write (SCHEDULER_SPEC.md
// 9.5), and a deferred assertion's action. On a thread of 4 MiB none of
// them fits, and each stops with its message; on one of 512 MiB, the size
// the driver gives, each runs to its end.
//
// Or it is front's, over the syntax: a sum of many terms. How much stack
// a step takes differs about tenfold between the builds, so on a thread
// of 8 MiB a sum of 1000 terms runs in some and stops with front's
// diagnostic in others; either is right, a crash is not. slang binds the
// sum before front lowers it, recursing too, and its recursion is not
// checked: the sizes are chosen so that slang's fits in every build.
// On a thread of the system's default size a small design either runs
// or stops with a message (512 KiB on macOS is less than the margin
// kept), but never crashes.
//
// Two cases pin one check each, the same on every system: on a thread of
// 768 KiB, less than the margin even with what a system adds to the size
// asked for, every check finds the stack nearly used up, so the first check
// a design meets is the one that stops it. front's
// check is the first when the design is compiled there; the check before
// a guard that the kernel runs in the middle of a write is the first when
// the design, compiled on a large thread, runs there and makes no call
// before that guard. Without either check the design runs to its end.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <pthread.h>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/vm/vm.h"

namespace {

namespace kernel = kirimasim::kernel;

constexpr const char* kFrontStop = "error: nested too deeply for the stack of this thread";
constexpr const char* kRunStop = "error: the stack of this thread is nearly used up";

/// How a design ended: front's diagnostics when it did not compile, else
/// the reason and what the run wrote.
struct Outcome {
    bool compiled = false;
    std::string diagnostics;
    std::string reason;
    std::string out;
    std::string err;
};

struct CloseFile {
    void operator()(std::FILE* f) const { std::fclose(f); }
};
using File = std::unique_ptr<std::FILE, CloseFile>;

std::string contents(std::FILE* f) {
    std::fflush(f);
    std::fseek(f, 0, SEEK_SET);
    std::string text;
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0)
        text.append(buffer, n);
    return text;
}

/// Runs a compiled design and keeps how it ended.
void runInto(const kirimasim::sir::Design& design, Outcome& o) {
    const File out(std::tmpfile());
    const File err(std::tmpfile());
    if (!out || !err) {
        o.reason = "no temporary file";
        return;
    }
    {
        kernel::Context context(kernel::OutputDestinations{out.get(), err.get()});
        const kirimasim::vm::RunResult run = kirimasim::vm::run(design, context, {});
        context.output().flush();
        o.reason = kernel::endReasonName(run.reason);
    }
    o.out = contents(out.get());
    o.err = contents(err.get());
}

Outcome compileAndRun(const std::string& file) {
    Outcome o;
    auto r = kirimasim::front::compile(std::vector<std::string>{file});
    o.diagnostics = r.diagnostics + r.verifyErrors;
    o.compiled = r.success;
    if (r.success) runInto(r.design, o);
    return o;
}

/// Runs f on a new thread with a stack of `kib` KiB, or of the system's
/// default size for a std::thread when kib is 0.
bool onThread(std::size_t kib, const std::function<void()>& f) {
    if (kib == 0) {
        std::thread t(f);
        t.join();
        return true;
    }
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) return false;
    pthread_t id;
    auto body = [](void* p) -> void* {
        (*static_cast<const std::function<void()>*>(p))();
        return nullptr;
    };
    const bool made = pthread_attr_setstacksize(&attr, kib << 10) == 0 &&
                      pthread_create(&id, &attr, body, const_cast<std::function<void()>*>(&f)) == 0;
    pthread_attr_destroy(&attr);
    if (made) pthread_join(id, nullptr);
    return made;
}

// --- The designs ------------------------------------------------------------

std::string sum(int n) {
    std::string terms = "a";
    for (int i = 1; i < n; ++i)
        terms += " + a";
    return "module top;\n  int a = 1, x;\n  initial begin\n    x = " + terms +
           ";\n    $display(\"x=%0d\", x);\n  end\nendmodule\n";
}

const char* const kDepth = R"(
  function automatic int depth(input int n);
    return n == 0 ? 0 : 1 + depth(n - 1);
  endfunction
)";

std::string function(int n) {
    return std::string("module top;") + kDepth + "  initial $display(\"depth=%0d\", depth(" +
           std::to_string(n) + "));\nendmodule\n";
}

std::string task(int n) {
    return R"(module top;
  task automatic down(input int n, output int r);
    if (n == 0) r = 0;
    else begin
      down(n - 1, r);
      r = r + 1;
    end
  endtask
  int r;
  initial begin
    down()" +
           std::to_string(n) +
           R"(, r);
    $display("depth=%0d", r);
  end
endmodule
)";
}

std::string guard(int n) {
    return R"(module top;
  int v = -1;
  function automatic int down(input int n);
    v = n;
    return n == 0 ? 0 : 1 + down(n - 1);
  endfunction
  initial begin
    wait (v == 0);
    $display("seen");
  end
  initial #1 $display("depth=%0d", down()" +
           std::to_string(n) + R"());
endmodule
)";
}

/// No call of the design before the guard of `@(v iff ...)`, which the kernel
/// runs when the write of v at time 1 changes it.
std::string guardOnly() {
    return R"(module top;
  int v = 0;
  initial #1 v = 1;
  initial begin
    @(v iff (v == 1));
    $display("seen");
  end
endmodule
)";
}

std::string deferred(int n) {
    return std::string("module top;") + kDepth +
           "  initial assert #0 (1) $display(\"depth=%0d\", depth(" + std::to_string(n) +
           "));\nendmodule\n";
}

// --- The matrix --------------------------------------------------------------

enum class Expect {
    RunStops,    ///< the run ends with resource_limit and kRunStop
    FrontStops,  ///< front stops with kFrontStop
    Runs,        ///< the run ends with quiescent and prints `output`
    Either,      ///< Runs, or front stops with kFrontStop, or RunStops
};

constexpr std::size_t kMiB = 1024;  ///< in KiB, the unit of the sizes below

struct Case {
    std::string name;
    std::size_t kib;  ///< the thread that compiles, and runs unless runKiB says
    std::string design;
    Expect expect;
    std::string output;
    /// When not 0, the design is compiled on the thread of `kib` KiB and
    /// run on another one of this many KiB.
    std::size_t runKiB = 0;
};

std::vector<Case> cases() {
    // 9998 calls nest 9999 deep, inside the limit of 10000; they take 11
    // MiB of stack in release and more in the other builds (RUNTIME_ABI.md 5).
    constexpr int kCalls = 9998;
    return {
        {"front_small", 8 * kMiB, sum(1000), Expect::Either, "x=1000\n"},
        {"front_large", 512 * kMiB, sum(10000), Expect::Runs, "x=10000\n"},
        {"function_small", 4 * kMiB, function(kCalls), Expect::RunStops, ""},
        {"function_large", 512 * kMiB, function(kCalls), Expect::Runs, "depth=9998\n"},
        {"task_small", 4 * kMiB, task(kCalls), Expect::RunStops, ""},
        {"task_large", 512 * kMiB, task(kCalls), Expect::Runs, "depth=9998\n"},
        {"guard_small", 4 * kMiB, guard(kCalls), Expect::RunStops, ""},
        {"guard_large", 512 * kMiB, guard(kCalls), Expect::Runs, "depth=9998\nseen\n"},
        {"deferred_small", 4 * kMiB, deferred(kCalls), Expect::RunStops, ""},
        {"deferred_large", 512 * kMiB, deferred(kCalls), Expect::Runs, "depth=9998\n"},
        {"default_thread", 0, function(10), Expect::Either, "depth=10\n"},
        {"class_static_unit_thread", 0,
         "class Box;static int data=7;endclass "
         "module top;initial $display(\"value=%0d\",Box::data);endmodule",
         Expect::Either, "value=7\n"},
        {"class_static_package_thread", 0,
         "package pkg;class Box;static int data=7;endclass endpackage "
         "module top;initial $display(\"value=%0d\",pkg::Box::data);endmodule",
         Expect::Either, "value=7\n"},
        {"class_static_module_thread", 0,
         "module top;class Box;static int data=7;endclass "
         "initial $display(\"value=%0d\",Box::data);endmodule",
         Expect::Either, "value=7\n"},
        {"front_first", 768, function(10), Expect::FrontStops, ""},
        {"guard_first", 512 * kMiB, guardOnly(), Expect::RunStops, "", 768},
        // Class discovery checks the stack before any procedure owns a
        // catch. compile must return the diagnostic from that outer path.
        {"class_static_first", 768,
         "module top;class Box;static int data=7;endclass "
         "initial $display(\"value=%0d\",Box::data);endmodule",
         Expect::FrontStops, ""},
    };
}

bool contains(std::string_view text, std::string_view part) {
    return text.find(part) != std::string_view::npos;
}

void print(const Outcome& o) {
    std::printf("compiled=%d reason=%s\n--- diagnostics\n%s--- out\n%s--- err\n%s",
                o.compiled ? 1 : 0, o.reason.c_str(), o.diagnostics.c_str(), o.out.c_str(),
                o.err.c_str());
}

bool check(const Case& c, const Outcome& o) {
    const bool frontStops = !o.compiled && contains(o.diagnostics, kFrontStop);
    const bool runStops = o.compiled && o.reason == "resource_limit" && contains(o.err, kRunStop);
    const bool runs = o.compiled && o.reason == "quiescent" && o.out == c.output;
    switch (c.expect) {
        case Expect::RunStops: return runStops;
        case Expect::FrontStops: return frontStops;
        case Expect::Runs: return runs;
        case Expect::Either: return frontStops || runStops || runs;
    }
    return false;
}

int runCase(const std::string& name) {
    for (const Case& c : cases()) {
        if (c.name != name) continue;
        const std::string file = std::string(KIRIMASIM_STACK_DIR) + "/" + c.name + ".sv";
        {
            std::ofstream f(file);
            f << c.design;
        }
        Outcome o;
        if (c.runKiB == 0) {
            if (!onThread(c.kib, [&] { o = compileAndRun(file); })) {
                std::printf("FAIL %s: no thread of %zu KiB\n", name.c_str(), c.kib);
                return 1;
            }
        } else {
            kirimasim::front::CompileResult r;
            if (!onThread(c.kib,
                          [&] { r = kirimasim::front::compile(std::vector<std::string>{file}); })) {
                std::printf("FAIL %s: no thread of %zu KiB\n", name.c_str(), c.kib);
                return 1;
            }
            o.diagnostics = r.diagnostics + r.verifyErrors;
            o.compiled = r.success;
            if (o.compiled && !onThread(c.runKiB, [&] { runInto(r.design, o); })) {
                std::printf("FAIL %s: no thread of %zu KiB\n", name.c_str(), c.runKiB);
                return 1;
            }
        }
        if (check(c, o)) {
            std::printf("ok %s\n", name.c_str());
            return 0;
        }
        std::printf("FAIL %s\n", name.c_str());
        print(o);
        return 1;
    }
    std::printf("no case %s\n", name.c_str());
    return 2;
}

}  // namespace

int runTests(int argc, char** argv) {
    if (argc == 4 && std::string_view(argv[1]) == "probe") {
        const std::string file = argv[2];
        Outcome o;
        if (!onThread(std::strtoull(argv[3], nullptr, 10) * kMiB,
                      [&] { o = compileAndRun(file); })) {
            std::printf("no thread\n");
            return 1;
        }
        print(o);
        return 0;
    }
    if (argc != 2) {
        std::fprintf(stderr, "usage: kirimasim_stack_tests <case> | probe <file> <MiB>\n");
        return 2;
    }
    return runCase(argv[1]);
}

int main(int argc, char** argv) {
    try {
        return runTests(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "kirimasim_stack_tests: %s\n", e.what());
    }
    return 1;
}
