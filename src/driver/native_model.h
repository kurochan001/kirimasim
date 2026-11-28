// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/codegen/model.h"
#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/vm/vm.h"
#include <filesystem>
#include <memory>

namespace kirimasim::driver {
struct NativeModule {
    using Library = std::unique_ptr<void, int (*)(void*)>;
    Library library;
    const kirima_module_table* descriptor;
    const kirima_fusion_body* fusion = nullptr;
    const kirima_site_module_v1* sites = nullptr;
};
struct NativeBuild {
    std::optional<NativeModule> module;
    std::string error;
    bool compilerMissing = false;
};
std::filesystem::path defaultNativeCache();
enum class Tier { Vm, Auto, Native };
struct NativeOptions {
    Tier tier = Tier::Vm;
    bool check = false;
    bool approximateTwoState = false;
    bool reportEmissions = false;
    std::string compiler = "c++";
    std::filesystem::path cache = defaultNativeCache();
};

/// Own the loaded libraries until every registered process has finished.
class IslandModules {
public:
    std::string prepare(const sir::VerifiedDesign&, const analysis::TableData&,
                        const NativeOptions&, std::uint64_t maxSteps, bool bounds, bool vpi);
    void bind(vm::RunOptions&) const;
    const auto& unsupported() const { return generated_.unsupportedIslands; }
    const auto& emissions() const { return generated_.emissions; }
    bool loaded() const { return four_.has_value(); }

private:
    codegen::Model generated_;
    codegen::KnownModel known_;
    std::optional<NativeModule> four_, two_;
};
}  // namespace kirimasim::driver
