// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "foreign.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/SystemSubroutine.h"
#include "slang/ast/types/Type.h"
#include <memory>
namespace kirimasim::front {
namespace {
class Foreign final : public slang::ast::SystemSubroutine {
public:
    Foreign(std::string name, const slang::ast::Type& result, bool task)
        : SystemSubroutine(std::move(name), task ? slang::ast::SubroutineKind::Task
                                                 : slang::ast::SubroutineKind::Function),
          result_(result) {}
    const slang::ast::Type& checkArguments(const slang::ast::ASTContext&, const Args&,
                                           slang::SourceRange,
                                           const slang::ast::Expression*) const override {
        return result_;
    }
    slang::ConstantValue eval(slang::ast::EvalContext& context, const Args&,
                              slang::SourceRange range,
                              const slang::ast::CallExpression::SystemCallInfo&) const override {
        notConst(context, range);
        return nullptr;
    }

private:
    const slang::ast::Type& result_;
};
}  // namespace
bool registerForeign(slang::ast::Compilation& compilation, std::span<const ForeignRoutine> routines,
                     std::string& error) {
    for (const auto& routine : routines) {
        if (compilation.getSystemSubroutine(routine.name)) {
            error = "kirimasim: error: VPI registration conflicts with system routine '" +
                    routine.name + "'\n";
            return false;
        }
        const slang::ast::Type* result = &compilation.getVoidType();
        if (routine.result.kind == sir::TypeKind::Real)
            result = &compilation.getRealType();
        else if (sir::isIntegral(routine.result)) {
            slang::bitmask<slang::ast::IntegralFlags> flags = slang::ast::IntegralFlags::FourState;
            if (sir::isSigned(routine.result)) flags |= slang::ast::IntegralFlags::Signed;
            result = &compilation.getType(routine.result.width, flags);
        }
        compilation.addSystemSubroutine(std::make_shared<Foreign>(
            routine.name, *result, routine.result.kind == sir::TypeKind::Void));
    }
    return true;
}
}  // namespace kirimasim::front
