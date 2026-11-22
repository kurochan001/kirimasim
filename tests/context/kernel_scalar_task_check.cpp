// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <cstdint>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/random.h"
#include "kirimasim/kernel/scalar_task_abi.h"
#include "kirimasim/kernel/urandom.h"
extern "C" int check_c_scalar_tasks(const kirima_kernel_api*, kirima_context*);

bool checkNativeScalarTasks(const kirima_kernel_api& kernel) {
    namespace k = kirimasim::kernel;
    k::Context ctx;
    auto* c = k::toC(ctx);
    if (!check_c_scalar_tasks(&kernel, c)) return false;
    const auto* s = static_cast<const kirima_scalar_task_api*>(
        kernel.query_service(KIRIMA_SERVICE_SCALAR_TASK, 1));
    if (!s) return false;
    std::uint64_t state = 0, child = 0;
    k::Rng g{k::initialState(12, "top.i")};
    if (s->initial_rng(12, {"top.i", 5}, &state) || state != g.state ||
        s->child_rng(&state, &child) || child != g.child().state || state != g.state)
        return false;
    std::uint32_t value = 0, seed = 7;
    if (s->draw(&state, &seed, &value) || value != k::urandom(g, seed) || state != g.state ||
        s->draw(&state, nullptr, &value) || value != g.draw() ||
        s->draw_range(&state, 3, 11, &value) || value != k::urandomRange(g, 3, 11) ||
        state != g.state)
        return false;
    for (auto kind = 0U; kind <= KIRIMA_DIST_ERLANG; ++kind) {
        for (std::int32_t a : std::array{-1, 2, 27}) {
            std::int32_t actual = 782, expected = actual;
            const auto r = k::drawDistribution(static_cast<k::Distribution>(kind), expected, a, 3);
            kirima_dist_result got{};
            if (s->distribution(c, kind, &actual, a, 3, &got) || actual != expected ||
                got.value != r.value || got.bad_argument != static_cast<unsigned>(r.bad))
                return false;
        }
    }
    std::int32_t expected = ctx.randomSeed();
    kirima_dist_result got{};
    return !s->distribution(c, KIRIMA_RANDOM, nullptr, 0, 0, &got) &&
           got.value == k::randomInt(expected) && ctx.randomSeed() == expected &&
           s->distribution(c, KIRIMA_DIST_NORMAL, nullptr, 0, 3, &got) == KIRIMA_VALUE_INVALID;
}
