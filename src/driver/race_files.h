// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <filesystem>
#include <string>

namespace kirimasim::driver {
/// Capture source file contents before the baseline opens them. Each replay
/// gets a private writable copy, while the baseline writes the requested path.
class RaceFiles {
public:
    explicit RaceFiles(const std::filesystem::path& baseline);
    ~RaceFiles();
    RaceFiles(const RaceFiles&) = delete;
    RaceFiles& operator=(const RaceFiles&) = delete;
    [[nodiscard]] const std::filesystem::path& baseline() const { return baseline_; }
    [[nodiscard]] const std::string& error() const { return error_; }
    static std::string resolve(void*, const std::string&, bool);

private:
    bool replay_;
    std::filesystem::path owned_, baseline_;
    std::string error_;
    std::string resolve(const std::string&, bool);
};
}  // namespace kirimasim::driver
