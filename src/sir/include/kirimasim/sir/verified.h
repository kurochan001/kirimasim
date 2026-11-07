// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"

namespace kirimasim::sir {

class VerifiedDesign;
struct VerifyOutcome;

/// The one gate of SIR (docs/notes/verified-design.md): the checks of the
/// verifier, of activation scopes and of pattern layouts, in one place.
/// The Design moves in, and comes back inside a VerifiedDesign when every
/// check passes.
VerifyOutcome verifyDesign(Design design);

/// A Design that has passed verifyDesign. Only verifyDesign makes one, and
/// only a const Design is reachable from it, so it cannot change after the
/// check; release() gives the Design back, to be verified again.
class VerifiedDesign {
public:
    [[nodiscard]] const Design& design() const noexcept { return design_; }
    [[nodiscard]] Design release() && noexcept { return std::move(design_); }

private:
    explicit VerifiedDesign(Design design) noexcept : design_(std::move(design)) {}
    friend VerifyOutcome verifyDesign(Design design);
    Design design_;
};

/// What verifyDesign gives: a VerifiedDesign exactly when errors is empty.
struct VerifyOutcome {
    std::optional<VerifiedDesign> verified;
    std::vector<VerifyError> errors;
};

}  // namespace kirimasim::sir
