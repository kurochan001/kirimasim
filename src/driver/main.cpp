// The haremasim command line (CLI.md).
//
//   haremasim [options] <file>... [+plusarg]...   compile and run
//   haremasim parse [options] <file>...          parse and elaborate only
//   haremasim --version | --help
//
// Options so far: -f, +incdir+, +define+, --top (passed to slang),
// --dump-sir and --stats=json. Exit codes are those of CLI.md 5.

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include "build_info.h"
#include "haremasim/front/front.h"
#include "haremasim/kernel/context.h"
#include "haremasim/sir/text.h"
#include "haremasim/vm/vm.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitInputError = 2;
constexpr int kExitInternalError = 3;

void printUsage(std::FILE* out) {
    std::fputs("usage: haremasim [options] <file>... [+plusarg]...\n"
               "       haremasim parse [options] <file>...\n"
               "       haremasim --version\n"
               "options: -f <file>, +incdir+<dir>, +define+<name>[=<value>], --top=<name>,\n"
               "         --dump-sir, --stats=json\n",
               out);
}

std::string versionText() {
    return std::string("haremasim ") + haremasim::driver::kVersion + " (commit " +
           haremasim::driver::kCommit + ")";
}

void printVersion() {
    std::printf("%s, built with %s %s\n", versionText().c_str(), haremasim::driver::kCompilerId,
                haremasim::driver::kCompilerVersion);
}

struct Options {
    std::string command;                 // "" (compile and run) or "parse"
    std::vector<std::string> slangArgs;  // files and options handed to slang
    std::vector<std::string> plusargs;   // run-time plusargs, in order
    std::vector<std::string> original;   // the arguments as given, for the version line
    bool statsJson = false;
    bool dumpSir = false;
};

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
        if (arg == "--stats=json") {
            o.statsJson = true;
        } else if (arg == "--dump-sir") {
            o.dumpSir = true;
        } else if (arg == "-f" || arg == "--top") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "haremasim: error: %s needs a value\n", argv[i]);
                return kExitInputError;
            }
            o.slangArgs.emplace_back(arg);
            o.slangArgs.emplace_back(argv[++i]);
            o.original.emplace_back(argv[i]);
        } else if (arg.starts_with("--top=")) {
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with("+")) {
            if (isSlangPlusOption(arg))
                o.slangArgs.emplace_back(arg);
            else
                o.plusargs.emplace_back(arg);
        } else if (arg.starts_with("-")) {
            std::fprintf(stderr, "haremasim: error: unknown option '%s'\n", argv[i]);
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

int runParse(haremasim::kernel::Context& context, const Options& o) {
    if (o.slangArgs.empty()) {
        std::fputs("haremasim: error: parse needs at least one file\n", stderr);
        return kExitInputError;
    }
    const auto result = haremasim::front::parseAndElaborate(o.slangArgs);
    std::fputs(result.diagnostics.c_str(), stderr);

    auto& counters = context.counters();
    counters.set(counters.add("front.files"), result.numFiles);
    counters.set(counters.add("front.syntax_trees"), result.numSyntaxTrees);
    counters.set(counters.add("front.errors"), result.numErrors);
    counters.set(counters.add("front.warnings"), result.numWarnings);

    return result.success ? kExitOk : kExitInputError;
}

int runDesign(haremasim::kernel::Context& context, const Options& o) {
    if (o.slangArgs.empty()) {
        std::fputs("haremasim: error: no input files\n", stderr);
        printUsage(stderr);
        return kExitInputError;
    }
    if (!o.dumpSir) {
        std::string line = "haremasim: info: " + versionText() + " args:";
        for (const auto& a : o.original)
            line += " " + a;
        std::fprintf(stderr, "%s\n", line.c_str());
    }

    const auto compiled = haremasim::front::compile(o.slangArgs);
    std::fputs(compiled.diagnostics.c_str(), stderr);
    if (!compiled.verifyErrors.empty()) {
        std::fputs(compiled.verifyErrors.c_str(), stderr);
        return kExitInternalError;
    }
    if (!compiled.success) return kExitInputError;

    if (o.dumpSir) {
        std::fputs(haremasim::sir::toText(compiled.design).c_str(), stdout);
        return kExitOk;
    }

    const auto result = haremasim::vm::run(compiled.design, context);
    haremasim::kernel::flushOut();
    const auto reason = result.reason;
    if (reason == haremasim::kernel::EndReason::Internal) {
        std::fprintf(stderr, "haremasim: internal error: %s\n", result.error.c_str());
    } else {
        const bool silent = (reason == haremasim::kernel::EndReason::Finish ||
                             reason == haremasim::kernel::EndReason::Stop) &&
                            context.scheduler().level() == 0;
        if (!silent) {
            std::fprintf(stderr, "haremasim: info: %s at time %llu\n",
                         haremasim::kernel::endReasonName(reason),
                         static_cast<unsigned long long>(context.scheduler().time()));
        }
    }
    return haremasim::kernel::exitCode(reason);
}

int run(int argc, char** argv) {
    Options o;
    if (const int code = parseArgs(argc, argv, o); code >= 0) return code;

    haremasim::kernel::Context context;
    const int code = o.command == "parse" ? runParse(context, o) : runDesign(context, o);
    if (o.statsJson) std::fputs(context.counters().toJson().c_str(), stdout);
    return code;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "haremasim: internal error: %s\n", e.what());
    } catch (...) {
        std::fputs("haremasim: internal error: unknown exception\n", stderr);
    }
    return kExitInternalError;
}
