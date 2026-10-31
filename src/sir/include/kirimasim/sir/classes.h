// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <span>
#include <vector>

#include "kirimasim/sir/verify.h"

namespace kirimasim::sir {

struct ClassValidation {
    std::vector<VerifyError> errors;
    std::vector<std::vector<ClassId>> ancestors;
    std::vector<std::vector<FrameVarId>> processReceivers, subroutineReceivers;
    [[nodiscard]] bool assignable(ClassId dynamic, ClassId view) const;
    [[nodiscard]] std::span<const FrameVarId> receivers(Owner owner) const;
};

ClassValidation validateClasses(const Design& design);
std::vector<VerifyError> verifyObjectInstruction(const Design& design,
                                                 const ClassValidation& classes,
                                                 const Subroutine* caller, const Instr& instruction,
                                                 std::span<const Type> operands);
bool hasClassMetadata(const Design& design);
Subroutine virtualSignature(const VirtualSlot& slot);
bool classReferenceShape(const Design& design, std::span<const FrameVar> frame,
                         std::span<const BasicBlock> blocks, const Operand& reference,
                         std::span<const ArrayDim> expected);

}  // namespace kirimasim::sir
