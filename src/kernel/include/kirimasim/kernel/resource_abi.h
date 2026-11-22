// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/abi.h"
/* Opaque, Context-owned, single-release tickets. The all-zero ticket denotes
 * absence. Pins, constructor stages, field bindings and event pins are distinct. */
typedef struct kirima_resource {
    const kirima_context* owner;
    uint64_t serial;
} kirima_resource;
