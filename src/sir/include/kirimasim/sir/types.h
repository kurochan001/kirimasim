// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Value types of SIR (SIR_SPEC.md section 4.1).

#pragma once

#include <cstdint>
#include <string>

#include "kirimasim/sir/id.h"

namespace kirimasim::sir {

struct TypeTag;
using TypeId = Id<TypeTag>;
struct ClassTag;
using ClassId = Id<ClassTag>;

/// Kind of a value type.
enum class TypeKind : std::uint8_t {
    Void,       ///< No value (instructions without a result).
    U2,         ///< Two-state, unsigned: i2<w>.
    S2,         ///< Two-state, signed: s2<w>.
    U4,         ///< Four-state, unsigned: i4<w>.
    S4,         ///< Four-state, signed: s4<w>.
    Real,       ///< real and realtime.
    ShortReal,  ///< shortreal.
    String,     ///< string (a handle).
    Event,      ///< Named event (a handle).
    Chandle,    ///< chandle.
    Ref,        ///< A reference to a variable, for ref arguments (SIR_SPEC.md 8; a handle).
    DynArray,   ///< Dynamic-array handle; detail names its type-table row.
    Queue,      ///< Queue handle; detail names its type-table row.
    Strength,   ///< Internal SSA metadata: strength<w>, one complete range per bit.
    Class,      ///< A class handle; object names its immutable class descriptor.
};

/// A value type. Integral and Strength kinds carry a positive width; other
/// kinds have width 0. Strength is not a SystemVerilog storage type.
struct Type {
    TypeKind kind = TypeKind::Void;
    std::uint32_t width = 0;
    /// Container descriptor; also the target of a ref to one container
    /// variable. kNone for other types and ordinary refs.
    TypeId detail = kNone;
    ClassId object = kNone;    ///< Static class view, also for a ref to a class variable.
    bool readOnlyRef = false;  ///< Permission of a typed class-variable reference.

    friend bool operator==(const Type&, const Type&) = default;
};

/// Integral constant storage units (SIR_SPEC 3.2/4.1). The front and
/// verifier share these; SIR remains independent of the execution kernel.
inline constexpr std::uint32_t integralWords(std::uint32_t width) {
    return width / 64 + (width % 64 != 0);
}
inline constexpr std::uint32_t integralBytes(std::uint32_t width) {
    return width / 8 + (width % 8 != 0);
}

inline Type voidType() { return {TypeKind::Void, 0}; }
inline Type refType(TypeId target = kNone) { return {TypeKind::Ref, 0, target}; }
inline Type dynarrayType(TypeId row) { return {TypeKind::DynArray, 0, row}; }
inline Type queueType(TypeId row) { return {TypeKind::Queue, 0, row}; }
inline Type classType(ClassId row) { return {TypeKind::Class, 0, kNone, row}; }
inline Type classRefType(ClassId row, bool readonly = false) {
    return {TypeKind::Ref, 0, kNone, row, readonly};
}
inline bool isClass(Type t) { return t.kind == TypeKind::Class; }
inline bool isContainer(Type t) {
    return t.kind == TypeKind::DynArray || t.kind == TypeKind::Queue;
}
inline Type u2(std::uint32_t w) { return {TypeKind::U2, w}; }
inline Type s2(std::uint32_t w) { return {TypeKind::S2, w}; }
inline Type u4(std::uint32_t w) { return {TypeKind::U4, w}; }
inline Type s4(std::uint32_t w) { return {TypeKind::S4, w}; }
inline Type strengthType(std::uint32_t w) { return {TypeKind::Strength, w}; }

inline bool isIntegral(Type t) {
    return t.kind == TypeKind::U2 || t.kind == TypeKind::S2 || t.kind == TypeKind::U4 ||
           t.kind == TypeKind::S4;
}
inline bool isFourState(Type t) { return t.kind == TypeKind::U4 || t.kind == TypeKind::S4; }
inline bool isSigned(Type t) { return t.kind == TypeKind::S2 || t.kind == TypeKind::S4; }

/// The integral type with the same width and signedness in the other state
/// domain (two-state <-> four-state). Non-integral types are returned as is.
Type otherDomain(Type t);

/// Text form used by the SIR text format: i2<8>, s4<32>, real, string, ...
std::string toString(Type t);

}  // namespace kirimasim::sir
