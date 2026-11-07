// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/verified.h"

#include <iterator>

#include "kirimasim/sir/pattern.h"

namespace kirimasim::sir {

VerifyOutcome verifyDesign(Design design) {
    // The checks the VM ran on load before this gate existed, in one place
    // (docs/notes/verified-design.md). Nothing new is checked here.
    VerifyOutcome outcome;
    outcome.errors = verify(design);
    auto scopes = verifyActivationScopes(design);
    outcome.errors.insert(outcome.errors.end(), std::make_move_iterator(scopes.begin()),
                          std::make_move_iterator(scopes.end()));
    auto patterns = validatePatterns(design);
    outcome.errors.insert(outcome.errors.end(), std::make_move_iterator(patterns.errors.begin()),
                          std::make_move_iterator(patterns.errors.end()));
    if (outcome.errors.empty()) outcome.verified.emplace(VerifiedDesign(std::move(design)));
    return outcome;
}

}  // namespace kirimasim::sir
