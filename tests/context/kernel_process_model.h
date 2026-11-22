// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/process_abi.h"

typedef struct process_fixture {
    const kirima_process_api* api;
    kirima_process parent, child, self;
    uint32_t delivered, dropped, calls, child_calls, self_calls, error;
} process_fixture;
#ifdef __cplusplus
extern "C" {
#endif
int start_process_fixture(const kirima_kernel_api*, kirima_context*, kirima_context*,
                          process_fixture*);
int finish_process_fixture(kirima_context*, process_fixture*);
int start_program_fixture(const kirima_kernel_api*, kirima_context*, process_fixture*);
#ifdef __cplusplus
}
#endif
