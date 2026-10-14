// The haremasim command line.
//
//   haremasim --version
//   haremasim [--stats=json] parse <file>...
//
// Exit codes follow the design: 0 on success, 2 for command line or input
// errors (including syntax errors), 3 for internal errors.

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include "build_info.h"
#include "haremasim/front/front.h"
#include "haremasim/kernel/context.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitInputError = 2;
constexpr int kExitInternalError = 3;

void printUsage(std::FILE* out) {
    std::fputs("usage: haremasim --version\n"
               "       haremasim [--stats=json] parse <file>...\n",
               out);
}

void printVersion() {
    std::printf("haremasim %s (commit %s, built with %s %s)\n", haremasim::driver::kVersion,
                haremasim::driver::kCommit, haremasim::driver::kCompilerId,
                haremasim::driver::kCompilerVersion);
}

int runParse(haremasim::kernel::Context& context, const std::vector<std::string>& files) {
    if (files.empty()) {
        std::fputs("haremasim: error: parse needs at least one file\n", stderr);
        return kExitInputError;
    }

    const auto result = haremasim::front::parseAndElaborate(files);
    std::fputs(result.diagnostics.c_str(), stderr);

    auto& counters = context.counters();
    counters.set(counters.add("front.files"), result.numFiles);
    counters.set(counters.add("front.syntax_trees"), result.numSyntaxTrees);
    counters.set(counters.add("front.errors"), result.numErrors);
    counters.set(counters.add("front.warnings"), result.numWarnings);

    return result.success ? kExitOk : kExitInputError;
}

int run(int argc, char** argv) {
    bool statsJson = false;
    std::string command;
    std::vector<std::string> operands;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--version") {
            printVersion();
            return kExitOk;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage(stdout);
            return kExitOk;
        }
        if (arg == "--stats=json") {
            statsJson = true;
        } else if (arg.starts_with("-")) {
            std::fprintf(stderr, "haremasim: error: unknown option '%s'\n", argv[i]);
            printUsage(stderr);
            return kExitInputError;
        } else if (command.empty()) {
            command = arg;
        } else {
            operands.emplace_back(arg);
        }
    }

    if (command != "parse") {
        if (!command.empty())
            std::fprintf(stderr, "haremasim: error: unknown command '%s'\n", command.c_str());
        printUsage(stderr);
        return kExitInputError;
    }

    haremasim::kernel::Context context;
    const int code = runParse(context, operands);
    if (statsJson) std::fputs(context.counters().toJson().c_str(), stdout);
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
