// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "kernel_abi_model.h"
#include "kirimasim/kernel/context.h"

namespace {
bool matches(std::FILE* file, std::string_view expected) {
    if (std::fseek(file, 0, SEEK_SET) != 0) return false;
    std::array<char, 32> data{};
    const auto count = std::fread(data.data(), 1, data.size(), file);
    return std::string_view(data.data(), count) == expected;
}
}  // namespace
bool checkNativeIo(const kirima_kernel_api& api) {
    namespace k = kirimasim::kernel;
    using File = std::unique_ptr<std::FILE, int (*)(std::FILE*)>;
    File out(std::tmpfile(), &std::fclose), err(std::tmpfile(), &std::fclose);
    if (!out || !err) return false;
    const auto root = std::filesystem::temp_directory_path();
    const auto name = "kirimasim-abi-io-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto dir = root / name;
    if (!std::filesystem::create_directory(dir)) return false;
    const auto path = dir / "data";
    bool ok = false;
    {
        k::Context ctx({out.get(), err.get()}), other({out.get(), err.get()});
        std::uint64_t handle = 0;
        ok = check_c_io_model(&api, k::toC(ctx), k::toC(other), path.string().c_str(), &handle) &&
             ctx.strings().get(handle) == "abc" && matches(out.get(), "x\n") &&
             matches(err.get(), "err!\n");
    }
    File result(std::fopen(path.string().c_str(), "rb"), &std::fclose);
    ok = ok && result && matches(result.get(), "x");
    result.reset();
    std::filesystem::remove_all(dir);
    return ok;
}
