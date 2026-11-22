// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/analysis/islands.h"
#include "kirimasim/codegen/model.h"
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/sir/text_v1.h"
#include "kirimasim/vm/vm.h"
#include <algorithm>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
bool exerciseHost(const kirimasim::sir::VerifiedDesign& design,
                  const kirimasim::vm::RunOptions& options) {
    namespace vm = kirimasim::vm;
    using kirimasim::kernel::EndReason;
    vm::RunResult result;
    auto left = vm::Model::create(design, result, options);
    auto right = vm::Model::create(design, result, options);
    if (!left || !right) return false;
    const auto clk = left->bind("top.clk"), d = left->bind("top.data"), q = left->bind("top.q");
    if (!clk || !d || !q || left->bind("top.missing")) return false;
    const std::array<std::uint64_t, 1> low{0}, high{1};
    const std::array<std::uint64_t, 4> mixed{0x12345, 0x8080, 0xabcd, 0x4000};
    const std::array<std::uint64_t, 4> other{9, 0, 1, 0};
    std::array<std::uint64_t, 4> value{};
    for (auto* model : {left.get(), right.get()}) {
        if (!model->drive(*clk, low) || !model->advanceTo(0) || model->drive(*d, low) ||
            model->drive(*d, mixed, 1) || model->read(*q, value, 1))
            return false;
    }
    if (!left->drive(*d, mixed) || !right->drive(*d, other) || !left->advanceTo(2) ||
        !left->drive(*clk, high) || !left->advanceTo(2) || !left->read(*q, value) || mixed != value)
        return false;
    if (right->time() != 0 || !right->drive(*clk, high) || !right->advanceTo(0) ||
        !right->read(*q, value) || other != value)
        return false;
    if (left->advanceTo(1) || left->time() != 2 || left->advanceTo(30) || left->time() != 17 ||
        left->result().reason != EndReason::Finish || left->finish().reason != EndReason::Finish ||
        left->drive(*d, other))
        return false;
    return right->advanceTo(3) && right->finish().reason == EndReason::Quiescent &&
           right->finish().reason == EndReason::Quiescent && !right->advanceTo(17);
}
}  // namespace

int main(int argc, char** argv) try {
    namespace a = kirimasim::analysis;
    namespace k = kirimasim::kernel;
    namespace vm = kirimasim::vm;
    if (argc < 3) return 2;
    std::optional<kirimasim::sir::VerifiedDesign> input;
    if (std::string_view{argv[1]}.ends_with(".sir")) {
        std::ifstream file(argv[1]);
        std::string text{std::istreambuf_iterator<char>{file}, {}};
        input = kirimasim::sir::parse1(text).verified;
    } else {
        const std::array<std::string, 1> sources{argv[1]};
        auto compiled = kirimasim::front::compile(sources);
        if (!compiled.verified) std::cerr << compiled.diagnostics;
        input = std::move(compiled.verified);
    }
    if (!input) return 2;
    const auto& verified = *input;
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
    const auto known = kirimasim::codegen::emitKnownModel(verified, *table, 1000000000, false);
    if (std::string_view{argv[2]} == "emit-known") {
        std::cout << known.model.source;
        return std::cout ? 0 : 2;
    }
    using Library = std::unique_ptr<void, int (*)(void*)>;
    const auto load = [&](const char* path, Library& library,
                          const auto& bodyIds) -> const kirima_module_table* {
        library.reset(dlopen(path, RTLD_NOW | RTLD_LOCAL));
        if (!library) {
            std::cerr << dlerror() << '\n';
            return nullptr;
        }
        auto query = reinterpret_cast<kirima_module_query_fn>(
            dlsym(library.get(), "kirima_get_module_table"));
        const auto* api = kirima_get_kernel_api();
        if (!query) return nullptr;
        const auto* module = query(api->abi_major, api->abi_minor);
        kirima_model_identity identity{1, KIRIMA_MODEL_LAYOUT_64_V1, {}};
        std::ranges::copy(table->key.design, identity.design_sha256);
        std::vector<kirima_model_layout> layouts;
        layouts.reserve(bodyIds.size());
        for (auto id : bodyIds)
            layouts.push_back({KIRIMA_MODEL_PROCESS, id.index(), 0, 8, 0, 0, nullptr});
        return kirima_validate_module(api, module, &identity, layouts.data(),
                                      static_cast<uint32_t>(layouts.size())) == KIRIMA_MODULE_OK
                   ? module
                   : nullptr;
    };
    Library four(nullptr, dlclose), two(nullptr, dlclose);
    const kirima_module_table *model = nullptr, *knownModel = nullptr;
    if (std::string_view{argv[2]} != "vm") {
        model = load(argv[2], four, emitted.bodies);
        if (!model) return 2;
    }
    if (argc > 4) {
        knownModel = load(argv[4], two, known.model.bodies);
        if (!knownModel) return 2;
    }
    k::Context ctx;
    vm::RunOptions options;
    options.maxSteps = 1000000000;
    options.tierCheck = argc > 3 && std::string_view{argv[3]} == "check";
    options.plusargs.push_back("n=32");
    if (knownModel)
        for (const auto& guard : known.guards)
            options.knownIslands.push_back({guard.island, guard.inputs});
    std::size_t m = 0, km = 0;
    for (const auto& row : table->members) {
        vm::IslandMember member{row.process, row.island, row.level,
                                a::islandSensitivity(design, design.process(row.process))};
        if (model && m < emitted.bodies.size() && row.process == emitted.bodies[m])
            member.native = model->bodies[m++].resume;
        if (knownModel && km < known.model.bodies.size() && row.process == known.model.bodies[km])
            member.nativeKnown = knownModel->bodies[km++].resume;
        options.islands.push_back(std::move(member));
    }
    if (model && m < emitted.bodies.size()) {
        options.combinationalBodies.resize(design.processes.size());
        for (; m < emitted.bodies.size(); ++m)
            options.combinationalBodies[emitted.bodies[m].index()] = model->bodies[m].resume;
    }
    if (argc > 3 && std::string_view{argv[3]} == "host")
        return exerciseHost(verified, options) ? 0 : 1;
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
