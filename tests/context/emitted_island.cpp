// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/analysis/islands.h"
#include "kirimasim/codegen/model.h"
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/vm/vm.h"
#include <algorithm>
#include <dlfcn.h>
#include <iostream>
#include <memory>

int main(int argc, char** argv) try {
    namespace a = kirimasim::analysis;
    namespace k = kirimasim::kernel;
    namespace vm = kirimasim::vm;
    if (argc < 3) return 2;
    const std::array<std::string, 1> sources{argv[1]};
    auto compiled = kirimasim::front::compile(sources);
    if (!compiled.verified) return 2;
    const auto& verified = *compiled.verified;
    const auto& design = verified.design();
    auto table = a::analyzeIslands(verified);
    if (!table) return 2;
    const auto emitted = kirimasim::codegen::emitModel(verified, *table, 1000000000, false);
    if (emitted.bodies.empty()) return 2;
    if (std::string_view{argv[2]} == "stale") {
        table->key.design.front() ^= 1;
        return kirimasim::codegen::emitModel(verified, *table, 1000000000, false).bodies.empty()
                   ? 0
                   : 1;
    }
    if (std::string_view{argv[2]} == "emit") {
        std::cout << emitted.source;
        return std::cout ? 0 : 2;
    }
    std::unique_ptr<void, int (*)(void*)> library(nullptr, dlclose);
    const kirima_module_table* model = nullptr;
    if (std::string_view{argv[2]} != "vm") {
        library.reset(dlopen(argv[2], RTLD_NOW | RTLD_LOCAL));
        if (!library) {
            std::cerr << dlerror() << '\n';
            return 2;
        }
        auto query = reinterpret_cast<kirima_module_query_fn>(
            dlsym(library.get(), "kirima_get_module_table"));
        const auto* api = kirima_get_kernel_api();
        if (!query) return 2;
        model = query(api->abi_major, api->abi_minor);
        kirima_model_identity identity{1, KIRIMA_MODEL_LAYOUT_64_V1, {}};
        std::ranges::copy(table->key.design, identity.design_sha256);
        std::vector<kirima_model_layout> layouts;
        layouts.reserve(emitted.bodies.size());
        for (auto id : emitted.bodies)
            layouts.push_back({KIRIMA_MODEL_PROCESS, id.index(), 0, 8, 0, 0, nullptr});
        if (kirima_validate_module(api, model, &identity, layouts.data(),
                                   static_cast<uint32_t>(layouts.size())) != KIRIMA_MODULE_OK)
            return 2;
    }
    k::Context ctx;
    vm::RunOptions options;
    options.maxSteps = 1000000000;
    options.tierCheck = argc > 3 && std::string_view{argv[3]} == "check";
    options.plusargs.push_back("n=32");
    std::size_t m = 0;
    for (const auto& row : table->members) {
        vm::IslandMember member{row.process, row.island, row.level,
                                a::islandSensitivity(design, design.process(row.process))};
        if (model && m < emitted.bodies.size() && row.process == emitted.bodies[m])
            member.native = model->bodies[m++].resume;
        options.islands.push_back(std::move(member));
    }
    const auto result = vm::run(verified, ctx, options);
    ctx.output().flush();
    std::cout << ctx.counters().toJson();
    if (result.reason == k::EndReason::Internal) {
        std::cerr << result.error << '\n';
        return 3;
    }
    return result.reason == k::EndReason::Quiescent || result.reason == k::EndReason::Finish ? 0
                                                                                             : 1;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 3;
}
