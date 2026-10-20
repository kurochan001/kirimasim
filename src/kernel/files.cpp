#include "kirimasim/kernel/files.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace kirimasim::kernel {

namespace {

/// The types of Table 21-6, which are C's modes as they are (21.3.1).
constexpr std::array<std::string_view, 15> kTypes{
    "r", "rb", "w", "wb", "a", "ab", "r+", "r+b", "rb+", "w+", "w+b", "wb+", "a+", "a+b", "ab+"};
constexpr std::uint32_t kFdBit = 0x8000'0000U;
constexpr std::uint32_t kLastMcdBit = 30;  // bit 31 marks an fd (21.3.1)

}  // namespace

Files::~Files() {
    for (const File& r : rows_)
        if (r.f != nullptr) std::fclose(r.f);
}

std::uint32_t Files::place(std::uint32_t row, std::FILE* f) {
    if (rows_.size() <= row) rows_.resize(row + 1);
    rows_[row] = {f, ++serial_};
    return row;
}

std::uint32_t Files::openMcd(const std::string& name) {
    std::uint32_t row = 1;
    while (row <= kLastMcdBit && open(row))
        ++row;
    if (row > kLastMcdBit) return 0;
    std::FILE* f = std::fopen(name.c_str(), "w");
    if (f == nullptr) return 0;
    return 1U << place(row, f);
}

std::uint32_t Files::openFd(const std::string& name, std::string_view type, bool& badType) {
    badType = std::find(kTypes.begin(), kTypes.end(), type) == kTypes.end();
    if (badType) return 0;
    std::uint32_t row = kFdRow + 3;  // after STDIN, STDOUT and STDERR
    while (open(row))
        ++row;
    std::FILE* f = std::fopen(name.c_str(), std::string(type).c_str());
    if (f == nullptr) return 0;
    return kFdBit | (place(row, f) - kFdRow);
}

FileTargets Files::targets(W4 desc) const {
    FileTargets t;
    const auto a = static_cast<std::uint32_t>(desc.a);
    if (static_cast<std::uint32_t>(desc.b) != 0) {
        t.notOpen = true;  // x or z bits
        return t;
    }
    if ((a & kFdBit) != 0) {
        // STDIN takes no output, as C's stream would not (SYSTEM_TASKS.md 6.1).
        const std::uint32_t n = a & ~kFdBit;
        if (n == 1) {
            t.out = true;
        } else if (n == 2) {
            t.err = true;
        } else if (n >= 3 && open(kFdRow + n)) {
            t.files.emplace_back(kFdRow + n, rows_[kFdRow + n].serial);
        } else if (n != 0) {
            t.notOpen = true;
        }
        return t;
    }
    if (a == 0) t.notOpen = true;
    if ((a & 1U) != 0) t.out = true;
    for (std::uint32_t row = 1; row <= kLastMcdBit; ++row) {
        if (((a >> row) & 1U) == 0) continue;
        if (open(row))
            t.files.emplace_back(row, rows_[row].serial);
        else
            t.notOpen = true;
    }
    return t;
}

void Files::write(const FileTargets& t, std::string_view text) {
    if (text.empty()) return;
    if (t.out) output_.write(text);
    if (t.err) output_.writeError(text);
    for (const auto& [row, serial] : t.files)
        if (open(row) && rows_[row].serial == serial)
            std::fwrite(text.data(), 1, text.size(), rows_[row].f);
}

bool Files::live(const FileTargets& t) const {
    if (t.out || t.err) return true;
    for (const auto& [row, serial] : t.files)
        if (open(row) && rows_[row].serial == serial) return true;
    return false;
}

bool Files::close(W4 desc) {
    const FileTargets t = targets(desc);
    for (const auto& [row, serial] : t.files) {
        std::fclose(rows_[row].f);
        rows_[row] = {};
    }
    return !t.notOpen;
}

bool Files::flush(W4 desc) {
    const FileTargets t = targets(desc);
    if (t.out) output_.flush();
    for (const auto& [row, serial] : t.files)
        std::fflush(rows_[row].f);
    return !t.notOpen;
}

void Files::readLine(W4 desc, std::string& line, bool& notOpen) {
    line.clear();
    const FileTargets t = targets(desc);
    notOpen = t.notOpen;
    const auto a = static_cast<std::uint32_t>(desc.a);
    if ((a & kFdBit) == 0 || t.files.size() != 1) return;
    std::FILE* f = rows_[t.files[0].first].f;
    for (int c = std::fgetc(f); c != EOF; c = std::fgetc(f)) {
        line.push_back(static_cast<char>(c));
        if (c == '\n') break;
    }
}

void Files::flushAll() {
    output_.flush();
    for (const File& r : rows_)
        if (r.f != nullptr) std::fflush(r.f);
}

}  // namespace kirimasim::kernel
