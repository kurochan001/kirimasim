// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The files a design opens (SYSTEM_TASKS.md 6): multichannel descriptors
// (mcd) and file descriptors (fd). Standard output and standard error are
// the Context's Output, so that every path to them keeps one order
// (SYSTEM_TASKS.md 2); STDIN reads the stream the Context was given.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/kernel/scan.h"
#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

class Files {
public:
    /// in is what STDIN reads (SYSTEM_TASKS.md 6.3); nullptr reads nothing.
    Files(Output& output, std::FILE* in);
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

    // Input (SYSTEM_TASKS.md 6.3, 6.4). Each takes an fd and sets notOpen
    // when desc names no open fd (an mcd, x or z bits, a closed number);
    // then nothing is read and the error value is returned. Reading a file
    // that only writes fails with EBADF, once what $ungetc pushed back on
    // it has been read.

    /// $fgetc: the next byte, or -1 at the end of the file or on an error.
    int getChar(W4 desc, bool& notOpen);
    /// $ungetc: pushes c back, to be read before the rest, on any open
    /// file; 0, or -1.
    int ungetChar(int c, W4 desc, bool& notOpen);
    /// $fgets: reads up to a newline, which is included, the end of the
    /// file, or max characters (0 for no limit) into line.
    void readLine(W4 desc, std::size_t max, std::string& line, bool& notOpen);
    /// $fscanf: as scanString, taking from the file exactly the characters
    /// the conversions read; -1 when the input ends first or on an error.
    int scan(W4 desc, std::string_view format, std::vector<ScanValue>& out, bool& notOpen);
    /// $fread: up to n bytes into bytes.
    void readBytes(W4 desc, std::size_t n, std::string& bytes, bool& notOpen);
    /// $ftell: the position of the next byte read, what was pushed back
    /// counting as not read yet; -1 on an error.
    std::int64_t tell(W4 desc, bool& notOpen);
    /// $fseek and $rewind: from 0, 1 or 2 is the start, the position or the
    /// end; drops what was pushed back. 0, or -1.
    int seek(W4 desc, std::int64_t offset, std::int64_t from, bool& notOpen);
    /// $feof: 1 once a read met the end of the file, until a seek.
    int atEnd(W4 desc, bool& notOpen);
    /// $ferror: the error number of the most recent operation on desc, 0
    /// if it succeeded, and its text; EBADF for a desc that is not open.
    int error(W4 desc, std::string& text);

private:
    struct File {
        std::FILE* f = nullptr;
        std::uint64_t serial = 0;
        bool owned = true;   ///< closed by Files (not STDIN's stream)
        bool reads = false;  ///< opened with r or a type for update
        bool writes = true;
        std::string back;      ///< pushed back, the next one read last
        bool reading = false;  ///< the last operation read (C needs a seek between)
        bool end = false;      ///< a read met the end of the file
        int err = 0;           ///< errno of the most recent operation
    };
    /// Rows 1 to 30 are the bits of an mcd; row 32 + n is fd 32'h8000_0000 + n.
    static constexpr std::uint32_t kFdRow = 32;
    [[nodiscard]] bool open(std::uint32_t row) const {
        return row < rows_.size() && rows_[row].f != nullptr;
    }
    std::uint32_t place(std::uint32_t row, std::FILE* f);
    /// The row of fd desc for input, or nullptr with notOpen set.
    File* reader(W4 desc, bool& notOpen);
    /// Starts an operation that reads r: false, with EBADF, when r only
    /// writes and has nothing pushed back.
    bool startRead(File& r);
    /// The next character of r, or -1 at the end or on an error.
    int take(File& r);
    /// Readies r to be written after reading.
    void startWrite(File& r);

    Output& output_;
    std::vector<File> rows_;
    std::uint64_t serial_ = 0;
};

}  // namespace kirimasim::kernel
