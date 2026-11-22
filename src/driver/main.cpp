// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The kirimasim command line (CLI.md).
//
//   kirimasim [options] <file>... [+plusarg]...   compile and run
//   kirimasim parse [options] <file>...          parse and elaborate only
//   kirimasim --version | --help
//
// Options so far: -f, -I, -D, -y, --libdir, +incdir+, +define+, --top,
// --timescale and the four elaboration limits (passed to slang), --max-time,
// --max-time-ok, --max-zero-delay, --max-slot-events,
// --max-steps, --max-scc-iter, --max-call-depth, --seed, --dump-sir,
// --stats=json, --mintypmax, --bounds-check, --cond-x, --autoflush, --stop,
// --max-wall-time, --wave, --wave-scope and --wave-glitches. Exit codes
// are those of CLI.md 5.

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <pthread.h>

#include "build_info.h"
#include "kirimasim/analysis/islands.h"
#include "kirimasim/analysis/race.h"
#include "kirimasim/analysis/values.h"
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/text_v1.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/vm/vm.h"
#include "native_model.h"
#include "race_files.h"
#include "race_trace.h"
#include "vpi_loader.h"
#include <climits>
#include <unistd.h>

namespace {

constexpr int kExitOk = 0;
constexpr int kExitInputError = 2;
constexpr int kExitInternalError = 3;
constexpr int kExitResourceLimit = 6;

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
    std::fputs("usage: kirimasim [options] <file>... [+plusarg]...\n"
               "       kirimasim parse [options] <file>...\n"
               "       kirimasim --load-sir=<file> [simulation options] [+plusarg]...\n"
               "       kirimasim --version\n"
               "options: -f <file>, -I<dir>, -I <dir>, -D<name>[=<value>], -D <name>[=<value>],\n"
               "         -y<dir>, -y <dir>, --libdir=<dir>, +incdir+<dir>,\n"
               "         +define+<name>[=<value>], +libext+<ext>, --top=<name>,\n"
               "         --vpi=<file>,\n"
               "         -W<name>, -Wno-<name>, -Werror[=<name>], --compat=default|vcs|all,\n"
               "         --max-hierarchy-depth=<n>, --max-generate-steps=<n>,\n"
               "         --max-constexpr-depth=<n>, --max-constexpr-steps=<n>,\n"
               "         --error-limit=<n>, --single-unit,\n"
               "         --timescale=<unit>/<precision>, --max-time=<time>, --max-time-ok,\n"
               "         --max-zero-delay=<n>, --max-slot-events=<n>, --max-steps=<n>,\n"
               "         --max-scc-iter=<n>, --max-call-depth=<n>, --seed=<n>, --dump-sir[=0|1],\n"
               "         --stats=json, --mintypmax=min|typ|max, --bounds-check=off|warn,\n"
               "         --cond-x=off|warn|error, --init-values=x|zero|one|random, --autoflush,\n"
               "         --stop=finish|ignore|error, --max-wall-time=<seconds>, --max-procs=<n>,\n"
               "         --race-check=<n>, --race-seed=<n>, --race-nba, --public=<pattern>,\n"
               "         --preserve=none|ports|all,\n"
               "         --tier=vm|auto|native, --tier-check, --state=2|4, --cxx=<compiler>, "
               "--cache-dir=<dir>,\n"
               "         --enable-pass=islands, --disable-pass=islands,\n"
               "         --report-islands=<file>, --report-state-opt, --report-pulses, "
               "--wave=<file>.vcd|.fst,\n"
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

enum class DumpFormat { Legacy, Canonical };

struct Options {
    std::string command;  // "" (compile and run) or "parse"
    std::vector<std::string> vpiFiles;
    std::vector<std::string> slangArgs;  // files and options handed to slang
    std::vector<std::string> plusargs;   // run-time plusargs, in order
    std::vector<std::string> original;   // the arguments as given, for the version line
    bool islands = false;
    std::optional<bool> islandSwitch;
    kirimasim::driver::NativeOptions native;
    bool reportState = false;
    std::string executable;
    std::uint64_t raceCount = 0, raceSeed = 0;
    int raceRecord = -1;
    std::optional<bool> racePipe;
    std::string raceDirectory;
    bool raceNba = false;
    std::vector<std::string> publicNames;
    bool blockingRaceWarning = false;
    std::optional<bool> blockingRaceError;
    bool warningsAsErrors = false;
    enum class Preserve { None, Ports, All };
    Preserve preserve = Preserve::None;
    std::string islandReport;
    bool statsJson = false;
    std::optional<DumpFormat> dumpSir;
    std::string sirFile;
    std::string maxTime;  // as given; converted once the time precision is known
    bool maxTimeOk = false;
    std::uint64_t maxZeroDelay = 1000000;  // the defaults of CLI.md 2.6; 0: no limit
    std::uint64_t maxSlotEvents = 100000000;
    std::uint64_t maxSteps = 1000000000;
    std::uint64_t maxWallTime = 0;
    std::uint64_t maxProcs = kirimasim::kernel::Limits{}.maxProcs;
    std::uint64_t maxSccIter = 65536;
    std::uint64_t maxCallDepth = kirimasim::vm::kDefaultCallDepth;  // CLI.md 2.7
    std::uint64_t seed = 0;                                         // --seed (SYSTEM_TASKS.md 13.2)
    std::string waveFile;                                           // --wave
    std::vector<std::string> waveScopes;                            // --wave-scope
    bool waveGlitches = false;
    bool boundsCheck = false;
    kirimasim::vm::CondXMode condX = kirimasim::vm::CondXMode::Off;
    kirimasim::vm::InitValuesMode initValues = kirimasim::vm::InitValuesMode::X;
    bool autoFlush = false;
    bool reportPulses = false;
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
    o.executable = argv[0];
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
              {"--max-procs", &o.maxProcs},
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
        } else if (arg == "--dump-sir" || arg == "--dump-sir=0") {
            o.dumpSir = DumpFormat::Legacy;
        } else if (arg == "--dump-sir=1") {
            o.dumpSir = DumpFormat::Canonical;
        } else if (arg.starts_with("--dump-sir=")) {
            std::fputs("kirimasim: error: --dump-sir needs format 0 or 1\n", stderr);
            return kExitInputError;
        } else if (arg.starts_with("--load-sir=")) {
            const auto path = arg.substr(std::string_view("--load-sir=").size());
            if (path.empty() || !o.sirFile.empty()) {
                std::fputs("kirimasim: error: --load-sir needs exactly one file\n", stderr);
                return kExitInputError;
            }
            o.sirFile = path;
        } else if (arg.starts_with("--race-check=") || arg.starts_with("--race-seed=")) {
            const bool count = arg.starts_with("--race-check=");
            auto& value = count ? o.raceCount : o.raceSeed;
            if (!parseCount(arg, count ? "--race-check" : "--race-seed", value) ||
                (count && !value)) {
                std::fprintf(stderr, "kirimasim: error: invalid race option %s\n", argv[i]);
                return kExitInputError;
            }
        } else if (arg == "--race-nba") {
            o.raceNba = true;
        } else if (arg == "--race-pipe=ended" || arg == "--race-pipe=prefix") {
            o.racePipe = arg == "--race-pipe=ended";
        } else if (arg.starts_with("--race-directory=")) {
            o.raceDirectory = arg.substr(17);
        } else if (arg.starts_with("--race-record-fd=")) {
            std::uint64_t fd = 0;
            if (!parseCount(arg, "--race-record-fd", fd) || fd < 3 || fd > INT_MAX)
                return kExitInputError;
            o.raceRecord = static_cast<int>(fd);
        } else if (arg.starts_with("--preserve=")) {
            const auto choice = arg.substr(11);
            if (choice == "none")
                o.preserve = Options::Preserve::None;
            else if (choice == "ports")
                o.preserve = Options::Preserve::Ports;
            else if (choice == "all")
                o.preserve = Options::Preserve::All;
            else {
                std::fputs("kirimasim: error: --preserve needs none, ports or all\n", stderr);
                return kExitInputError;
            }
        } else if (arg.starts_with("--public=")) {
            const auto pattern = arg.substr(9);
            if (pattern.empty()) {
                std::fputs("kirimasim: error: --public needs a name pattern\n", stderr);
                return kExitInputError;
            }
            o.publicNames.emplace_back(pattern);
        } else if (arg.starts_with("--state=")) {
            if (arg == "--state=2")
                o.native.approximateTwoState = true;
            else if (arg == "--state=4")
                o.native.approximateTwoState = false;
            else {
                std::fputs("kirimasim: error: --state needs 2 or 4\n", stderr);
                return kExitInputError;
            }
        } else if (arg.starts_with("--tier=")) {
            using kirimasim::driver::Tier;
            const auto tier = arg.substr(7);
            if (tier == "vm")
                o.native.tier = Tier::Vm;
            else if (tier == "auto")
                o.native.tier = Tier::Auto;
            else if (tier == "native")
                o.native.tier = Tier::Native;
            else {
                std::fputs("kirimasim: error: --tier needs vm, auto or native\n", stderr);
                return kExitInputError;
            }
        } else if (arg == "--tier-check") {
            o.native.check = true;
        } else if (arg.starts_with("--cxx=") || arg.starts_with("--cache-dir=")) {
            const auto choice = arg.substr(arg.find('=') + 1);
            if (choice.empty()) {
                std::fprintf(stderr, "kirimasim: error: option needs a value: %s\n", argv[i]);
                return kExitInputError;
            }
            if (arg.starts_with("--cxx="))
                o.native.compiler = choice;
            else
                o.native.cache = choice;
        } else if (arg.starts_with("--enable-pass=") || arg.starts_with("--disable-pass=")) {
            if (arg.substr(arg.find('=') + 1) != "islands") {
                std::fprintf(stderr, "kirimasim: error: unknown optimization pass in %s\n",
                             argv[i]);
                return kExitInputError;
            }
            o.islandSwitch = arg.starts_with("--enable-pass=");
        } else if (arg.starts_with("--report-islands=")) {
            o.islandReport = arg.substr(std::string_view("--report-islands=").size());
            if (o.islandReport.empty()) {
                std::fputs("kirimasim: error: --report-islands needs a file\n", stderr);
                return kExitInputError;
            }
        } else if (arg == "--report-state-opt") {
            o.reportState = true;
        } else if (arg == "--report-pulses") {
            o.reportPulses = true;
        } else if (arg.starts_with("--bounds-check=")) {
            const auto choice = arg.substr(std::string_view("--bounds-check=").size());
            if (choice != "off" && choice != "warn") {
                std::fprintf(stderr, "kirimasim: error: %s needs off or warn\n", argv[i]);
                return kExitInputError;
            }
            o.boundsCheck = choice == "warn";
        } else if (arg.starts_with("--cond-x=")) {
            const auto choice = arg.substr(std::string_view("--cond-x=").size());
            if (choice == "off")
                o.condX = kirimasim::vm::CondXMode::Off;
            else if (choice == "warn")
                o.condX = kirimasim::vm::CondXMode::Warn;
            else if (choice == "error")
                o.condX = kirimasim::vm::CondXMode::Error;
            else {
                std::fprintf(stderr, "kirimasim: error: %s needs off, warn or error\n", argv[i]);
                return kExitInputError;
            }
        } else if (arg.starts_with("--init-values=")) {
            const auto choice = arg.substr(std::string_view("--init-values=").size());
            if (choice == "x")
                o.initValues = kirimasim::vm::InitValuesMode::X;
            else if (choice == "zero")
                o.initValues = kirimasim::vm::InitValuesMode::Zero;
            else if (choice == "one")
                o.initValues = kirimasim::vm::InitValuesMode::One;
            else if (choice == "random")
                o.initValues = kirimasim::vm::InitValuesMode::Random;
            else {
                std::fprintf(stderr, "kirimasim: error: %s needs x, zero, one or random\n",
                             argv[i]);
                return kExitInputError;
            }
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
            // Explicit format selection (approved FST contract 1).
            o.waveFile = arg.substr(std::string_view("--wave=").size());
            const auto basename = o.waveFile.substr(o.waveFile.find_last_of('/') + 1);
            if ((!o.waveFile.ends_with(".vcd") && !o.waveFile.ends_with(".fst")) ||
                basename == ".vcd" || basename == ".fst") {
                std::fprintf(stderr, "kirimasim: error: %s needs a file ending in .vcd or .fst\n",
                             argv[i]);
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
        } else if (arg.starts_with("-I") || arg.starts_with("-D") || arg.starts_with("-y") ||
                   arg.starts_with("--libdir=")) {
            const bool longDirectory = arg.starts_with("--libdir=");
            const auto name = longDirectory ? std::string_view("--libdir") : arg.substr(0, 2);
            const bool separate = arg == name;
            std::string_view value = arg.substr(name.size() + (longDirectory ? 1 : 0));
            if (separate) {
                if (i + 1 >= argc) {
                    std::fprintf(stderr, "kirimasim: error: %.*s needs a value\n",
                                 static_cast<int>(name.size()), name.data());
                    return kExitInputError;
                }
                value = argv[++i];
                o.original.emplace_back(value);
            }
            if (value.empty() || (name == "-D" && value.front() == '=')) {
                std::fprintf(stderr, "kirimasim: error: %.*s needs a nonempty %s\n",
                             static_cast<int>(name.size()), name.data(),
                             name == "-D" ? "macro name" : "directory");
                return kExitInputError;
            }
            o.slangArgs.emplace_back(arg);
            if (separate) o.slangArgs.emplace_back(value);
        } else if (arg == "-f" || arg == "--top") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "kirimasim: error: %s needs a value\n", argv[i]);
                return kExitInputError;
            }
            o.slangArgs.emplace_back(arg);
            o.slangArgs.emplace_back(argv[++i]);
            o.original.emplace_back(argv[i]);
        } else if (arg.starts_with("--vpi=")) {
            const auto path = arg.substr(6);
            if (path.empty()) {
                std::fputs("kirimasim: error: --vpi needs a nonempty path\n", stderr);
                return kExitInputError;
            }
            o.vpiFiles.emplace_back(path);
        } else if (arg == "-Wkirima-race-blocking" || arg == "-Wno-kirima-race-blocking") {
            o.blockingRaceWarning = arg == "-Wkirima-race-blocking";
        } else if (arg == "-Werror=kirima-race-blocking") {
            o.blockingRaceWarning = true;
            o.blockingRaceError = true;
        } else if (arg == "-Wno-error=kirima-race-blocking") {
            o.blockingRaceError = false;
        } else if (arg.starts_with("-W")) {
            if (arg.size() == 2) {
                std::fputs("kirimasim: error: -W needs a warning name\n", stderr);
                return kExitInputError;
            }
            if (arg == "-Werror" || arg == "-Wno-error") o.warningsAsErrors = arg == "-Werror";
            // Other names are handled by slang (CLI.md 2.1).
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with("--compat=")) {
            // CLI.md 2.1 delegates compatibility modes to the pinned frontend.
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with("--max-hierarchy-depth=") ||
                   arg.starts_with("--max-generate-steps=") ||
                   arg.starts_with("--max-constexpr-depth=") ||
                   arg.starts_with("--max-constexpr-steps=")) {
            // CLI.md 2.1: slang owns elaboration limits and their diagnostics.
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with("--error-limit=") || arg == "--single-unit") {
            // CLI.md 2.1: slang owns the number of errors it reports before
            // it stops, and how the files make compilation units (3.12.1).
            // With no number slang would take the next argument for it.
            if (arg == "--error-limit=") {
                std::fputs("kirimasim: error: --error-limit needs a number\n", stderr);
                return kExitInputError;
            }
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with("--top=") || arg.starts_with("--timescale=")) {
            o.slangArgs.emplace_back(arg);
        } else if (arg.starts_with('+')) {
            if (isSlangPlusOption(arg))
                o.slangArgs.emplace_back(arg);
            else
                o.plusargs.emplace_back(arg);
        } else if (arg.starts_with('-')) {
            std::fprintf(stderr, "kirimasim: error: unknown option '%s'\n", argv[i]);
            printUsage(stderr);
            return kExitInputError;
        } else if (o.command.empty() && o.slangArgs.empty() && arg == "parse") {
            o.command = "parse";
        } else {
            o.slangArgs.emplace_back(arg);
        }
    }
    if (!o.sirFile.empty() && (!o.slangArgs.empty() || !o.command.empty())) {
        std::fputs(
            "kirimasim: error: --load-sir cannot take source files, frontend options or parse\n",
            stderr);
        return kExitInputError;
    }
    if (o.dumpSir == DumpFormat::Canonical && (o.statsJson || !o.command.empty())) {
        std::fputs("kirimasim: error: --dump-sir=1 cannot be combined with --stats=json or parse\n",
                   stderr);
        return kExitInputError;
    }
    o.islands = o.islandSwitch.value_or(true);
    return -1;
}

std::vector<kirimasim::front::ForeignRoutine>
foreignDeclarations(kirimasim::kernel::Context& context) {
    std::vector<kirimasim::front::ForeignRoutine> out;
    for (const auto& routine : context.vpi().routines()) {
        kirimasim::sir::Type type = kirimasim::sir::voidType();
        if (!routine.task)
            type = routine.result.real ? kirimasim::sir::Type{kirimasim::sir::TypeKind::Real, 0}
                   : routine.result.signedValue ? kirimasim::sir::s4(routine.result.width)
                                                : kirimasim::sir::u4(routine.result.width);
        out.push_back({routine.name, type});
    }
    return out;
}
void configureVpi(kirimasim::kernel::Context& context, const Options& o, int precision) {
    if (o.vpiFiles.empty()) return;
    std::vector<std::string> argv{"kirimasim"};
    argv.insert(argv.end(), o.original.begin(), o.original.end());
    context.vpi().configure(std::move(argv), versionText(), precision);
}
int runParse(kirimasim::kernel::Context& context, const Options& o) {
    if (!o.vpiFiles.empty()) {
        const auto declarations = foreignDeclarations(context);
        const auto result = kirimasim::front::compile(
            o.slangArgs, {.foreignRoutines = declarations, .foreignEnabled = true});
        std::fputs(result.diagnostics.c_str(), stderr);
        if (!result.verifyErrors.empty()) {
            std::fputs(result.verifyErrors.c_str(), stderr);
            return kExitInternalError;
        }
        if (!result.success || !result.verified.has_value()) return kExitInputError;
        configureVpi(context, o, result.design().timePrecision);
        const auto prepared =
            kirimasim::vm::run(*result.verified, context, {.vpiCompileOnly = true});
        if (!prepared.error.empty()) {
            std::fprintf(stderr, "kirimasim: error: VPI: %s\n", prepared.error.c_str());
            return kExitInputError;
        }
        return prepared.reason == kirimasim::kernel::EndReason::Quiescent ? kExitOk
                                                                          : kExitInputError;
    }

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

int readDesign(const Options& o, std::optional<kirimasim::sir::VerifiedDesign>& verified) {
    std::ifstream input(o.sirFile, std::ios::binary);
    if (!input) {
        std::fprintf(stderr, "kirimasim: error: cannot open SIR file '%s'\n", o.sirFile.c_str());
        return kExitInputError;
    }
    std::string text;
    std::array<char, 16384> chunk;
    do {
        input.read(chunk.data(), chunk.size());
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > kirimasim::sir::kMaxTextV1Bytes - text.size()) {
            std::fputs("kirimasim: error: SIR text size limit exceeded\n", stderr);
            return kExitInputError;
        }
        text.append(chunk.data(), count);
    } while (input);
    if (input.bad() || !input.eof()) {
        std::fprintf(stderr, "kirimasim: error: cannot read SIR file '%s'\n", o.sirFile.c_str());
        return kExitInputError;
    }
    auto parsed = kirimasim::sir::parse1(text);
    for (const auto& error : parsed.errors)
        std::fprintf(stderr, "kirimasim: error: %s: %s: %s\n", o.sirFile.c_str(),
                     error.where.c_str(), error.message.c_str());
    if (!parsed.verified) return kExitInputError;
    const auto& design = parsed.verified->design();
    for (const auto& [missing, option] :
         {std::pair{o.condX != kirimasim::vm::CondXMode::Off && !design.condXInstrumentation,
                    "--cond-x"},
          std::pair{o.initValues != kirimasim::vm::InitValuesMode::X &&
                        !design.initValuesInstrumentation,
                    "--init-values"},
          std::pair{!o.vpiFiles.empty() && !design.foreignReflection, "--vpi"}}) {
        if (missing) {
            std::fprintf(stderr, "kirimasim: error: saved SIR lacks instrumentation for %s\n",
                         option);
            return kExitInputError;
        }
    }
    verified = std::move(parsed.verified);
    return kExitOk;
}

int prepareDesign(kirimasim::kernel::Context& context, const Options& o,
                  std::optional<kirimasim::sir::VerifiedDesign>& verified) {
    if (!o.sirFile.empty()) {
        if (o.preserve == Options::Preserve::Ports) {
            std::fputs("kirimasim: error: use --preserve=ports when compiling source; saved SIR "
                       "retains those marks\n",
                       stderr);
            return kExitInputError;
        }
        return readDesign(o, verified);
    }
    const auto declarations = foreignDeclarations(context);
    auto compiled = kirimasim::front::compile(
        o.slangArgs, {.foreignRoutines = declarations,
                      .foreignEnabled = !o.vpiFiles.empty(),
                      .condX = o.condX != kirimasim::vm::CondXMode::Off,
                      .initValues = o.initValues != kirimasim::vm::InitValuesMode::X,
                      .preservePorts = o.preserve == Options::Preserve::Ports});
    std::fputs(compiled.diagnostics.c_str(), stderr);
    if (!compiled.verifyErrors.empty()) {
        std::fputs(compiled.verifyErrors.c_str(), stderr);
        return kExitInternalError;
    }
    verified = std::move(compiled.verified);
    return verified ? kExitOk : kExitInputError;
}

int runDesign(kirimasim::kernel::Context& context, const Options& o) {
    if (o.slangArgs.empty() && o.sirFile.empty()) {
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
        if (o.initValues != kirimasim::vm::InitValuesMode::X) {
            const std::string_view choice =
                o.initValues == kirimasim::vm::InitValuesMode::Zero  ? "zero"
                : o.initValues == kirimasim::vm::InitValuesMode::One ? "one"
                                                                     : "random";
            std::fprintf(stderr,
                         "kirimasim: note: --init-values=%.*s: 4-state variables with no initial "
                         "value start as %.*s, not x (VALUE_SEMANTICS.md 1.4)\n",
                         static_cast<int>(choice.size()), choice.data(),
                         static_cast<int>(choice.size()), choice.data());
        }
    }

    std::optional<kirimasim::sir::VerifiedDesign> verified;
    if (const int code = prepareDesign(context, o, verified); code != kExitOk) return code;
    if (!verified) {
        std::fputs("kirimasim: internal error: successful input preparation returned no design\n",
                   stderr);
        return kExitInternalError;
    }
    if (!o.publicNames.empty() || o.preserve == Options::Preserve::All) {
        auto marked = std::move(*verified).release();
        for (const auto& name : marked.names)
            if (o.preserve == Options::Preserve::All ||
                std::ranges::any_of(o.publicNames, [&](const auto& pattern) {
                    return kirimasim::kernel::globMatch(pattern, name.path);
                })) {
                marked.slots[name.slot.index()].flags |= kirimasim::sir::kObservable;
                marked.slots[kirimasim::sir::physicalSlot(marked, name.slot).index()].flags |=
                    kirimasim::sir::kObservable;
            }
        verified = kirimasim::sir::verifyDesign(std::move(marked)).verified;
        if (!verified) return kExitInternalError;
    }
    // The size of the SIR (PERFORMANCE.md 3.2).
    const kirimasim::sir::Design& d = verified->design();
    if (o.blockingRaceWarning) {
        const auto races = kirimasim::analysis::blockingRaces(*verified);
        const auto location = [&](kirimasim::sir::SourceLoc loc) {
            return (loc.file < d.sources.size() ? d.sources[loc.file] : std::string{"<unknown>"}) +
                   ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column);
        };
        const bool error = o.blockingRaceError.value_or(o.warningsAsErrors);
        for (const auto& race : races)
            std::fprintf(stderr,
                         "%s: %s: blocking write to %s by %s and read by %s at %s on the same %s "
                         "of %s [-Wkirima-race-blocking]\n",
                         location(race.write).c_str(), error ? "error" : "warning",
                         d.slot(race.slot).name.c_str(), d.process(race.writer).name.c_str(),
                         d.process(race.reader).name.c_str(), location(race.read).c_str(),
                         race.edge == kirimasim::sir::Edge::Pos ? "posedge" : "negedge",
                         d.slot(race.clock).name.c_str());
        if (error && !races.empty()) return kExitInputError;
    }
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

    std::optional<kirimasim::analysis::TableData> table;
    std::vector<kirimasim::analysis::IterationRow> iterations;
    if (o.islands || !o.islandReport.empty())
        table = kirimasim::analysis::analyzeIslands(*verified, &iterations);
    kirimasim::driver::IslandModules modules;
    if (!o.dumpSir && table && o.islands &&
        (o.native.tier != kirimasim::driver::Tier::Vm || o.native.check)) {
        const auto error = modules.prepare(*verified, *table, o.native, o.maxSteps, o.boundsCheck,
                                           !o.vpiFiles.empty());
        if (!error.empty()) {
            std::fprintf(stderr, "kirimasim: error: %s\n", error.c_str());
            return kExitInputError;
        }
    }
    if (!o.islandReport.empty()) {
        std::ofstream report(o.islandReport);
        report << kirimasim::analysis::islandReport(d, table ? &*table : nullptr,
                                                    modules.unsupported(), iterations);
        report.close();
        if (!report) {
            std::fprintf(stderr, "kirimasim: error: cannot write island report %s\n",
                         o.islandReport.c_str());
            return kExitInputError;
        }
    }
    configureVpi(context, o, d.timePrecision);
    if (o.dumpSir) {
        if (!o.vpiFiles.empty()) {
            const auto prepared = kirimasim::vm::run(*verified, context, {.vpiCompileOnly = true});
            if (!prepared.error.empty()) {
                std::fprintf(stderr, "kirimasim: error: VPI: %s\n", prepared.error.c_str());
                return kExitInputError;
            }
        }
        if (o.dumpSir == DumpFormat::Canonical) {
            const auto text = kirimasim::sir::print1(*verified);
            if (!text) {
                std::fputs("kirimasim: error: design cannot be represented in SIR format 1\n",
                           stderr);
                return kExitInputError;
            }
            if (std::fwrite(text->data(), 1, text->size(), stdout) != text->size() ||
                std::fflush(stdout) != 0) {
                std::fputs("kirimasim: error: cannot write SIR output\n", stderr);
                return kExitInputError;
            }
        } else {
            std::fputs(kirimasim::sir::toText(d).c_str(), stdout);
        }
        return kExitOk;
    }

    const int precision = verified->design().timePrecision;
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
    limits.maxSccIter = o.maxSccIter;
    limits.maxProcs = o.maxProcs;
    context.scheduler().setLimits(limits);
    kirimasim::vm::RunOptions run;
    if (o.islands && table) {
        for (const auto& member : table->members) {
            run.islands.push_back(
                {member.process, member.island, member.level,
                 kirimasim::analysis::islandSensitivity(d, d.process(member.process))});
            if (!iterations.empty())
                run.islands.back().component = iterations[member.process.index()].component;
        }
    }
    run.tierCheck = o.native.check;
    run.approximateTwoState = o.native.approximateTwoState;
    modules.bind(run);
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
    run.condX = o.condX;
    run.initValues = o.initValues;
    run.reportPulses = o.reportPulses;
    run.seed = o.seed;
    // SIGINT stops the run from here on; before, while compiling, it ends
    // kirimasim as the system does by default (SCHEDULER_SPEC.md 16.2).
    context.scheduler().setInterruptFlag(&interruptRequested);
    std::signal(SIGINT, kirimasimOnInterrupt);
    if (o.maxWallTime != 0)
        context.scheduler().setWallTimeLimit(o.maxWallTime, std::chrono::steady_clock::now());
    if (o.reportState) context.scheduler().recordIslandStates();
    namespace k = kirimasim::kernel;
    using kirimasim::driver::RaceFile;
    RaceFile trace(nullptr, std::fclose), input(nullptr, std::fclose);
    std::optional<kirimasim::driver::RaceTrace> recorder;
    std::optional<kirimasim::driver::RaceFiles> raceFiles;
    if ((!o.raceDirectory.empty()) != (o.raceRecord >= 0)) return kExitInputError;
    if ((o.raceCount && o.raceRecord >= 0) || (o.racePipe && o.raceRecord < 0))
        return kExitInputError;
    kirimasim::driver::RaceInput inputState;
    if (o.raceCount || o.raceRecord >= 0) {
        trace.reset(o.raceRecord >= 0 ? fdopen(dup(o.raceRecord), "wb") : std::tmpfile());
        if (o.raceCount) input.reset(std::tmpfile());
        if (!trace || (o.raceCount && !input)) {
            std::fputs("kirimasim: error: cannot open race recording\n", stderr);
            return kExitInputError;
        }
        raceFiles.emplace(o.raceDirectory);
        if (!raceFiles->error().empty()) {
            std::fprintf(stderr, "kirimasim: error: %s\n", raceFiles->error().c_str());
            return kExitInputError;
        }
        context.files().setPathResolver(&kirimasim::driver::RaceFiles::resolve, &*raceFiles);
        recorder.emplace(trace.get());
        context.scheduler().setRaceHook(recorder->hook());
        context.output().setRaceScheduler(&context.scheduler());
        if (input && !kirimasim::driver::snapshotInput(stdin, input.get(), inputState)) {
            std::fputs("kirimasim: error: cannot snapshot standard input for race exploration\n",
                       stderr);
            return kExitInputError;
        }
        if (inputState.offset < 0) context.files().recordInput(input.get());
        if (o.racePipe) context.files().replayPipe(*o.racePipe);
        if (o.raceRecord >= 0) context.scheduler().setRaceOrder(o.raceSeed, o.raceNba);
    }
    const auto result = kirimasim::vm::run(*verified, context, run);
    if (recorder) {
        context.scheduler().raceEvent(k::RaceKind::End, static_cast<std::uint32_t>(result.reason),
                                      static_cast<std::uint32_t>(context.scheduler().level()));
        context.scheduler().setRaceHook({});
        context.output().setRaceScheduler(nullptr);
        inputState.ended = context.files().inputEnded();
        context.files().recordInput(nullptr);
        if (context.files().missingReplayInput()) {
            std::fputs(
                "kirimasim: error: replay requires more input than the recorded pipe supplied\n",
                stderr);
            return kExitInternalError;
        }
        context.files().setPathResolver(nullptr, nullptr);
        if (!raceFiles || !raceFiles->error().empty()) {
            std::fprintf(stderr, "kirimasim: error: %s\n",
                         raceFiles ? raceFiles->error().c_str() : "missing race files");
            return kExitInternalError;
        }
        if (!recorder->flush() ||
            (input && (std::ferror(input.get()) || std::fflush(input.get())))) {
            std::fputs("kirimasim: error: race recording failed\n", stderr);
            return kExitInputError;
        }
        if (o.raceCount && !kirimasim::driver::exploreRaces(
                               o.executable, o.original, d, trace.get(), input.get(), o.raceCount,
                               o.raceSeed, raceFiles->baseline().string(), inputState)) {
            std::fputs("kirimasim: error: race replay did not produce a complete trace\n", stderr);
            return kExitInternalError;
        }
    }
    context.output().flush();
    if (o.reportState) {
        const auto storage =
            kirimasim::analysis::stateStorageReport(*verified, !o.waveFile.empty());
        std::fputs(storage.c_str(), stderr);
        const auto counts = context.scheduler().islandStates();
        for (std::size_t i = 0; i < counts.size(); ++i) {
            const auto& c = counts[i];
            const auto total = c.two + c.four;
            const auto fraction =
                total ? std::to_string(c.two) + "/" + std::to_string(total) : "n/a";
            std::fprintf(stderr, "state island %zu two=%llu four=%llu fraction=%s\n", i,
                         static_cast<unsigned long long>(c.two),
                         static_cast<unsigned long long>(c.four), fraction.c_str());
        }
    }
    const auto reason = result.reason;
    if (reason == kirimasim::kernel::EndReason::Internal) {
        std::fprintf(stderr, "kirimasim: internal error: %s\n", result.error.c_str());
    } else {
        if ((reason == kirimasim::kernel::EndReason::WallTimeLimit ||
             reason == kirimasim::kernel::EndReason::ResourceLimit) &&
            !context.scheduler().endNote().empty())
            std::fprintf(stderr, "kirimasim: error: %s\n", context.scheduler().endNote().c_str());
        else if (reason == kirimasim::kernel::EndReason::ResourceLimit &&
                 context.scheduler().memoryRanOut())
            std::fputs("kirimasim: error: the memory for the run ran out\n", stderr);
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
    if (const auto error = kirimasim::driver::loadVpi(context, o.vpiFiles); !error.empty()) {
        std::fprintf(stderr, "kirimasim: error: VPI: %s\n", error.c_str());
        return context.scheduler().reason() == kirimasim::kernel::EndReason::ResourceLimit
                   ? 6
                   : kExitInputError;
    }
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
    } catch (const std::bad_alloc&) {
        // Memory ran out outside the run, while compiling: the same reason
        // and code as in the run (CLI.md 2.7), not a fault of kirimasim.
        std::fputs("kirimasim: error: the memory for the run ran out\n", stderr);
        job.code = kExitResourceLimit;
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
        // The system's refusal, not a fault of kirimasim (RUNTIME_ABI.md 5).
        std::fprintf(stderr,
                     "kirimasim: error: the system gives no thread a stack of %zu MiB for the "
                     "run\n",
                     stack >> 20);
        return kExitInputError;
    }
    pthread_join(thread, nullptr);
    return job.code;
}
