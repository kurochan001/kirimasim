// Includes every public kernel header so that they are compiled as C++20.
// Keep this list in sync with src/kernel/include/haremasim/kernel/.

#include "haremasim/kernel/abi.h"
#include "haremasim/kernel/bits.h"
#include "haremasim/kernel/context.h"
#include "haremasim/kernel/counters.h"
#include "haremasim/kernel/edge.h"
#include "haremasim/kernel/format.h"
#include "haremasim/kernel/sim.h"
#include "haremasim/kernel/store.h"
#include "haremasim/kernel/w4.h"
#include "haremasim/kernel/w4ops.h"

static_assert(__cplusplus == 202002L, "this file must be compiled as C++20");
