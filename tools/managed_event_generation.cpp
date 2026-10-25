// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Explicit, long-running production-width generation audit; not a CTest.
// c++ -std=c++20 -O3 -DNDEBUG -ffp-contract=off -Isrc/kernel/include \
//   src/kernel/managed_event.cpp tools/managed_event_generation.cpp -o build/event-generation
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "kirimasim/kernel/managed_event.h"

namespace k = kirimasim::kernel;

struct Backend {
    std::uint32_t created = 0;
    std::uint64_t cleared = 0;
    static k::ManagedEventResult<k::EventId> create(void* frame) noexcept {
        return {k::EventId{static_cast<Backend*>(frame)->created++}};
    }
    static void clear(void* frame, k::EventId) noexcept { ++static_cast<Backend*>(frame)->cleared; }
};

int main() {
    Backend backend;
    k::ManagedEvents store;
    if (store.setBackend({&backend, Backend::create, Backend::clear}) != k::ManagedEventError::None)
        return 1;
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    k::ManagedEventToken last;
    for (std::uint64_t generation = 1; generation <= maximum; ++generation) {
        auto created = store.create();
        if (!created || created.value.token().bits != ((generation << 32) | 1) ||
            store.valid(last) || store.resolve(created.value).value != k::EventId{0})
            return 2;
        last = created.value.token();
        created.value.reset();
        if (store.collect() != k::ManagedEventError::None || store.valid(last) ||
            store.allocatedObjects() != 1 || store.liveObjects() != 0)
            return 3;
        if (generation % (std::uint64_t{1} << 28) == 0) {
            std::fprintf(stderr, "completed %" PRIu64 " generations\n", generation);
            std::fflush(stderr);
        }
    }
    if (store.retiredObjects() != 1 || backend.created != 1 || backend.cleared != maximum) return 4;
    auto next = store.create();
    if (!next || next.value.token().bits != ((std::uint64_t{1} << 32) | 2) || store.valid(last) ||
        store.resolve(next.value).value != k::EventId{1})
        return 5;
    std::printf("{\"completed_generations\":%" PRIu64
                ",\"retired_objects\":1,\"allocated_objects_after_next\":2,"
                "\"next_token\":%" PRIu64 ",\"stale_last_token_rejected\":true}\n",
                std::uint64_t{maximum}, next.value.token().bits);
}
