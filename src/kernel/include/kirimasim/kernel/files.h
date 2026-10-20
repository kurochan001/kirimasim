// The files a design opens (SYSTEM_TASKS.md 6): multichannel descriptors
// (mcd) and file descriptors (fd). Standard output and standard error are
// the Context's Output, so that every path to them keeps one order
// (SYSTEM_TASKS.md 2).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

class Files {
public:
    explicit Files(Output& output) : output_(output) {}
    /// Writes out and closes every open file (SYSTEM_TASKS.md 6.1).
    ~Files();
    Files(const Files&) = delete;
    Files& operator=(const Files&) = delete;

    /// $fopen(name): opens for writing and returns an mcd with the lowest
    /// free bit, or 0 if it cannot open or no bit is free.
    std::uint32_t openMcd(const std::string& name);
    /// $fopen(name, type): returns an fd with the lowest free number from
    /// 32'h8000_0003, or 0 if it cannot open; badType is set when type is
    /// not one of Table 21-6, and then nothing is opened.
    std::uint32_t openFd(const std::string& name, std::string_view type, bool& badType);
    /// The files desc names now.
    [[nodiscard]] FileTargets targets(W4 desc) const;
    /// Writes text to those of t that are still open as t named them.
    void write(const FileTargets& t, std::string_view text);
    /// Whether some of t is still open as t named it.
    [[nodiscard]] bool live(const FileTargets& t) const;
    /// $fclose: closes the files desc names; false if it names none open.
    bool close(W4 desc);
    /// $fflush(desc): false if desc names no open file.
    bool flush(W4 desc);
    /// $fflush with no argument: every open file and standard output.
    void flushAll();
    /// $fgets: reads a line of the file of fd desc, its newline included,
    /// into line (SYSTEM_TASKS.md 6.3). notOpen is set when desc names no
    /// open file; an mcd, STDIN and the end of the file read nothing.
    void readLine(W4 desc, std::string& line, bool& notOpen);

private:
    struct File {
        std::FILE* f = nullptr;
        std::uint64_t serial = 0;
    };
    /// Rows 1 to 30 are the bits of an mcd; row 32 + n is fd 32'h8000_0000 + n.
    static constexpr std::uint32_t kFdRow = 32;
    [[nodiscard]] bool open(std::uint32_t row) const {
        return row < rows_.size() && rows_[row].f != nullptr;
    }
    std::uint32_t place(std::uint32_t row, std::FILE* f);

    Output& output_;
    std::vector<File> rows_;
    std::uint64_t serial_ = 0;
};

}  // namespace kirimasim::kernel
