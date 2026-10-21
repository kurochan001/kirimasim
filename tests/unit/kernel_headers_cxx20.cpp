// Includes every public kernel header so that they are compiled as C++20.
// Keep this list in sync with src/kernel/include/kirimasim/kernel/.

#include "kirimasim/kernel/abi.h"
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/counters.h"
#include "kirimasim/kernel/edge.h"
#include "kirimasim/kernel/files.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/memfile.h"
#include "kirimasim/kernel/random.h"
#include "kirimasim/kernel/scan.h"
#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/strength.h"
#include "kirimasim/kernel/strings.h"
#include "kirimasim/kernel/udp.h"
#include "kirimasim/kernel/w4.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/kernel/wide.h"

static_assert(__cplusplus == 202002L, "this file must be compiled as C++20");
