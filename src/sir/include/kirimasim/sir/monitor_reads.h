// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/sir/pattern.h"
#include <span>

namespace kirimasim::sir {
/// Static sensitivity of a verified guard, watch or postponed process.
/// Execution tiers bind reference and frame rows to the current activation.
struct MonitorReads {
    struct RefRead {
        FrameVarId var;
        std::uint32_t leaf;
        bool anyElement;
    };
    struct FrameRead {
        FrameVarId var;
        std::uint32_t lsb, width;
    };
    bool strength = false, heap = false, events = false;
    std::vector<SlotId> privateContainers;
    std::vector<SlotRange> reads;
    std::vector<RefRead> refs;
    std::vector<FrameRead> frames;
};
const std::vector<FrameVar>& effectiveFrame(const Design&, Owner);
MonitorReads monitorReads(const Design&, Owner, std::span<const PatternShape>);
}  // namespace kirimasim::sir
