// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Observational probe of the pinned dependency, without front, SIR or VM.
#include "slang/ast/Compilation.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/syntax/SyntaxTree.h"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    auto tree = slang::syntax::SyntaxTree::fromFile(argv[1]);
    if (!tree) return 2;
    slang::ast::CompilationOptions options;
    options.topModules.emplace(argv[2]);
    slang::Bag bag;
    bag.set(options);
    slang::ast::Compilation compilation(bag);
    compilation.addSyntaxTree(tree.value());
    const auto& root = compilation.getRoot();
    for (int i = 3; i < argc; ++i) {
        const auto* symbol = root.lookupName(argv[i]);
        if (!symbol || symbol->kind != slang::ast::SymbolKind::Parameter) return 2;
        std::cout << argv[i] << '='
                  << symbol->as<slang::ast::ParameterSymbol>().getValue().toString() << '\n';
    }
    return compilation.hasIssuedErrors() ? 2 : 0;
}
