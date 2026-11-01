// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "vpi_loader.h"
#include <dlfcn.h>
namespace kirimasim::driver {
namespace {
void closeLibrary(void* library) { (void)dlclose(library); }
}  // namespace
std::string loadVpi(kernel::Context& context, std::span<const std::string> files) {
    for (const auto& file : files) {
        void* library = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!library) {
            const char* error = dlerror();
            return "cannot load VPI plugin '" + file + "': " + (error ? error : "loader error");
        }
        void* symbol = dlsym(library, "vlog_startup_routines");
        if (!symbol) {
            closeLibrary(library);
            return "VPI plugin '" + file + "' has no vlog_startup_routines";
        }
        context.vpi().ownLibrary(library, &closeLibrary);
        if (!context.vpi().startup(reinterpret_cast<void (**)()>(symbol)))
            return context.vpi().error();
    }
    return {};
}
}  // namespace kirimasim::driver
