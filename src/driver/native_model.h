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
};
struct NativeBuild {
    std::optional<NativeModule> module;
    std::string error;
    bool compilerMissing = false;
};
NativeBuild loadNativeModel(const sir::VerifiedDesign&, const codegen::Model&,
                            const analysis::Digest&, const std::string& compiler,
                            const std::filesystem::path& cache);
std::filesystem::path defaultNativeCache();
enum class Tier { Vm, Auto, Native };
struct NativeOptions {
    Tier tier = Tier::Vm;
    bool check = false;
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

private:
    codegen::Model generated_;
    codegen::KnownModel known_;
    std::optional<NativeModule> four_, two_;
};
}  // namespace kirimasim::driver
