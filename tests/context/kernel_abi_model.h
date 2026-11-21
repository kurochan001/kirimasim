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
int check_c_value_model(const kirima_kernel_api* api);
int check_c_io_model(const kirima_kernel_api*, kirima_context*, kirima_context*, const char* path,
                     uint64_t* string_handle);
int start_abi_model(const kirima_kernel_api* api, kirima_context* context, model_frame* frame);
#ifdef __cplusplus
}
#endif
