// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/context.h"
#include <span>
#include <string>
namespace kirimasim::driver {
std::string loadVpi(kernel::Context&, std::span<const std::string> files);
}
