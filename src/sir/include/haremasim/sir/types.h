// Value types of SIR (SIR_SPEC.md section 4.1).

#pragma once

#include <cstdint>
#include <string>

namespace haremasim::sir {

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
};

/// A value type. Integral kinds carry a bit width of at least 1; other kinds
/// have width 0.
struct Type {
    TypeKind kind = TypeKind::Void;
    std::uint32_t width = 0;

    friend bool operator==(const Type&, const Type&) = default;
};

inline Type voidType() { return {TypeKind::Void, 0}; }
inline Type u2(std::uint32_t w) { return {TypeKind::U2, w}; }
inline Type s2(std::uint32_t w) { return {TypeKind::S2, w}; }
inline Type u4(std::uint32_t w) { return {TypeKind::U4, w}; }
inline Type s4(std::uint32_t w) { return {TypeKind::S4, w}; }

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

}  // namespace haremasim::sir
