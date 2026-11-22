// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/io_abi.h"
namespace kirimasim::kernel::abi {
inline bool restore(kirima_context* c, const kirima_file_targets* src, FileTargets& t) {
    if (!c || !src || src->owner != c || src->count > KIRIMA_MCD_FILES || (src->flags & ~7U))
        return false;
    t.out = (src->flags & KIRIMA_FILE_OUT) != 0;
    t.err = (src->flags & KIRIMA_FILE_ERR) != 0;
    t.notOpen = (src->flags & KIRIMA_FILE_NOT_OPEN) != 0;
    for (const auto& f : std::span(src->files, src->count)) {
        if (f.reserved || !f.serial || (!t.files.empty() && t.files.back().first >= f.row))
            return false;
        t.files.emplace_back(f.row, f.serial);
    }
    return true;
}
}  // namespace kirimasim::kernel::abi
