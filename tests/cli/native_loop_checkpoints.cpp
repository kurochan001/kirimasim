// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Run the actual generated clocked body with a deterministic interrupt at
// every branch. Inspect the host payload and outstanding NBA prefix there.
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/process_abi.h"
#include "kirimasim/kernel/w4.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <limits>
#include <source_location>
#include <vector>
namespace {
using kirimasim::kernel::W4;
void check(bool condition, std::source_location site = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "native checkpoint comparison failed at line %u\n", site.line());
        std::exit(EXIT_FAILURE);
    }
}
using Entry = std::array<uint64_t, 5>;
struct Host {
    std::array<W4, 2> frame{{{777, 0}, {888, 0}}};
    uint64_t input[2] = {1, 0};
    size_t cut = std::numeric_limits<size_t>::max();
    std::vector<Entry> events;
    std::vector<Entry> checkpoints;
};
const void* query(uint32_t id, uint32_t version) {
    static constexpr auto service = [] {
        kirima_process_api api{};
        api.checkpoint = [](kirima_context* ctx, uint32_t kind, uint32_t* out) -> uint32_t {
            auto& h = *reinterpret_cast<Host*>(ctx);
            check(kind == KIRIMA_TAKE_INTERRUPT);
            h.checkpoints.push_back(
                {h.frame[0].a, h.frame[0].b, h.frame[1].a, h.frame[1].b, h.events.size()});
            *out = h.checkpoints.size() == h.cut;
            return KIRIMA_PROCESS_OK;
        };
        return api;
    }();
    return id == KIRIMA_SERVICE_PROCESS && version == KIRIMA_PROCESS_VERSION ? &service : nullptr;
}
kirima_status run(Host& host, const kirima_model_body& b) {
    kirima_kernel_api api{};
    api.query_service = query;
    api.slot_words = [](kirima_context* c, uint32_t, uint32_t) {
        return reinterpret_cast<Host*>(c)->input;
    };
    api.ending = [](kirima_context*) { return 0; };
    api.schedule_nba = [](kirima_context* c, uint32_t who, uint32_t slot, uint32_t e, uint32_t,
                          uint32_t width, const uint64_t* value, uint64_t delay) {
        check(who == 42 && !delay && width <= 64);
        reinterpret_cast<Host*>(c)->events.push_back({slot, e, width, value[0], value[1]});
    };
    kirima_model_activation act{&api, 42, 0, host.frame.data(), nullptr};
    return b.resume(reinterpret_cast<kirima_context*>(&host), &act);
}
}  // namespace

int main(int argc, char** argv) {
    check(argc == 2);
    void* handle = dlopen(argv[1], RTLD_NOW);
    check(handle != nullptr);
    const auto get =
        reinterpret_cast<kirima_module_query_fn>(dlsym(handle, "kirima_get_module_table"));
    check(get != nullptr);
    const auto* model = get(KIRIMA_ABI_MAJOR, KIRIMA_ABI_MINOR);
    check(model && model->service_count == 1 &&
          query(model->services[0].id, model->services[0].version));
    const auto* b = std::find_if(model->bodies, model->bodies + model->body_count,
                                 [](auto& b) { return b.layout.field_count == 2; });
    check(b != model->bodies + model->body_count && b->layout.bytes == 2 * sizeof(W4));
    for (int reset : {0, 1}) {
        Host ref;
        ref.input[0] = reset;
        check(run(ref, *b) == KIRIMA_DONE);
        check(ref.events.size() == 65 && ref.checkpoints.size() > 180);
        check(ref.frame[reset ? 0 : 1].a == 64);
        check(ref.frame[reset ? 1 : 0].a == uint64_t(reset ? 888 : 777));
        for (size_t i = 1; i <= ref.checkpoints.size(); ++i) {
            Host h;
            h.input[0] = reset;
            h.cut = i;
            check(run(h, *b) == KIRIMA_STOP);
            auto point = ref.checkpoints[i - 1];
            check(h.checkpoints.size() == i && h.events.size() == point[4]);
            check(std::equal(h.events.begin(), h.events.end(), ref.events.begin()));
            check(std::equal(h.checkpoints.begin(), h.checkpoints.end(), ref.checkpoints.begin()));
            check(h.frame[0].a == point[0] && h.frame[0].b == point[1] &&
                  h.frame[1].a == point[2] && h.frame[1].b == point[3]);
        }
    }
    dlclose(handle);
}
