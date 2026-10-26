// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original dependency smoke: all pack modes create a file readable by libfst.
// This does not enable SystemVerilog waveform output.
#include <cstdio>
#include <string>

#include <fstapi.h>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
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
    return 0;
}
