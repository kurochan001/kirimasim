// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/place_abi.h"
#include "kirimasim/kernel/sim.h"
namespace kirimasim::kernel {
struct ModelOrdinary {
    kirima_frame_provider* provider;
    kirima_frame_identity id;
    std::uint64_t offset;
};
struct ModelCell {
    HeapLayoutId layout;
    CellToken token;
    bool packed;
};
struct ModelField {
    ObjectRef field;
    std::optional<ObjectBitRef> bits;
};
using ModelPlaceSource =
    std::variant<SlotId, ModelOrdinary, ActivationBinding, ModelCell, ModelField>;
struct ModelPlace {
    ModelPlaceSource source{SlotId{0}};
    kirima_place_info info{};
    std::uint32_t fieldWidth = 0;
    ActivationLease activation;
    ObjectPin object;
    bool framePinned = false;
    ModelPlace() = default;
    ModelPlace(const ModelPlace&) = delete;
    ModelPlace& operator=(const ModelPlace&) = delete;
    ~ModelPlace() {
        if (framePinned) {
            const auto& f = *std::get_if<ModelOrdinary>(&source);
            f.provider->release(f.provider->frame, f.id);
        }
    }
};
struct ModelHooks {
    SafePointHook previousSafe;
    PulseHook previousPulse;
    std::uint64_t previous = 0;
};
}  // namespace kirimasim::kernel
