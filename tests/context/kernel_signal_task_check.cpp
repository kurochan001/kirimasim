// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/io_abi.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
extern "C" int check_c_signals_tasks(const kirima_kernel_api*, kirima_context*, kirima_context*,
                                     kirima_bytes, void (*)(kirima_context*));
bool checkNativeSignalsAndTasks(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    std::unique_ptr<std::FILE, decltype(&std::fclose)> output(std::tmpfile(), &std::fclose);
    if (!output) return false;
    k::Context ctx({output.get(), output.get()}, nullptr), other;
    const auto a = ctx.store().add(k::Domain::Four, 8, 1, k::Init::Zero);
    const auto side = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    const auto path =
        std::filesystem::temp_directory_path() /
        ("kirimasim-model-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".vcd");
    const auto filename = path.string();
    const bool ok =
        check_c_signals_tasks(&api, k::toC(ctx), k::toC(other), {filename.data(), filename.size()},
                              [](kirima_context* c) { (void)k::fromC(c).scheduler().run(); });
    std::filesystem::remove(path);
    std::array<char, 4> actual{};
    return ok && std::fseek(output.get(), 0, SEEK_SET) == 0 &&
           std::fread(actual.data(), 1, 4, output.get()) == 4 &&
           actual == std::array<char, 4>{'a', 0, 'b', '\n'} && ctx.store().element(a, 0)[0] == 9 &&
           ctx.store().element(side, 0)[0] == 1 && ctx.scheduler().time() == 3;
}
