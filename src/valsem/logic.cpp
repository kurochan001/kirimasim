#include "haremasim/valsem/logic.h"

namespace haremasim::valsem {

Logic logicAnd(Logic x, Logic y) {
    using enum Logic;
    // Rows: x, columns: y, in the order 0, 1, Z, X.
    static constexpr Logic table[4][4] = {
        /* 0 */ {Zero, Zero, Zero, Zero},
        /* 1 */ {Zero, One, X, X},
        /* Z */ {Zero, X, X, X},
        /* X */ {Zero, X, X, X},
    };
    return table[static_cast<int>(x)][static_cast<int>(y)];
}

}  // namespace haremasim::valsem
