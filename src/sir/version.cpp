// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/version.h"

#include "kirimasim/sir/text_v1.h"

namespace kirimasim::sir {

std::uint32_t formatVersion() { return kTextV1Version; }

}  // namespace kirimasim::sir
