// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The libFuzzer target of front and tier 1 (the memory-safety plan, step
// 5; CONTRIBUTING.md, "Building and testing"). Each input is the text of
// one SystemVerilog file. front compiles it, and a design that compiles
// runs under small limits, so that every input ends soon. A crash, a
// report of a sanitizer or a leak is a finding; diagnostics, run-time
// errors of the design and the limits are not.
//
// slang is built as in a release, without its assertions, and its
// unreachable points are left out of UndefinedBehaviorSanitizer (the fuzz
// preset): a failed assertion of slang is slang's finding, to be reported
// upstream as slang is used unmodified (CLAUDE.md), and one can fail in a
// destructor, which ends the process. AddressSanitizer still sees the
// memory errors of slang too.
//
// Each input runs on a thread with the stack the driver gives (RUNTIME_ABI.md
// 5), so that what the driver handles is not reported. The designs run in
// a directory of their own, where what they write with $fopen goes.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <pthread.h>
#include <unistd.h>

#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/vm/vm.h"

namespace {

namespace kernel = kirimasim::kernel;

struct Input {
    const std::uint8_t* data;
    std::size_t size;
};

/// The directory the designs run in, removed at exit.
std::filesystem::path workDir;

/// The stack of the driver's simulation thread (src/driver/main.cpp).
constexpr std::size_t kStackBytes = std::size_t{512} << 20;

void compileAndRun(const Input& in) {
    {
        std::ofstream f("input.sv", std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(in.data), static_cast<std::streamsize>(in.size));
    }
    auto compiled = kirimasim::front::compile(std::vector<std::string>{"input.sv"});
    if (!compiled.success) return;
    std::FILE* sink = std::fopen("/dev/null", "w");
    if (sink == nullptr) return;
    {
        kernel::Context context(kernel::OutputDestinations{sink, sink});
        kernel::Limits limits;
        limits.maxTime = 100000;  // in the design's precision
        limits.maxZeroDelay = 1000;
        limits.maxSlotEvents = 100000;
        context.scheduler().setLimits(limits);
        kirimasim::vm::RunOptions options;
        options.maxSteps = 100000;
        kirimasim::vm::run(compiled.design, context, options);
        context.output().flush();
    }
    std::fclose(sink);
}

}  // namespace

#if defined(__APPLE__)
// On macOS libFuzzer's own runtime uses the system's libc++, whose
// containers carry no annotations for AddressSanitizer, while the code
// built here does: where the two meet, as in libFuzzer's parsing of its
// flags, AddressSanitizer reports container overflows that are not
// there. The asan preset keeps the check for kirimasim's code.
// The name is AddressSanitizer's, which calls it to read its options.
extern "C" const char* __asan_default_options() {  // NOLINT(bugprone-reserved-identifier)
    return "detect_container_overflow=0";
}
#endif

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
    // Under the system's directory for temporary files: only this test
    // program reads the environment (CLAUDE.md is about src/).
    std::string dir = (std::filesystem::temp_directory_path() / "kirimasim-fuzz-XXXXXX").string();
    if (mkdtemp(dir.data()) == nullptr || chdir(dir.c_str()) != 0) {
        std::perror("kirimasim_fuzz_compile: no directory to work in");
        std::exit(1);
    }
    workDir = dir;
    std::atexit([] {
        std::error_code ignored;
        std::filesystem::remove_all(workDir, ignored);
    });
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    Input in{data, size};
    pthread_attr_t attr;
    pthread_t thread;
    if (pthread_attr_init(&attr) != 0) std::abort();
    auto body = [](void* p) -> void* {
        compileAndRun(*static_cast<const Input*>(p));
        return nullptr;
    };
    if (pthread_attr_setstacksize(&attr, kStackBytes) != 0 ||
        pthread_create(&thread, &attr, body, &in) != 0)
        std::abort();
    pthread_attr_destroy(&attr);
    pthread_join(thread, nullptr);
    return 0;
}
