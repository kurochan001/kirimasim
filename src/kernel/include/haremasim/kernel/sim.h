// The scheduler's state and the reasons a simulation ends.
//
// This first version holds only the Active region, enough for the thin path
// of P1 (stage M3a). Time, the other regions and subscriptions come with the
// kernel stage (SCHEDULER_SPEC.md).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <deque>
#include <string_view>

namespace haremasim::kernel {

/// Why a simulation ended (SCHEDULER_SPEC.md 16.2, CLI.md 5).
enum class EndReason : std::uint8_t {
    Finish,
    Quiescent,
    Stop,
    Fatal,
    TimeLimit,
    ZeroTimeLimit,
    Interrupted,
    Error,
    Internal,
};

/// The name used on stderr and in --stats=json: finish, quiescent, ...
const char* endReasonName(EndReason r);

/// The default exit code for a reason.
int exitCode(EndReason r);

/// Processes waiting to run and the request to end the simulation.
class Scheduler {
public:
    void scheduleActive(std::uint32_t process) { active_.push_back(process); }

    /// Takes the next process from the Active region. False if it is empty
    /// or the simulation is ending.
    bool popActive(std::uint32_t& process) {
        if (ending_ || active_.empty()) return false;
        process = active_.front();
        active_.pop_front();
        return true;
    }

    /// Asks the simulation to end. `level` is the argument of $finish or
    /// $stop (0, 1 or 2); it decides what the termination line prints.
    void requestEnd(EndReason r, int level) {
        if (ending_) return;
        ending_ = true;
        reason_ = r;
        level_ = level;
    }

    [[nodiscard]] bool ending() const { return ending_; }
    [[nodiscard]] EndReason reason() const { return ending_ ? reason_ : EndReason::Quiescent; }
    [[nodiscard]] int level() const { return level_; }
    [[nodiscard]] std::uint64_t time() const { return time_; }

private:
    std::deque<std::uint32_t> active_;
    bool ending_ = false;
    EndReason reason_ = EndReason::Quiescent;
    int level_ = 1;
    std::uint64_t time_ = 0;
};

/// Writes the design's output to stdout.
void writeOut(std::string_view text);

/// Flushes stdout, so that it comes before what follows on stderr.
void flushOut();

}  // namespace haremasim::kernel
