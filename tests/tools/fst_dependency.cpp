// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original dependency smoke: all pack modes create a file readable by libfst.
// This does not enable SystemVerilog waveform output.
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include <fstapi.h>

namespace {
// Make a forbidden zero-length libc call observable even on systems whose
// stdio declarations do not carry the Linux nonnull attribute.
std::size_t observeWrite(const void* buffer, std::size_t size, std::size_t count,
                         std::FILE* stream) {
    if (size == 0 || count == 0) {
        std::fputc('!', stream);
        return 0;
    }
    return std::fwrite(buffer, size, count, stream);
}
}  // namespace

#define fwrite observeWrite
#include "../../cmake/fst_stdio.h"
#undef fwrite

int main(int argc, char** argv) {
    // Original CLI tests use the reader as an independent value decoder.
    if (argc == 3 && std::string(argv[1]) == "--decode") {
        auto* reader = fstReaderOpen(argv[2]);
        if (reader == nullptr) return 1;
        // Empty selections are valid FST, but have no VCD hierarchy conversion.
        const bool hierarchy =
            fstReaderGetVarCount(reader) == 0 || fstReaderProcessHier(reader, stdout) != 0;
        fstReaderSetFacProcessMaskAll(reader);
        const bool valid = hierarchy &&
                           fstReaderIterBlocks(reader, nullptr, nullptr, stdout) != 0 &&
                           fstReaderGetFseekFailed(reader) == 0;
        fstReaderClose(reader);
        return valid && std::fflush(stdout) == 0 && std::ferror(stdout) == 0 ? 0 : 1;
    }
    if (argc != 2) return 1;
    const auto writes = [] {
        auto* stream = std::tmpfile();
        if (stream == nullptr) return false;
        const char bytes[] = "0x1z";
        bool valid = true;
        for (const auto& [size, count] :
             {std::pair<std::size_t, std::size_t>{0, 1}, {1, 0}, {0, 0}})
            for (const void* buffer :
                 {static_cast<const void*>(nullptr), static_cast<const void*>(bytes)})
                valid = kirimasimFstWrite(buffer, size, count, stream) == 0 && valid;
        valid = std::ftell(stream) == 0 && std::ferror(stream) == 0 && valid;
        valid = kirimasimFstWrite(bytes, 1, 4, stream) == 4 && valid;
        valid = kirimasimFstWrite(bytes, 2, 2, stream) == 2 && valid;
        valid = std::fseek(stream, 0, SEEK_SET) == 0 && valid;
        char observed[8]{};
        valid = std::fread(observed, 1, sizeof(observed), stream) == sizeof(observed) &&
                std::memcmp(observed, "0x1z0x1z", sizeof(observed)) == 0 && valid;
        return std::fclose(stream) == 0 && valid;
    };
    if (!writes()) {
        std::fprintf(stderr, "FST stdio contract check failed\n");
        return 1;
    }
    const auto check = [&](enum fstWriterPackType pack, const char* suffix) {
        const std::string path = std::string(argv[1]) + suffix;
        auto* writer = fstWriterCreate(path.c_str(), 1);
        if (writer == nullptr) return false;
        fstWriterSetPackType(writer, pack);
        fstWriterSetTimescale(writer, -9);
        fstWriterSetScope(writer, FST_ST_VCD_MODULE, "top", nullptr);
        const auto signal =
            fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "signal", 0);
        fstWriterSetUpscope(writer);
        const char* values[] = {"0", "1", "x", "z"};
        for (unsigned time = 0; time < 4; ++time) {
            fstWriterEmitTimeChange(writer, time);
            fstWriterEmitValueChange(writer, signal, values[time]);
        }
        fstWriterClose(writer);
        auto* reader = fstReaderOpen(path.c_str());
        if (reader == nullptr) {
            std::remove(path.c_str());
            return false;
        }
        const bool valid = signal != 0 && fstReaderGetVarCount(reader) == 1 &&
                           fstReaderGetStartTime(reader) == 0 && fstReaderGetEndTime(reader) == 3;
        fstReaderClose(reader);
        return std::remove(path.c_str()) == 0 && valid;
    };
    if (!check(FST_WR_PT_ZLIB, ".zlib.fst") || !check(FST_WR_PT_FASTLZ, ".fastlz.fst") ||
        !check(FST_WR_PT_LZ4, ".lz4.fst"))
        return 1;

    // Empty designs and filters that select no variables must finalize readable
    // files too, across compression, hierarchy and flush/time boundaries.
    const auto empty = [&](enum fstWriterPackType pack, bool scope, bool later) {
        const auto path = std::string(argv[1]) + ".empty-" + std::to_string(pack) + "-" +
                          std::to_string(scope) + "-" + std::to_string(later) + ".fst";
        auto* writer = fstWriterCreate(path.c_str(), 1);
        if (writer == nullptr) return false;
        fstWriterSetPackType(writer, pack);
        if (scope) {
            fstWriterSetScope(writer, FST_ST_VCD_MODULE, "top", nullptr);
            fstWriterSetUpscope(writer);
        }
        fstWriterEmitTimeChange(writer, 0);
        if (later) {
            fstWriterFlushContext(writer);
            fstWriterEmitTimeChange(writer, 3);
        }
        fstWriterClose(writer);
        auto* reader = fstReaderOpen(path.c_str());
        if (reader == nullptr) {
            std::remove(path.c_str());
            return false;
        }
        const bool valid = fstReaderGetVarCount(reader) == 0 &&
                           fstReaderGetStartTime(reader) == 0 &&
                           fstReaderGetEndTime(reader) == (later ? 3U : 0U) &&
                           fstReaderIterBlocks(reader, nullptr, nullptr, nullptr) != 0 &&
                           fstReaderGetFseekFailed(reader) == 0;
        fstReaderClose(reader);
        return std::remove(path.c_str()) == 0 && valid;
    };
    for (const auto pack : {FST_WR_PT_ZLIB, FST_WR_PT_FASTLZ, FST_WR_PT_LZ4})
        for (const bool scope : {false, true})
            for (const bool later : {false, true})
                if (!empty(pack, scope, later)) {
                    std::fprintf(stderr, "Empty FST check failed: pack=%d scope=%d later=%d\n",
                                 pack, scope, later);
                    return 1;
                }
    return 0;
}
