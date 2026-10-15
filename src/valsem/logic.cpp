#include "haremasim/valsem/logic.h"

namespace haremasim::valsem {

namespace {

int idx(Logic v) { return static_cast<int>(v); }

}  // namespace

// Each table has rows for x and columns for y, in the order 0, 1, Z, X
// (VALUE_SEMANTICS.md 3.6; the standard's tables 11-11 to 11-15).

Logic logicAnd(Logic x, Logic y) {
    using enum Logic;
    static constexpr Logic table[4][4] = {
        /* 0 */ {Zero, Zero, Zero, Zero},
        /* 1 */ {Zero, One, X, X},
        /* Z */ {Zero, X, X, X},
        /* X */ {Zero, X, X, X},
    };
    return table[idx(x)][idx(y)];
}

Logic logicOr(Logic x, Logic y) {
    using enum Logic;
    static constexpr Logic table[4][4] = {
        /* 0 */ {Zero, One, X, X},
        /* 1 */ {One, One, One, One},
        /* Z */ {X, One, X, X},
        /* X */ {X, One, X, X},
    };
    return table[idx(x)][idx(y)];
}

Logic logicXor(Logic x, Logic y) {
    using enum Logic;
    static constexpr Logic table[4][4] = {
        /* 0 */ {Zero, One, X, X},
        /* 1 */ {One, Zero, X, X},
        /* Z */ {X, X, X, X},
        /* X */ {X, X, X, X},
    };
    return table[idx(x)][idx(y)];
}

Logic logicXnor(Logic x, Logic y) {
    using enum Logic;
    static constexpr Logic table[4][4] = {
        /* 0 */ {One, Zero, X, X},
        /* 1 */ {Zero, One, X, X},
        /* Z */ {X, X, X, X},
        /* X */ {X, X, X, X},
    };
    return table[idx(x)][idx(y)];
}

Logic logicNot(Logic x) {
    using enum Logic;
    static constexpr Logic table[4] = {One, Zero, X, X};
    return table[idx(x)];
}

bool isPosedge(Logic from, Logic to) {
    using enum Logic;
    // Rows: from, columns: to, in the order 0, 1, Z, X (table 9-2).
    static constexpr bool table[4][4] = {
        /* 0 */ {false, true, true, true},
        /* 1 */ {false, false, false, false},
        /* Z */ {false, true, false, false},
        /* X */ {false, true, false, false},
    };
    return table[idx(from)][idx(to)];
}

bool isNegedge(Logic from, Logic to) {
    using enum Logic;
    static constexpr bool table[4][4] = {
        /* 0 */ {false, false, false, false},
        /* 1 */ {true, false, true, true},
        /* Z */ {true, false, false, false},
        /* X */ {true, false, false, false},
    };
    return table[idx(from)][idx(to)];
}

}  // namespace haremasim::valsem
