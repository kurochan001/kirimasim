// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Every line of records.def names a struct, a member and an enumerator that
// exist in design.h (sir-v1-proposal.md, "The schema"): a wrong name stops the
// build. Nothing here runs.

#include "kirimasim/sir/design.h"

#include <type_traits>

namespace kirimasim::sir {

#define KIRIMASIM_SIR_RECORD(Struct, text) static_assert(std::is_class_v<Struct>, text);
#define KIRIMASIM_SIR_FIELD(Struct, member, kind)                                                  \
    static_assert(std::is_member_object_pointer_v<decltype(&Struct::member)>, #Struct "." #member);
#define KIRIMASIM_SIR_KIND(Struct, Kind, text) static_assert(std::is_enum_v<decltype(Kind)>, text);
#define KIRIMASIM_SIR_KIND_FIELD(Struct, Kind, member)                                             \
    static_assert(std::is_member_object_pointer_v<decltype(&Struct::member)>, #Kind "." #member);
#include "kirimasim/sir/records.def"
#undef KIRIMASIM_SIR_RECORD
#undef KIRIMASIM_SIR_FIELD
#undef KIRIMASIM_SIR_KIND
#undef KIRIMASIM_SIR_KIND_FIELD

}  // namespace kirimasim::sir
