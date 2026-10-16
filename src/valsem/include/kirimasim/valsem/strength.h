// Reference semantics of the resolution of nets with strengths
// (VALUE_SEMANTICS.md 5), one bit at a time.
//
// resolve() follows the rule of 5.4 word for word: it tries every way of
// taking one strength point from each driver. The kernel's fast version is
// tested against it.

#pragma once

#include <cstdint>
#include <vector>

#include "kirimasim/valsem/logic.h"
#include "kirimasim/valsem/value.h"

namespace kirimasim::valsem {

/// The kinds of net that resolve (VALUE_SEMANTICS.md 5.1).
enum class NetKind : std::uint8_t {
    Wire,
    Tri,
    Wand,
    Triand,
    Wor,
    Trior,
    Tri0,
    Tri1,
    Supply0,
    Supply1,
    Uwire,
};

/// The signal of one bit (5.2): the range [lo, hi] on the line from -7 to
/// +7, where -s is the strength s of a 0, +s the strength s of a 1 and 0
/// is highz.
struct Signal {
    int lo = 0;
    int hi = 0;
    friend bool operator==(const Signal&, const Signal&) = default;
};

/// One bit of a driver: its value and its strengths for 1 and for 0, each
/// from 0 (highz) to 7 (supply) (5.3, table 28-7).
struct Driver {
    Logic value = Logic::Z;
    int s1 = 6;
    int s0 = 6;
};

/// The signal of one driver bit (5.3): 0 is [-s0, -s0], 1 is [s1, s1], x
/// is [-s0, s1] and z is [0, 0].
Signal driverSignal(const Driver& d);

/// The signal of one bit of a net of the given kind with these drivers,
/// adding the pull of tri0 and tri1 and the supply of supply0 and supply1
/// (5.3, 5.4).
Signal resolve(NetKind kind, const std::vector<Driver>& drivers);

/// The four-state value of a signal; L and H read as X (5.2).
Logic valueOf(Signal s);

/// The value of a net of `width` bits whose drivers have the given values
/// (of that width) and strengths: resolve() for every bit.
Value resolveNet(NetKind kind, std::uint32_t width, const std::vector<Value>& values,
                 const std::vector<int>& s1, const std::vector<int>& s0);

}  // namespace kirimasim::valsem
