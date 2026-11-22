// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#ifdef __APPLE__
#include <crt_externs.h>
#else
#include <unistd.h>
#endif

namespace kirimasim::driver {
inline char** childEnvironment() {
#ifdef __APPLE__
    return *_NSGetEnviron();
#else
    return environ;
#endif
}
}  // namespace kirimasim::driver
