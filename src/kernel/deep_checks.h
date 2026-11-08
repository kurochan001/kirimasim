// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Deep checks of the kernel's invariants (docs/notes/deep-checks.md). The
// build option KIRIMASIM_DEEP_CHECKS turns them on (the debug presets) or off
// (release). They only read the kernel's state; a violation is reported
// through Scheduler::fail as an internal error. Private to the kernel.

#pragma once

namespace kirimasim::kernel {

#if defined(KIRIMASIM_DEEP_CHECKS) && KIRIMASIM_DEEP_CHECKS
inline constexpr bool kDeepChecks = true;
#else
inline constexpr bool kDeepChecks = false;
#endif

}  // namespace kirimasim::kernel
