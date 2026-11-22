// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "race_files.h"
#include "kirimasim/analysis/tables.h"
#include <cstdlib>
#include <fstream>
#include <unistd.h>

namespace kirimasim::driver {
namespace fs = std::filesystem;
RaceFiles::RaceFiles(const fs::path& baseline) : replay_(!baseline.empty()), baseline_(baseline) {
    std::error_code ec;
    auto root = replay_ ? baseline : fs::temp_directory_path(ec);
    if (ec || root.empty()) {
        error_ = "cannot find temporary directory";
        return;
    }
    auto pattern = (root / "kirimasim-race-XXXXXX").string();
    if (!mkdtemp(pattern.data())) {
        error_ = "cannot create race directory";
        return;
    }
    owned_ = pattern;
    if (!replay_) baseline_ = owned_;
}
RaceFiles::~RaceFiles() {
    std::error_code ec;
    if (!owned_.empty()) fs::remove_all(owned_, ec);
}
std::string RaceFiles::resolve(void* self, const std::string& name, bool write) {
    return static_cast<RaceFiles*>(self)->resolve(name, write);
}
std::string RaceFiles::resolve(const std::string& name, bool write) {
    if (name.empty() || !error_.empty()) return replay_ ? "" : name;
    std::error_code ec;
    const auto path = fs::weakly_canonical(fs::absolute(name, ec), ec);
    if (ec) {
        error_ = "cannot identify simulation file " + name;
        return replay_ ? "" : name;
    }
    constexpr std::string_view hex = "0123456789abcdef";
    std::string key;
    for (auto byte : analysis::contentDigest(path.string())) {
        key += hex[byte >> 4];
        key += hex[byte & 15];
    }
    const auto denied = baseline_ / (key + ".readonly");
    const auto before = baseline_ / (key + ".original");
    const auto missing = baseline_ / (key + ".missing");
    const auto local = owned_ / (key + ".copy");
    if (replay_ && fs::exists(local, ec)) return local.string();
    const auto existing = fs::exists(before, ec);
    const auto absent = fs::exists(missing, ec);
    if (!existing && !absent) {
        const auto status = fs::status(path, ec);
        if (status.type() == fs::file_type::not_found) ec.clear();
        if (!replay_ && !ec) {
            const auto target = fs::exists(status) ? path : path.parent_path();
            if (access(target.c_str(), W_OK) != 0 && !std::ofstream(denied))
                error_ = "cannot record access to " + name;
        }
        if (!ec && !fs::exists(status)) {
            if (!replay_ && !std::ofstream(missing)) error_ = "cannot record absent file " + name;
        } else if (!ec && fs::is_regular_file(status)) {
            if (!replay_) fs::copy_file(path, before, ec);
        } else if (!ec && fs::is_directory(status)) {
            return name;  // Opening a directory for output fails in every run.
        } else if (!ec && path == "/dev/null")
            return name;
        else if (!ec)
            error_ = "cannot snapshot non-regular file " + name;
        if (ec) error_ = "cannot snapshot " + name + ": " + ec.message();
    }
    if (!replay_) return name;
    if (!error_.empty()) return {};
    const auto original = existing ? before
                          : absent ? missing.parent_path() / (key + ".absent")
                                   : path;
    if (!write) return original.string();
    if (fs::exists(denied, ec) || !fs::is_directory(path.parent_path(), ec) ||
        (!existing && !absent && fs::exists(path, ec) && access(path.c_str(), W_OK) != 0))
        return {};
    if (fs::is_regular_file(original, ec)) {
        fs::copy_file(original, local, ec);
        if (ec) {
            error_ = "cannot prepare writable replay file " + name;
            return {};
        }
    }
    return local.string();
}
}  // namespace kirimasim::driver
