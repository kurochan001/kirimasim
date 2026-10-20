// The tests of state and Contexts (RUNTIME_ABI.md 9.8; TEST_SUITE.md, list
// of required tests). Each compiles and runs the designs of designs/ in one
// process and compares every run with the run of that design alone, which a
// child process of this program makes:
//
//   kirimasim_context_tests sequential   every design three times, one
//                                        after another (context_sequential)
//   kirimasim_context_tests parallel     2, 4 and 8 Contexts at the same
//                                        time on separate threads, the
//                                        designs compiled before
//                                        (context_parallel)
//   kirimasim_context_tests front        the same, each thread compiling
//                                        its design too (front_parallel;
//                                        9.7)
//   kirimasim_context_tests churn        a Context made, run and destroyed
//                                        100 times (context_churn)
//   kirimasim_context_tests alone <n>    design n alone, for the comparison
//
// A run is compared by all it leaves: front's diagnostics and the SIR it
// made, the output and the messages on the destinations its Context was
// made with (9.3.10), the reason and level of the end, from which the
// driver makes the exit code (9.3.9), the time it ended at, and the
// counters (PerfCount).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <latch>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pthread.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/vm/vm.h"

namespace {

namespace kernel = kirimasim::kernel;

struct Case {
    std::string file;
    std::vector<std::string> plusargs;
    std::uint64_t maxTime = UINT64_MAX;  ///< In the design's time precision.
};

std::vector<Case> cases() {
    return {
        {.file = "counter.sv"},
        {.file = "messages.sv"},
        {.file = "wide_strings.sv"},
        {.file = "runtime_error.sv"},
        {.file = "plusargs.sv", .plusargs = {"mode=fast", "count=12"}},
        {.file = "time_limit.sv", .maxTime = 50},
    };
}

struct Result {
    std::string diagnostics;
    std::string sir;
    std::string out;
    std::string err;
    std::string reason;
    std::string error;
    std::string counters;
    std::uint64_t level = 0;
    std::uint64_t time = 0;
};

struct Compiled {
    std::string diagnostics;
    std::string sir;
    bool success = false;
    kirimasim::sir::Design design;
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

Compiled compile(const Case& c) {
    const std::vector<std::string> args{std::string(KIRIMASIM_CONTEXT_DESIGNS) + "/" + c.file};
    auto r = kirimasim::front::compile(args);
    Compiled out{.diagnostics = r.diagnostics + r.verifyErrors, .success = r.success};
    if (r.success) {
        out.sir = kirimasim::sir::toText(r.design);
        out.design = std::move(r.design);
    }
    return out;
}

/// Runs a compiled design on a new Context, which is destroyed before the
/// function returns.
Result run(const Case& c, const Compiled& compiled) {
    Result r{.diagnostics = compiled.diagnostics, .sir = compiled.sir};
    if (!compiled.success) return r;
    const File out(std::tmpfile());
    const File err(std::tmpfile());
    if (!out || !err) {
        r.error = "no temporary file";
        return r;
    }
    {
        kernel::Context context(kernel::OutputDestinations{out.get(), err.get()});
        kernel::Limits limits;
        limits.maxTime = c.maxTime;
        context.scheduler().setLimits(limits);
        kirimasim::vm::RunOptions options;
        options.plusargs = c.plusargs;
        const kirimasim::vm::RunResult run = kirimasim::vm::run(compiled.design, context, options);
        context.output().flush();
        r.reason = kernel::endReasonName(run.reason);
        r.error = run.error;
        r.level = context.scheduler().level();
        r.time = context.scheduler().time();
        r.counters = context.counters().toJson();
    }
    r.out = contents(out.get());
    r.err = contents(err.get());
    return r;
}

// A result crosses from the child as its fields in order, each as its size
// on a line and then its bytes.
std::string serialize(const Result& r) {
    std::string s;
    for (const std::string& f : {r.diagnostics, r.sir, r.out, r.err, r.reason, r.error, r.counters,
                                 std::to_string(r.level), std::to_string(r.time)})
        s += std::to_string(f.size()) + "\n" + f;
    return s;
}

std::optional<Result> deserialize(std::string_view s) {
    std::vector<std::string> fields;
    while (!s.empty()) {
        const std::size_t nl = s.find('\n');
        if (nl == std::string_view::npos) return std::nullopt;
        const std::size_t size = std::strtoull(std::string(s.substr(0, nl)).c_str(), nullptr, 10);
        if (s.size() - nl - 1 < size) return std::nullopt;
        fields.emplace_back(s.substr(nl + 1, size));
        s.remove_prefix(nl + 1 + size);
    }
    if (fields.size() != 9) return std::nullopt;
    return Result{.diagnostics = fields[0],
                  .sir = fields[1],
                  .out = fields[2],
                  .err = fields[3],
                  .reason = fields[4],
                  .error = fields[5],
                  .counters = fields[6],
                  .level = std::strtoull(fields[7].c_str(), nullptr, 10),
                  .time = std::strtoull(fields[8].c_str(), nullptr, 10)};
}

/// The result of case n alone: this program, run again as a child process
/// that runs only that case.
std::optional<Result> alone(const char* self, char** envp, std::size_t n) {
    int fds[2];
    if (pipe(fds) != 0) return std::nullopt;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    std::string mode = "alone";
    std::string index = std::to_string(n);
    std::string path = self;
    char* argv[] = {path.data(), mode.data(), index.data(), nullptr};
    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, self, &actions, nullptr, argv, envp);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    std::string text;
    char buffer[4096];
    ssize_t got = 0;
    while (spawned == 0 && (got = read(fds[0], buffer, sizeof buffer)) > 0)
        text.append(buffer, static_cast<std::size_t>(got));
    close(fds[0]);
    if (spawned != 0) return std::nullopt;
    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return std::nullopt;
    return deserialize(text);
}

bool same(const Result& want, const Result& got, const std::string& what) {
    bool ok = true;
    const auto field = [&](const char* name, const std::string& a, const std::string& b) {
        if (a == b) return;
        std::fprintf(stderr, "%s: %s differs from the run alone\n--- alone\n%s\n--- here\n%s\n",
                     what.c_str(), name, a.c_str(), b.c_str());
        ok = false;
    };
    field("the diagnostics", want.diagnostics, got.diagnostics);
    field("the SIR", want.sir, got.sir);
    field("the output", want.out, got.out);
    field("the messages", want.err, got.err);
    field("the end reason", want.reason, got.reason);
    field("the internal error", want.error, got.error);
    field("the counters", want.counters, got.counters);
    field("the end level", std::to_string(want.level), std::to_string(got.level));
    field("the end time", std::to_string(want.time), std::to_string(got.time));
    return ok;
}

/// Runs every case 3 times, one after another: in order, in reverse, and
/// in order again, so that each runs after others and after itself.
int sequential(const std::vector<Case>& cs, const std::vector<Result>& baseline) {
    std::vector<std::size_t> order;
    order.reserve(3 * cs.size());
    for (std::size_t i = 0; i < cs.size(); ++i)
        order.push_back(i);
    for (std::size_t i = cs.size(); i-- > 0;)
        order.push_back(i);
    for (std::size_t i = 0; i < cs.size(); ++i)
        order.push_back(i);
    int failures = 0;
    for (std::size_t k = 0; k < order.size(); ++k) {
        const std::size_t i = order[k];
        if (!same(baseline[i], run(cs[i], compile(cs[i])),
                  cs[i].file + " (run " + std::to_string(k) + ")"))
            ++failures;
    }
    std::printf("context_sequential: %zu runs, %d differ from their runs alone\n", order.size(),
                failures);
    return failures == 0 ? 0 : 1;
}

/// The stack of each thread: what the simulation thread of the driver
/// needs for these designs, with room for the sanitizers.
constexpr std::size_t kStackBytes = std::size_t{64} << 20;

struct Job {
    const Case* c;
    const Compiled* compiled;  ///< Null: the thread compiles the design too.
    std::latch* start;
    Result result;
};

/// Runs 2, 4 and then 8 cases at the same time, each on a thread of its
/// own; the threads start together. With `compiled`, the designs were
/// compiled before, and the threads only run Contexts; without, each
/// thread runs front too.
int parallel(const std::vector<Case>& cs, const std::vector<Compiled>* compiled,
             const std::vector<Result>& baseline, const char* name) {
    int failures = 0;
    std::size_t runs = 0;
    for (const std::size_t threads : {2U, 4U, 8U}) {
        std::latch start(static_cast<std::ptrdiff_t>(threads));
        std::vector<Job> jobs(threads);
        std::vector<pthread_t> ids(threads);
        std::vector<bool> started(threads, false);
        for (std::size_t t = 0; t < threads; ++t) {
            const std::size_t i = t % cs.size();
            jobs[t].c = &cs[i];
            jobs[t].compiled = compiled != nullptr ? &(*compiled)[i] : nullptr;
            jobs[t].start = &start;
            pthread_attr_t attr;
            if (pthread_attr_init(&attr) != 0) continue;
            started[t] = pthread_attr_setstacksize(&attr, kStackBytes) == 0 &&
                         pthread_create(
                             &ids[t], &attr,
                             [](void* p) -> void* {
                                 auto& job = *static_cast<Job*>(p);
                                 job.start->arrive_and_wait();
                                 job.result = job.compiled != nullptr
                                                  ? run(*job.c, *job.compiled)
                                                  : run(*job.c, compile(*job.c));
                                 return nullptr;
                             },
                             &jobs[t]) == 0;
            pthread_attr_destroy(&attr);
        }
        for (std::size_t t = 0; t < threads; ++t) {
            if (!started[t]) {
                std::fprintf(stderr, "%s: thread %zu of %zu did not start\n", name, t, threads);
                return 1;
            }
        }
        for (std::size_t t = 0; t < threads; ++t)
            pthread_join(ids[t], nullptr);
        for (std::size_t t = 0; t < threads; ++t) {
            ++runs;
            if (!same(baseline[t % cs.size()], jobs[t].result,
                      jobs[t].c->file + " (thread " + std::to_string(t) + " of " +
                          std::to_string(threads) + ")"))
                ++failures;
        }
    }
    std::printf("%s: %zu runs, %d differ from their runs alone\n", name, runs, failures);
    return failures == 0 ? 0 : 1;
}

/// Compiles each case once, then makes, runs and destroys a Context 100
/// times, the cases in turn. A leak shows under AddressSanitizer at exit.
int churn(const std::vector<Case>& cs, const std::vector<Compiled>& compiled,
          const std::vector<Result>& baseline) {
    constexpr std::size_t kRuns = 100;
    int failures = 0;
    for (std::size_t k = 0; k < kRuns; ++k) {
        const std::size_t i = k % cs.size();
        if (!same(baseline[i], run(cs[i], compiled[i]),
                  cs[i].file + " (Context " + std::to_string(k) + ")"))
            ++failures;
    }
    std::printf("context_churn: %zu Contexts, %d differ from their runs alone\n", kRuns, failures);
    return failures == 0 ? 0 : 1;
}

int runTests(int argc, char** argv, char** envp) {
    const std::vector<Case> cs = cases();
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "alone" && argc == 3) {
        const std::size_t n = std::strtoull(argv[2], nullptr, 10);
        if (n >= cs.size()) return 2;
        const std::string s = serialize(run(cs[n], compile(cs[n])));
        return std::fwrite(s.data(), 1, s.size(), stdout) == s.size() ? 0 : 1;
    }
    if (mode != "sequential" && mode != "parallel" && mode != "front" && mode != "churn") {
        std::fputs("usage: kirimasim_context_tests sequential|parallel|front|churn|alone <n>\n",
                   stderr);
        return 2;
    }
    std::vector<Result> baseline;
    for (std::size_t i = 0; i < cs.size(); ++i) {
        auto r = alone(argv[0], envp, i);
        if (!r) {
            std::fprintf(stderr, "%s: the run alone failed\n", cs[i].file.c_str());
            return 1;
        }
        baseline.push_back(std::move(*r));
    }
    if (mode == "sequential") return sequential(cs, baseline);
    if (mode == "front") return parallel(cs, nullptr, baseline, "front_parallel");
    std::vector<Compiled> compiled;
    compiled.reserve(cs.size());
    for (const Case& c : cs)
        compiled.push_back(compile(c));
    if (mode == "parallel") return parallel(cs, &compiled, baseline, "context_parallel");
    return churn(cs, compiled, baseline);
}

}  // namespace

int main(int argc, char** argv, char** envp) {
    try {
        return runTests(argc, argv, envp);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "kirimasim_context_tests: %s\n", e.what());
    }
    return 1;
}
