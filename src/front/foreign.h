// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/front/front.h"
#include <span>
namespace slang::ast {
class Compilation;
}
namespace kirimasim::front {
bool registerForeign(slang::ast::Compilation&, std::span<const ForeignRoutine>, std::string&);
}
