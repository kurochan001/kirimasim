// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The kirimasim command line (CLI.md).
//
//   kirimasim [options] <file>... [+plusarg]...   compile and run
//   kirimasim parse [options] <file>...          parse and elaborate only
//   kirimasim --version | --help
//
// Options so far: -f, +incdir+, +define+, --top, --timescale (passed to
// slang), --max-time, --max-time-ok, --max-zero-delay, --max-slot-events,
// --max-steps, --max-scc-iter, --max-call-depth, --seed, --dump-sir,
// --stats=json, --mintypmax, --bounds-check, --autoflush, --stop,
// --max-wall-time, --wave, --wave-scope and --wave-glitches. Exit codes
// are those of CLI.md 5.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <pthread.h>

#include "build_info.h"
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/vm/vm.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitInputError = 2;
constexpr int kExitInternalError = 3;

// The one variable outside the Context (RUNTIME_ABI.md 9.4, listed in
// tools/global_state_allowlist.txt): the flag of SIGINT, as a signal
// handler can reach nothing else. The scheduler reads it through the
// pointer the driver gives it (RUNTIME_ABI.md 4.4).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,custom-kirimasim-global-state)
volatile std::sig_atomic_t interruptRequested = 0;

}  // namespace

// The first SIGINT asks the run to stop; a second one ends kirimasim at
// once, with the code of the reason interrupted (SCHEDULER_SPEC.md 16.2,
// CLI.md 5).
extern "C" void kirimasimOnInterrupt(int /*signal*/) {
    if (interruptRequested != 0) std::_Exit(130);
    interruptRequested = 1;
}

namespace {

void printUsage(std::FILE* out) {
    std::fputs(
        "usage: kirimasim [options] <file>... [+plusarg]...\n"
        "       kirimasim parse [options] <file>...\n"
        "       kirimasim --version\n"
        "options: -f <file>, +incdir+<dir>, +define+<name>[=<value>], --top=<name>,\n"
        "         --timescale=<unit>/<precision>, --max-time=<time>, --max-time-ok,\n"
        "         --max-zero-delay=<n>, --max-slot-events=<n>, --max-steps=<n>,\n"
        "         --max-scc-iter=<n>, --max-call-depth=<n>, --seed=<n>, --dump-sir,\n"
        "         --stats=json, --mintypmax=min|typ|max, --bounds-check=off|warn, --autoflush,\n"
        "         --stop=finish|ignore|error, --max-wall-time=<seconds>,\n"
        "         --wave=<file>.vcd,\n"
        "         --wave-scope=<pattern>[,<pattern>...], --wave-glitches\n",
        out);
}

std::string versionText() {
    return std::string("kirimasim ") + kirimasim::driver::kVersion + " (commit " +
           kirimasim::driver::kCommit + ")";
}

void printVersion() {
    std::printf("%s, built with %s %s\n", versionText().c_str(), kirimasim::driver::kCompilerId,
                kirimasim::driver::kCompilerVersion);
}

struct Options {
    std::string command;                 // "" (compile and run) or "parse"
    std::vector<std::string> slangArgs;  // files and options handed to slang
    std::vector<std::string> plusargs;   // run-time plusargs, in order
    std::vector<std::string> original;   // the arguments as given, for the version line
    bool statsJson = false;
    bool dumpSir = false;
    std::string maxTime;  // as given; converted once the time precision is known
    bool maxTimeOk = false;
    std::uint64_t maxZeroDelay = 1000000;  // the defaults of CLI.md 2.6; 0: no limit
    std::uint64_t maxSlotEvents = 100000000;
    std::uint64_t maxSteps = 1000000000;
    std::uint64_t maxWallTime = 0;
    std::uint64_t maxSccIter = 0;  // no effect until tier 2 has islands (P3)
    std::uint64_t maxCallDepth = kirimasim::vm::kDefaultCallDepth;  // CLI.md 2.7
    std::uint64_t seed = 0;                                         // --seed (SYSTEM_TASKS.md 13.2)
    std::string waveFile;                                           // --wave
    std::vector<std::string> waveScopes;                            // --wave-scope
    bool waveGlitches = false;
    bool boundsCheck = false;
    bool autoFlush = false;
    kirimasim::kernel::StopAction stopAction = kirimasim::kernel::StopAction::Finish;
};

/// Reads the number of --name=<n>. False if it is not a whole number.
bool parseCount(std::string_view arg, std::string_view name, std::uint64_t& out) {
    std::string_view text = arg;
    text.remove_prefix(std::min(text.size(), name.size() + 1));  // past "--name=
    const char* first = std::to_address(text.begin());
    const char* last = std::to_address(text.end());
    const auto r = std::from_chars(first, last, out);
    return !text.empty() && r.ec == std::errc() && r.ptr == last;
}

/// Reads --max-call-depth=<n> (CLI.md 2.7): a whole number above 0, as
/// the stack of the simulation thread is sized by it (RUNTIME_ABI.md 5).
/// False, with the line written, if it is not one.
bool parseCallDepth(std::string_view arg, std::uint64_t& out) {
    if (parseCount(arg, "--max-call-depth", out) && out != 0) return true;
    std::fprintf(stderr, "kirimasim: error: %.*s needs a whole number above 0\n",
                 static_cast<int>(arg.size()), arg.data());
    return false;
}

bool isSlangPlusOption(std::string_view arg) {
    return arg.starts_with("+incdir+") || arg.starts_with("+define+") ||
           arg.starts_with("+libext+");
}

/// Parses the command line. Returns -1 to go on, or an exit code.
int parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        o.original.emplace_back(arg);
        if (arg == "--version") {
            printVersion();
            return kExitOk;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage(stdout);
            return kExitOk;
        }
        bool counted = false;
        for (const auto& [name, field] :
             {std::pair<std::string_view, std::uint64_t*>{"--max-zero-delay", &o.maxZeroDelay},
              {"--max-slot-events", &o.maxSlotEvents},
              {"--max-steps", &o.maxSteps},
              {"--max-wall-time", &o.maxWallTime},
              {"--max-scc-iter", &o.maxSccIter},
              {"--seed", &o.seed}}) {
            if (!arg.starts_with(name) || arg.size() <= name.size() || arg[name.size()] != '=')
                continue;
            if (!parseCount(arg, name, *field)) {
                std::fprintf(stderr, "kirimasim: error: %s needs a whole number\n", argv[i]);
                return kExitInputError;
            }
            counted = true;
        }
        if (counted) continue;
        if (arg.starts_with("--max-call-depth=")) {
            if (!parseCallDepth(arg, o.maxCallDepth)) return kExitInputError;
        } else if (arg == "--stats=json") {
            o.statsJson = true;
        } else if (arg == "--autoflush") {
            o.autoFlush = true;
        } else if (arg.starts_with("--stop=")) {
            const auto choice = arg.substr(std::string_view("--stop=").size());
            if (choice == "finish")
                o.stopAction = kirimasim::kernel::StopAction::Finish;
            else if (choice == "ignore")
                o.stopAction = kirimasim::kernel::StopAction::Ignore;
            else if (choice == "error")
                o.stopAction = kirimasim::kernel::StopAction::Error;
            else {
                std::fprintf(stderr, "kirimasim: error: %s needs finish, ignore or error\n",
                             argv[i]);
                return kExitInputError;
            }
        } else if (arg.starts_with("--max-time=")) {
            o.maxTime = arg.substr(std::string_view("--max-time=").size());
            // The precision is not known yet; check the form with the
            // coarsest one, which can only fail on the form.
            if (!kirimasim::sir::parseTimeTicks(o.maxTime, kirimasim::sir::kMaxTimeExponent)) {
                std::fprintf(stderr, "kirimasim: error: %s needs a time such as 10us or 1.5ns\n",
                             argv[i]);
                return kExitInputError;
            }
        } else if (arg == "--max-time-ok") {
            o.maxTimeOk = true;
        } else if (arg == "--dump-sir") {
            o.dumpSir = true;
        } else if (arg.starts_with("--bounds-check=")) {
            const auto choice = arg.substr(std::string_view("--bounds-check=").size());
            if (choice != "off" && choice != "warn") {
                std::fprintf(stderr, "kirimasim: error: %s needs off or warn\n", argv[i]);
                return kExitInputError;
            }
            o.boundsCheck = choice == "warn";
        } else if (arg.starts_with("--mintypmax=")) {
            const auto choice = arg.substr(std::string_view("--mintypmax=").size());
            if (choice != "min" && choice != "typ" && choice != "max") {
                std::fprintf(stderr, "kirimasim: error: %s needs min, typ or max\n", argv[i]);
                return kExitInputError;
            }
            // Let slang select every min:typ:max expression consistently
            // during elaboration (CLI.md 2.2, SCHEDULER_SPEC.md 12).
            o.slangArgs.emplace_back("--timing=" + std::string(choice));
        } else if (arg.starts_with("--wave=")) {
            // A VCD file; FST waits for the decision on libfstapi
            // (WAVEFORM.md 1).
            o.waveFile = arg.substr(std::string_view("--wave=").size());
            if (o.waveFile.ends_with(".fst")) {
                std::fprintf(stderr,
                             "kirimasim: error: %s: FST is not supported yet; use a .vcd file\n",
                             argv[i]);
                return kExitInputError;
            }
            if (!o.waveFile.ends_with(".vcd") || o.waveFile.size() == 4) {
                std::fprintf(stderr, "kirimasim: error: %s needs a file ending in .vcd\n", argv[i]);
                return kExitInputError;
            }
        } else if (arg.starts_with("--wave-scope=")) {
            // Patterns separated by commas (WAVEFORM.md 5).
            std::string_view rest = arg.substr(std::string_view("--wave-scope=").size());
            for (;;) {
                const std::size_t comma = rest.find(',');
                const std::string_view p = rest.substr(0, comma);
                if (p.empty()) {
                    std::fprintf(stderr, "kirimasim: error: %s has an empty pattern\n", argv[i]);
                    return kExitInputError;
                }
                o.waveScopes.emplace_back(p);
                if (comma == std::string_view::npos) break;
                rest.remove_prefix(comma + 1);
            }
        } else if (arg == "--wave-glitches") {
            o.waveGlitches = true;
        } else if (arg == "-f" || arg == "--top") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "kirimasim: error: %s needs a value\n", argv[i]);
                return kExitInputError;
            }
            o.slangArgs.emplace_back(arg);
            o.slangArgs.emplace_back(argv[++i]);
            o.original.emplace_back(argv[i]);
        } else if (arg.starts_with("--top=") || arg.starts_with("--timescale=")) {
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with("+")) {
            if (isSlangPlusOption(arg))
                o.slangArgs.emplace_back(arg);
            else
                o.plusargs.emplace_back(arg);
        } else if (arg.starts_with("-")) {
            std::fprintf(stderr, "kirimasim: error: unknown option '%s'\n", argv[i]);
            printUsage(stderr);
            return kExitInputError;
        } else if (o.command.empty() && o.slangArgs.empty() && arg == "parse") {
            o.command = "parse";
        } else {
            o.slangArgs.emplace_back(arg);
        }
    }
    return -1;
}

int runParse(kirimasim::kernel::Context& context, const Options& o) {
    if (o.slangArgs.empty()) {
        std::fputs("kirimasim: error: parse needs at least one file\n", stderr);
        return kExitInputError;
    }
    const auto result = kirimasim::front::parseAndElaborate(o.slangArgs);
    std::fputs(result.diagnostics.c_str(), stderr);

    auto& counters = context.counters();
    counters.set(counters.add("front.files"), result.numFiles);
    counters.set(counters.add("front.syntax_trees"), result.numSyntaxTrees);
    counters.set(counters.add("front.errors"), result.numErrors);
    counters.set(counters.add("front.warnings"), result.numWarnings);

    return result.success ? kExitOk : kExitInputError;
}

/// The exit code of the reason a run ended for (CLI.md 5). The table is
/// the driver's: the kernel returns the reason only (RUNTIME_ABI.md 9.3.9).
int exitCode(kirimasim::kernel::EndReason r) {
    using kirimasim::kernel::EndReason;
    switch (r) {
        case EndReason::Finish:
        case EndReason::Quiescent:
        case EndReason::Stop: return 0;
        case EndReason::Fatal: return 1;
        case EndReason::Error: return 2;
        case EndReason::Internal: return 3;
        case EndReason::TimeLimit:
        case EndReason::WallTimeLimit: return 4;
        case EndReason::ZeroTimeLimit: return 5;
        case EndReason::ResourceLimit: return 6;
        case EndReason::Interrupted: return 130;
    }
    return 3;
}

int runDesign(kirimasim::kernel::Context& context, const Options& o) {
    if (o.slangArgs.empty()) {
        std::fputs("kirimasim: error: no input files\n", stderr);
        printUsage(stderr);
        return kExitInputError;
    }
    if (!o.dumpSir) {
        std::string line =
            "kirimasim: info: " + versionText() + " seed=" + std::to_string(o.seed) + " args:";
        for (const auto& a : o.original)
            line += " " + a;
        std::fprintf(stderr, "%s\n", line.c_str());
    }

    const auto compiled = kirimasim::front::compile(o.slangArgs);
    std::fputs(compiled.diagnostics.c_str(), stderr);
    if (!compiled.verifyErrors.empty()) {
        std::fputs(compiled.verifyErrors.c_str(), stderr);
        return kExitInternalError;
    }
    if (!compiled.success) return kExitInputError;

    // The size of the SIR (PERFORMANCE.md 3.2).
    const kirimasim::sir::Design& d = compiled.design;
    std::uint64_t blocks = 0;
    std::uint64_t instrs = 0;
    const auto count = [&](const std::vector<kirimasim::sir::BasicBlock>& bs) {
        blocks += bs.size();
        for (const auto& b : bs)
            instrs += b.instrs.size();
    };
    for (const auto& p : d.processes)
        count(p.blocks);
    for (const auto& s : d.subroutines)
        count(s.blocks);
    auto& counters = context.counters();
    counters.set(counters.add("sir.slots"), d.slots.size());
    counters.set(counters.add("sir.processes"), d.processes.size());
    counters.set(counters.add("sir.subroutines"), d.subroutines.size());
    counters.set(counters.add("sir.blocks"), blocks);
    counters.set(counters.add("sir.instrs"), instrs);

    if (o.dumpSir) {
        std::fputs(kirimasim::sir::toText(d).c_str(), stdout);
        return kExitOk;
    }

    const int precision = compiled.design.timePrecision;
    kirimasim::kernel::Limits limits;
    if (!o.maxTime.empty()) {
        const auto ticks = kirimasim::sir::parseTimeTicks(o.maxTime, precision);
        if (!ticks) {
            std::fprintf(stderr,
                         "kirimasim: error: --max-time=%s is too large for the time precision %s\n",
                         o.maxTime.c_str(), kirimasim::sir::timeExponentText(precision).c_str());
            return kExitInputError;
        }
        limits.maxTime = *ticks;
    }
    limits.maxZeroDelay = o.maxZeroDelay;
    limits.maxSlotEvents = o.maxSlotEvents;
    context.scheduler().setLimits(limits);
    kirimasim::vm::RunOptions run;
    run.maxSteps = o.maxSteps;
    run.maxCallDepth = o.maxCallDepth;
    for (const std::string& a : o.plusargs)
        run.plusargs.push_back(a.substr(1));
    // The waveform's date, read here as the time of day is an outside
    // input (RUNTIME_ABI.md 9.3.8; WAVEFORM.md 3).
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
    char date[32] = "";
    if (gmtime_r(&now, &utc) != nullptr)
        std::strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S UTC", &utc);
    run.date = date;
    run.version = std::string("kirimasim ") + kirimasim::driver::kVersion;
    run.waveFile = o.waveFile;
    run.waveScopes = o.waveScopes;
    run.waveGlitches = o.waveGlitches;
    run.boundsCheck = o.boundsCheck;
    run.seed = o.seed;
    // SIGINT stops the run from here on; before, while compiling, it ends
    // kirimasim as the system does by default (SCHEDULER_SPEC.md 16.2).
    context.scheduler().setInterruptFlag(&interruptRequested);
    std::signal(SIGINT, kirimasimOnInterrupt);
    if (o.maxWallTime != 0)
        context.scheduler().setWallTimeLimit(o.maxWallTime, std::chrono::steady_clock::now());
    const auto result = kirimasim::vm::run(compiled.design, context, run);
    context.output().flush();
    const auto reason = result.reason;
    if (reason == kirimasim::kernel::EndReason::Internal) {
        std::fprintf(stderr, "kirimasim: internal error: %s\n", result.error.c_str());
    } else {
        if (reason == kirimasim::kernel::EndReason::WallTimeLimit)
            std::fprintf(stderr, "kirimasim: error: %s\n", context.scheduler().endNote().c_str());
        else if (!context.scheduler().endNote().empty())
            std::fprintf(stderr, "kirimasim: error: time did not advance: %s\n",
                         context.scheduler().endNote().c_str());
        const bool silent = (reason == kirimasim::kernel::EndReason::Finish ||
                             reason == kirimasim::kernel::EndReason::Stop) &&
                            context.scheduler().level() == 0;
        if (!silent) {
            std::fprintf(stderr, "kirimasim: info: %s at %s\n",
                         kirimasim::kernel::endReasonName(reason),
                         kirimasim::sir::timeText(context.scheduler().time(), precision).c_str());
        }
        // $finish(2) and $stop(2): the statistics, the values of
        // --stats=json once the run has ended (SYSTEM_TASKS.md 10.2).
        if (((reason == kirimasim::kernel::EndReason::Finish ||
              reason == kirimasim::kernel::EndReason::Stop) &&
             context.scheduler().level() == 2) ||
            context.scheduler().ignoredStopStatistics())
            std::fprintf(stderr, "kirimasim: info: statistics %s",
                         context.counters().toJson().c_str());
    }
    if (reason == kirimasim::kernel::EndReason::TimeLimit && o.maxTimeOk) return kExitOk;
    if (reason == kirimasim::kernel::EndReason::Stop &&
        o.stopAction == kirimasim::kernel::StopAction::Error)
        return 1;
    return exitCode(reason);
}

int run(int argc, char** argv) {
    Options o;
    if (const int code = parseArgs(argc, argv, o); code >= 0) return code;

    // The CLI's destinations: standard output and standard error (CLI.md 3);
    // STDIN reads standard input (SYSTEM_TASKS.md 6.3).
    kirimasim::kernel::Context context(kirimasim::kernel::OutputDestinations{stdout, stderr},
                                       stdin);
    context.output().setAutoFlush(o.autoFlush);
    context.scheduler().setStopAction(o.stopAction);
    const int code = o.command == "parse" ? runParse(context, o) : runDesign(context, o);
    if (o.statsJson) {
        // The JSON is the last line of stdout (PERFORMANCE.md 3.1).
        if (!context.output().atLineStart()) std::fputc('\n', stdout);
        std::fputs(context.counters().toJson().c_str(), stdout);
    }
    return code;
}

/// The stack of the thread that compiles and runs the design: calls nested
/// as deep as tier 1 allows by default (SCHEDULER_SPEC.md 16.2) need more
/// than a main thread has (RUNTIME_ABI.md 5).
constexpr std::size_t kStackBytes = std::size_t{512} << 20;

/// The stack for calls nested `depth` deep: kStackBytes for the default
/// and less, as compiling runs on the thread too, and in the default's
/// ratio above it (RUNTIME_ABI.md 5). 0 when that does not fit a size_t.
std::size_t stackBytes(std::uint64_t depth) {
    if (depth <= kirimasim::vm::kDefaultCallDepth) return kStackBytes;
    if (depth > SIZE_MAX / kStackBytes) return 0;
    return kStackBytes * static_cast<std::size_t>(depth) / kirimasim::vm::kDefaultCallDepth;
}

struct Job {
    int argc;
    char** argv;
    int code = kExitInternalError;
};

void runJob(Job& job) {
    try {
        job.code = run(job.argc, job.argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "kirimasim: internal error: %s\n", e.what());
    } catch (...) {
        std::fputs("kirimasim: internal error: unknown exception\n", stderr);
    }
}

}  // namespace

int main(int argc, char** argv) {
    // The thread's stack is sized by --max-call-depth, read here, before
    // the thread that reads the other options starts.
    std::uint64_t depth = kirimasim::vm::kDefaultCallDepth;
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]).starts_with("--max-call-depth=") &&
            !parseCallDepth(argv[i], depth))
            return kExitInputError;
    const std::size_t stack = stackBytes(depth);
    Job job{argc, argv};
    pthread_attr_t attr;
    pthread_t thread;
    bool started = stack != 0 && pthread_attr_init(&attr) == 0;
    started = started && pthread_attr_setstacksize(&attr, stack) == 0 &&
              pthread_create(
                  &thread, &attr,
                  [](void* j) -> void* {
                      runJob(*static_cast<Job*>(j));
                      return nullptr;
                  },
                  &job) == 0;
    if (stack != 0) pthread_attr_destroy(&attr);
    if (!started && depth != kirimasim::vm::kDefaultCallDepth) {
        std::fprintf(stderr,
                     "kirimasim: error: --max-call-depth=%llu needs a larger stack than the "
                     "system gives a thread\n",
                     static_cast<unsigned long long>(depth));
        return kExitInputError;
    }
    if (!started) {
        std::fputs("kirimasim: internal error: no thread with a stack for the run\n", stderr);
        return kExitInternalError;
    }
    pthread_join(thread, nullptr);
    return job.code;
}
