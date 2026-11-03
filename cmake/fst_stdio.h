// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <stdio.h>

// An empty FST can pass a null buffer with a zero byte count. Honor fwrite's
// zero-length result without passing that buffer to a nonnull libc parameter.
static inline size_t kirimasimFstWrite(const void* buffer, size_t size, size_t count,
                                       FILE* stream) {
    if (size == 0 || count == 0) return 0;
    return fwrite(buffer, size, count, stream);
}
