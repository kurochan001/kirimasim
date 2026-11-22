// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <cstdio>
#include <memory>
#include <string_view>

#include "kernel_process_model.h"
#include "kirimasim/kernel/context.h"

bool checkNativeProcesses(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;
    File out(std::tmpfile(), &std::fclose);
    if (!out) return false;
    k::Context first({out.get(), out.get()}), other({out.get(), out.get()});
    process_fixture f{};
    if (!start_process_fixture(&api, k::toC(first), k::toC(other), &f) ||
        first.scheduler().run() != k::EndReason::Quiescent ||
        !finish_process_fixture(k::toC(first), &f))
        return false;
    if (std::fseek(out.get(), 0, SEEK_SET) != 0) return false;
    std::array<char, 32> bytes{};
    const auto size = std::fread(bytes.data(), 1, bytes.size(), out.get());
    if (std::string_view(bytes.data(), size).find("queued text") == std::string_view::npos)
        return false;
    process_fixture g{};
    auto* c = k::toC(other);
    if (!start_program_fixture(&api, c, &g) || other.scheduler().run() != k::EndReason::Finish ||
        g.error)
        return false;
    std::uint32_t present = 0;
    kirima_process ended{};
    bool child = false;
    do {
        if (g.api->take_ended(c, KIRIMA_ENDED_PROGRAM_PROCESS, &present, &ended)) return false;
        if (present && ended.index == g.child.index && ended.serial == g.child.serial) child = true;
    } while (present);
    if (!child || g.api->take_ended(c, KIRIMA_ENDED_FILE_MONITOR, &present, &ended) || present)
        return false;
    k::Context stopped;
    kirima_scheduler_state state{};
    return !g.api->terminate(k::toC(stopped), KIRIMA_OUT_OF_MEMORY) &&
           !g.api->state(k::toC(stopped), &state) && (state.flags & KIRIMA_MEMORY_RAN_OUT) &&
           state.reason == static_cast<std::uint32_t>(k::EndReason::ResourceLimit);
}
