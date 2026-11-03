// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "fst_stdio.h"

// Keep the pinned dependency source untouched and scope the adapter to libfst.
#define fwrite kirimasimFstWrite
#include <fstapi.c>
#undef fwrite
