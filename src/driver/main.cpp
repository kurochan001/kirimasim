// The kirimasim command line (CLI.md).
//
//   kirimasim [options] <file>... [+plusarg]...   compile and run
//   kirimasim parse [options] <file>...          parse and elaborate only
//   kirimasim --version | --help
//
// Options so far: -f, +incdir+, +define+, --top, --timescale (passed to
// slang), --max-time, --max-time-ok, --max-zero-delay, --max-slot-events,
// --max-steps, --max-scc-iter, --dump-sir and --stats=json. Exit codes are
// those of CLI.md 5.

#include <charconv>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

// The one variable outside the Context (CLAUDE.md): the flag of SIGINT, as
// a signal handler can reach nothing else. The scheduler reads it through
// the pointer the driver gives it (RUNTIME_ABI.md 4.4).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the exception
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
    std::fputs("usage: kirimasim [options] <file>... [+plusarg]...\n"
               "       kirimasim parse [options] <file>...\n"
               "       kirimasim --version\n"
               "options: -f <file>, +incdir+<dir>, +define+<name>[=<value>], --top=<name>,\n"
               "         --timescale=<unit>/<precision>, --max-time=<time>, --max-time-ok,\n"
               "         --max-zero-delay=<n>, --max-slot-events=<n>, --max-steps=<n>,\n"
               "         --max-scc-iter=<n>, --dump-sir, --stats=json\n",
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
    std::uint64_t maxSccIter = 0;  // no effect until tier 2 has islands (P2)
};

/// Reads the number of --name=<n>. False if it is not a whole number.
bool parseCount(std::string_view arg, std::string_view name, std::uint64_t& out) {
    const std::string_view text = arg.substr(name.size() + 1);
    const char* first = std::to_address(text.begin());
    const char* last = std::to_address(text.end());
    const auto r = std::from_chars(first, last, out);
    return !text.empty() && r.ec == std::errc() && r.ptr == last;
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
              {"--max-scc-iter", &o.maxSccIter}}) {
            if (!arg.starts_with(name) || arg.size() <= name.size() || arg[name.size()] != '=')
                continue;
            if (!parseCount(arg, name, *field)) {
                std::fprintf(stderr, "kirimasim: error: %s needs a whole number\n", argv[i]);
                return kExitInputError;
            }
            counted = true;
        }
        if (counted) continue;
        if (arg == "--stats=json") {
            o.statsJson = true;
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

int runDesign(kirimasim::kernel::Context& context, const Options& o) {
    if (o.slangArgs.empty()) {
        std::fputs("kirimasim: error: no input files\n", stderr);
        printUsage(stderr);
        return kExitInputError;
    }
    if (!o.dumpSir) {
        std::string line = "kirimasim: info: " + versionText() + " args:";
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
    for (const std::string& a : o.plusargs)
        run.plusargs.push_back(a.substr(1));
    // SIGINT stops the run from here on; before, while compiling, it ends
    // kirimasim as the system does by default (SCHEDULER_SPEC.md 16.2).
    context.scheduler().setInterruptFlag(&interruptRequested);
    std::signal(SIGINT, kirimasimOnInterrupt);
    const auto result = kirimasim::vm::run(compiled.design, context, run);
    context.output().flush();
    const auto reason = result.reason;
    if (reason == kirimasim::kernel::EndReason::Internal) {
        std::fprintf(stderr, "kirimasim: internal error: %s\n", result.error.c_str());
    } else {
        if (!context.scheduler().endNote().empty())
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
        if ((reason == kirimasim::kernel::EndReason::Finish ||
             reason == kirimasim::kernel::EndReason::Stop) &&
            context.scheduler().level() == 2)
            std::fprintf(stderr, "kirimasim: info: statistics %s",
                         context.counters().toJson().c_str());
    }
    if (reason == kirimasim::kernel::EndReason::TimeLimit && o.maxTimeOk) return kExitOk;
    return kirimasim::kernel::exitCode(reason);
}

int run(int argc, char** argv) {
    Options o;
    if (const int code = parseArgs(argc, argv, o); code >= 0) return code;

    kirimasim::kernel::Context context;
    const int code = o.command == "parse" ? runParse(context, o) : runDesign(context, o);
    if (o.statsJson) {
        // The JSON is the last line of stdout (PERFORMANCE.md 3.1).
        if (!context.output().atLineStart()) std::fputc('\n', stdout);
        std::fputs(context.counters().toJson().c_str(), stdout);
    }
    return code;
}

/// The stack of the thread that compiles and runs the design: calls nested
/// as deep as tier 1 allows (SCHEDULER_SPEC.md 16.2) need more than a main
/// thread has (RUNTIME_ABI.md 5).
constexpr std::size_t kStackBytes = std::size_t{512} << 20;

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
    Job job{argc, argv};
    pthread_attr_t attr;
    pthread_t thread;
    bool started = pthread_attr_init(&attr) == 0;
    started = started && pthread_attr_setstacksize(&attr, kStackBytes) == 0 &&
              pthread_create(
                  &thread, &attr,
                  [](void* j) -> void* {
                      runJob(*static_cast<Job*>(j));
                      return nullptr;
                  },
                  &job) == 0;
    pthread_attr_destroy(&attr);
    if (!started) {
        std::fputs("kirimasim: internal error: no thread with a stack for the run\n", stderr);
        return kExitInternalError;
    }
    pthread_join(thread, nullptr);
    return job.code;
}
