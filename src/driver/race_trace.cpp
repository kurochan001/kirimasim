// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "race_trace.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "process_environment.h"

namespace kirimasim::driver {
namespace {
using Header = std::array<std::uint64_t, 9>;
bool append(std::FILE* file, const void* data, std::size_t n) {
    return n == 0 || std::fwrite(data, 1, n, file) == n;
}
std::string describe(const Header& event, const sir::Design& d) {
    using kernel::RaceKind;
    std::string name;
    switch (static_cast<RaceKind>(event[1])) {
        case RaceKind::Wake: name = "wake"; break;
        case RaceKind::Slot:
            name = "change ";
            name += event[3] < d.slots.size() ? d.slots[event[3]].name : "?";
            name += "[" + std::to_string(event[4]) + "]";
            break;
        case RaceKind::Callback: name = "VPI callback " + std::to_string(event[3]); break;
        case RaceKind::Output: name = "stdout"; break;
        case RaceKind::ErrorOutput: name = "stderr"; break;
        case RaceKind::End: name = "end " + std::to_string(event[3]); break;
    }
    if (event[2] < d.processes.size()) {
        const auto& proc = d.processes[event[2]];
        name += " " + proc.name;
        const auto loc = event[6] ? sir::SourceLoc{static_cast<std::uint32_t>(event[5]),
                                                   static_cast<std::uint32_t>(event[6]),
                                                   static_cast<std::uint32_t>(event[7])}
                                  : proc.loc;
        if (loc.line && loc.file < d.sources.size())
            name += " (" + d.sources[loc.file] + ":" + std::to_string(loc.line) + ":" +
                    std::to_string(loc.column) + ")";
    }
    return name;
}
enum class Comparison { Equal, Different, Failed };
Comparison compare(std::FILE* baseline, std::FILE* candidate, const sir::Design& d,
                   std::uint64_t seed) {
    if (fseeko(baseline, 0, SEEK_SET) || fseeko(candidate, 0, SEEK_SET)) return Comparison::Failed;
    for (;;) {
        Header a{}, b{};
        const auto na = std::fread(a.data(), 1, sizeof(a), baseline);
        const auto nb = std::fread(b.data(), 1, sizeof(b), candidate);
        if (na == 0 && nb == 0) return Comparison::Failed;  // A complete trace ends with End.
        if (na != sizeof(a) || nb != sizeof(b)) return Comparison::Failed;
        bool same = a == b;
        std::string detail;
        std::array<std::byte, 4096> x{}, y{};
        for (auto remaining = a.back(); same && remaining;) {
            const auto count =
                static_cast<std::size_t>(std::min<std::uint64_t>(remaining, x.size()));
            if (std::fread(x.data(), 1, count, baseline) != count ||
                std::fread(y.data(), 1, count, candidate) != count)
                return Comparison::Failed;
            const auto first = std::mismatch(x.begin(), x.begin() + count, y.begin());
            same = first.first == x.begin() + count;
            if (!same) {
                const auto offset =
                    a.back() - remaining + static_cast<std::size_t>(first.first - x.begin());
                detail = ", byte " + std::to_string(offset) + " differs (" +
                         std::to_string(std::to_integer<unsigned>(*first.first)) + " vs " +
                         std::to_string(std::to_integer<unsigned>(*first.second)) + ")";
            }
            remaining -= count;
        }
        if (!same) {
            std::fprintf(
                stderr,
                "kirimasim: warning: race seed=%llu time=%llu ticks: baseline %s, replay %s%s\n",
                static_cast<unsigned long long>(seed),
                static_cast<unsigned long long>(std::min(a[0], b[0])), describe(a, d).c_str(),
                describe(b, d).c_str(), detail.c_str());
            return Comparison::Different;
        }
        if (a[1] == static_cast<unsigned>(kernel::RaceKind::End)) return Comparison::Equal;
    }
}
}  // namespace

void RaceTrace::write(void* raw, const kernel::RaceEvent& e) noexcept {
    auto& self = *static_cast<RaceTrace*>(raw);
    const Header head{e.time,        static_cast<unsigned>(e.kind),
                      e.body,        e.subject,
                      e.element,     e.site.file,
                      e.site.line,   e.site.column,
                      e.bytes.size()};
    if (!self.failed_)
        self.failed_ = !append(self.file_, head.data(), sizeof(head)) ||
                       !append(self.file_, e.bytes.data(), e.bytes.size());
}
bool RaceTrace::flush() const { return !failed_ && std::fflush(file_) == 0; }

bool snapshotInput(std::FILE* input, std::FILE* tape, RaceInput& state) {
    const auto offset = ftello(input);
    if (offset < 0) return errno == ESPIPE || errno == EBADF;
    const int fd = fileno(input);
    struct stat info{};
    if (fstat(fd, &info) != 0) return false;
    state.offset = offset;
    if (!S_ISREG(info.st_mode)) {
        // /dev/null is a seekable empty input on Unix. Never drain an
        // interactive device to take a snapshot.
        struct stat nullInfo{};
        if (stat("/dev/null", &nullInfo) != 0 || !S_ISCHR(info.st_mode) ||
            info.st_rdev != nullInfo.st_rdev)
            return false;
        return true;
    }
    std::array<char, 32768> buffer{};
    for (off_t pos = 0; pos < info.st_size;) {
        const auto n =
            pread(fd, buffer.data(),
                  static_cast<size_t>(std::min<off_t>(info.st_size - pos, buffer.size())), pos);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0 || !append(tape, buffer.data(), static_cast<size_t>(n))) return false;
        pos += n;
    }
    return true;
}

bool exploreRaces(const std::string& executable, const std::vector<std::string>& arguments,
                  const sir::Design& d, std::FILE* baseline, std::FILE* input, std::uint64_t runs,
                  std::uint64_t first, const std::string& directory, RaceInput state) {
    for (std::uint64_t i = 0; i < runs; ++i) {
        const auto seed = first + i;
        RaceFile journal(std::tmpfile(), std::fclose), output(std::tmpfile(), std::fclose);
        if (!journal || !output) return false;
        std::vector<std::string> args{executable};
        for (const auto& arg : arguments)
            if (!arg.starts_with("--race-check=") && !arg.starts_with("--race-seed=") &&
                !arg.starts_with("--report-islands="))
                args.push_back(arg);
        // Pick a fresh descriptor beyond every source, so a dup2 cannot clobber
        // the stream used in a later action.
        const int traceFd =
            std::max({fileno(journal.get()), fileno(output.get()), fileno(input), 2}) + 1;
        if (state.offset < 0)
            args.emplace_back(state.ended ? "--race-pipe=ended" : "--race-pipe=prefix");
        args.push_back("--race-directory=" + directory);
        args.push_back("--race-record-fd=" + std::to_string(traceFd));
        args.push_back("--race-seed=" + std::to_string(seed));
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args)
            argv.push_back(arg.data());
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        if (posix_spawn_file_actions_init(&actions)) return false;
        struct Cleanup {
            posix_spawn_file_actions_t& actions;
            ~Cleanup() { posix_spawn_file_actions_destroy(&actions); }
        } cleanup{actions};
        if (fseeko(input, std::max<std::int64_t>(0, state.offset), SEEK_SET)) return false;
        if (posix_spawn_file_actions_adddup2(&actions, fileno(journal.get()), traceFd) ||
            posix_spawn_file_actions_adddup2(&actions, fileno(output.get()), 1) ||
            posix_spawn_file_actions_adddup2(&actions, fileno(output.get()), 2) ||
            posix_spawn_file_actions_adddup2(&actions, fileno(input), 0))
            return false;
        pid_t child;
        if (posix_spawnp(&child, executable.c_str(), &actions, nullptr, argv.data(),
                         childEnvironment()))
            return false;
        int status;
        pid_t waited;
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) == 3) return false;
        const auto comparison = compare(baseline, journal.get(), d, seed);
        if (comparison == Comparison::Failed) return false;
        if (comparison == Comparison::Different) return true;
    }
    std::fprintf(stderr, "kirimasim: info: no difference in %llu race replay(s)\n",
                 static_cast<unsigned long long>(runs));
    return true;
}
}  // namespace kirimasim::driver
