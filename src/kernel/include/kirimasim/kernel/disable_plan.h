// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace kirimasim::kernel {
/// SCHEDULER_SPEC 10.4. The caller owns the frame stacks. The plan uses
/// indices, so it can be computed before ending children or releasing scopes.
template <class Block> struct UnwindPoint {
    std::uint32_t calls, sync;
    Block exit;
    bool at(std::size_t taskDepth, std::size_t syncDepth) const noexcept {
        return calls == taskDepth && sync == syncDepth;
    }
};
template <class Block> struct DisablePlan {
    UnwindPoint<Block> to;
    std::size_t keep;
    std::uint64_t children;
    bool scope;
};
template <class Scopes, class Calls, class Synchronous, class Body, class Block>
std::optional<DisablePlan<Block>>
planDisable(bool task, std::uint32_t scope, Body taskBody, Block returnToCaller, bool current,
            const Scopes& scopes, const Calls& calls, const Synchronous& synchronous) {
    if (!task) {
        for (std::size_t i = 0; i < scopes.size(); ++i) {
            const auto& a = scopes[i];
            if (a.scope == scope)
                return DisablePlan<Block>{{a.calls, a.sync, a.exit}, i, a.children, true};
        }
        return {};
    }
    const auto target = [&](std::uint32_t depth, std::uint32_t sync, std::uint64_t children) {
        auto keep = scopes.size();
        while (keep && (scopes[keep - 1].calls > depth ||
                        (scopes[keep - 1].calls == depth && scopes[keep - 1].sync > sync)))
            --keep;
        return DisablePlan<Block>{{depth, sync, returnToCaller}, keep, children, false};
    };
    // Disabling a recursive task ends its outermost matching invocation.
    for (std::size_t i = 0; i < calls.size(); ++i)
        if (calls[i].body == taskBody)
            return target(static_cast<std::uint32_t>(i), 0, calls[i].children);
    if (current) {
        for (std::size_t i = 0; i < synchronous.size(); ++i)
            if (synchronous[i].body == taskBody)
                return target(static_cast<std::uint32_t>(calls.size()),
                              static_cast<std::uint32_t>(i), synchronous[i].children);
    }
    return {};
}

enum class DisableActivity { Continue, Flush, EndTree, Error };
inline DisableActivity disableActivity(bool matches, bool inherited, bool inFunction,
                                       bool ownBlock) noexcept {
    if (!matches) return DisableActivity::Continue;
    if (inFunction && (!inherited || !ownBlock)) return DisableActivity::Error;
    return inherited ? DisableActivity::EndTree : DisableActivity::Flush;
}
template <class Block>
bool mayUnwind(const UnwindPoint<Block>& to, bool inFunction, bool current, std::size_t depth,
               std::size_t sync) noexcept {
    return !inFunction || (current && to.at(depth, sync));
}
}  // namespace kirimasim::kernel
