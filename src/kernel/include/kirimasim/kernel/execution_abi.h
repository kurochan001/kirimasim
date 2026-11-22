// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/io_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_EXECUTION 18u
#define KIRIMA_EXECUTION_VERSION 1u
enum kirima_logical_op { KIRIMA_LOGICAL_AND, KIRIMA_LOGICAL_OR, KIRIMA_IMPLIES, KIRIMA_EQUIVALENT };
typedef struct kirima_execution_api {
    uint32_t (*math_name)(kirima_bytes name, uint32_t* row, uint32_t* arity);
    uint32_t (*math)(uint32_t row, const double* arguments, uint32_t arity, double*);
    uint32_t (*logical)(uint32_t op, kirima_w4 left, kirima_w4 right, kirima_w4*);
    uint32_t (*select_real)(kirima_w4 condition, kirima_w4 yes, kirima_w4 no, uint32_t short_real,
                            kirima_w4*);
    uint32_t (*descriptor_error)(kirima_w4 descriptor, uint32_t input, kirima_byte_sink, void*);
} kirima_execution_api;
#ifdef __cplusplus
}
#endif
