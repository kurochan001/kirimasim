#include "haremasim/kernel/sim.h"

#include <cstdio>

namespace haremasim::kernel {

const char* endReasonName(EndReason r) {
    switch (r) {
        case EndReason::Finish: return "finish";
        case EndReason::Quiescent: return "quiescent";
        case EndReason::Stop: return "stop";
        case EndReason::Fatal: return "fatal";
        case EndReason::TimeLimit: return "time_limit";
        case EndReason::ZeroTimeLimit: return "zero_time_limit";
        case EndReason::Interrupted: return "interrupted";
        case EndReason::Error: return "error";
        case EndReason::Internal: return "internal";
    }
    return "internal";
}

int exitCode(EndReason r) {
    switch (r) {
        case EndReason::Finish:
        case EndReason::Quiescent:
        case EndReason::Stop: return 0;
        case EndReason::Fatal: return 1;
        case EndReason::Error: return 2;
        case EndReason::Internal: return 3;
        case EndReason::TimeLimit: return 4;
        case EndReason::ZeroTimeLimit: return 5;
        case EndReason::Interrupted: return 130;
    }
    return 3;
}

void writeOut(std::string_view text) { std::fwrite(text.data(), 1, text.size(), stdout); }

void flushOut() { std::fflush(stdout); }

}  // namespace haremasim::kernel
