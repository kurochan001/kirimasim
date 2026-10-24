// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Typed borrowed values for SYSTEM_TASKS 3.9. This renderer has no knowledge
// of slots, source types, heap tokens or object allocation. Each tier supplies
// rooted value views, declaration order and language-ordered associative keys.
#pragma once

#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

#include "kirimasim/kernel/format.h"

namespace kirimasim::kernel {

/// Front supplies the source identifier and whether source spelling requires
/// an escape (including reserved words). The renderer does not lex keywords.
struct PatternName {
    std::string_view text;
    bool escaped = false;
};
struct PatternEnumName {
    IntValue value;
    PatternName name;
};

struct PatternValue {
    enum class Kind {
        Integral,
        Real,
        String,
        Enumeration,
        Record,
        Array,
        Associative,
        Union,
        TaggedUnion,
        Handle
    };
    Kind kind = Kind::Integral;
    IntValue integral;
    double real = 0;
    /// String bytes or a handle's printable type name.
    std::string_view text;
    /// A Context-assigned display ordinal, never a token/address. 0 is null.
    std::uint64_t ordinal = 0;
    PatternName tag;
    std::span<const PatternEnumName> enumerators;
    /// Record/union names correspond to children, both in declaration order.
    /// A tagged union has one child (the selected payload) and uses tag.
    std::span<const PatternName> names;
    std::span<const PatternValue> children;
    /// Associative keys correspond to children, already in language key order.
    std::span<const PatternValue> keys;
};

enum class PatternError { None, InvalidValue, Cycle, ResourceLimit };
struct PatternFormatResult {
    std::string text;
    PatternError error = PatternError::None;
    explicit operator bool() const { return error == PatternError::None; }
};

/// Renders %p; positional selects the specified %0p form without member names.
/// All views remain valid for the call. Handles are scalar leaves, so valid
/// source values cannot produce aggregate cycles. Malformed views and cyclic
/// aggregate views fail without partial output. Traversal does not recurse.
/// Scratch allocations use memory (the default resource when null); returned
/// text owns its bytes independently of that resource. Allocation exceptions
/// become ResourceLimit. This does not enable source-language %p by itself.
PatternFormatResult formatPattern(const PatternValue& value, bool positional = false,
                                  std::pmr::memory_resource* memory = nullptr);

}  // namespace kirimasim::kernel
