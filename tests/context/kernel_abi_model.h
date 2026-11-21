// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "kirimasim/kernel/abi.h"

// A small hand-written model and its host share only this C boundary.
typedef struct model_frame {
    const kirima_kernel_api* api;
    uint32_t process;
    uint32_t resumes;
    uint64_t times[2];
    uint64_t sample[4];
} model_frame;

#ifdef __cplusplus
extern "C" {
#endif
int start_abi_model(const kirima_kernel_api* api, kirima_context* context, model_frame* frame);
#ifdef __cplusplus
}
#endif
