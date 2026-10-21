// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/front/front.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "lowerer.h"
#include "slang/ast/Compilation.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/driver/Driver.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxTree.h"

namespace kirimasim::front {

bool setUpDriver(slang::driver::Driver& driver, std::span<const std::string> args) {
    driver.addStandardArgs();
    std::vector<const char*> argv;
    argv.push_back("kirimasim");
    argv.push_back("--std=1800-2023");  // always IEEE 1800-2023 (CLI.md 2.1)
    for (const auto& a : args)
        argv.push_back(a.c_str());
    if (!driver.parseCommandLine(static_cast<int>(argv.size()), argv.data())) return false;
    // The threads of slang's pool have the system's default stack, too
    // small for statements nested about 1000 deep on macOS (#35).
    driver.options.numThreads = 1;
    return driver.processOptions();
}

bool chooseConfigTop(slang::driver::Driver& driver, std::string& diagnostics) {
    namespace syntax = slang::syntax;
    if (!driver.options.topModules.empty()) return true;
    const syntax::ConfigDeclarationSyntax* first = nullptr;
    for (const auto& tree : driver.syntaxTrees) {
        if (tree->isLibraryUnit || tree->root().kind != syntax::SyntaxKind::CompilationUnit)
            continue;
        for (const syntax::MemberSyntax* member :
             tree->root().as<syntax::CompilationUnitSyntax>().members) {
            if (member->kind != syntax::SyntaxKind::ConfigDeclaration) continue;
            const auto& config = member->as<syntax::ConfigDeclarationSyntax>();
            if (first == nullptr) {
                first = &config;
                continue;
            }
            const slang::SourceManager& sm = driver.sourceManager;
            const slang::SourceLocation sl = config.name.location();
            diagnostics +=
                std::string(sm.getFileName(sl)) + ":" + std::to_string(sm.getLineNumber(sl)) + ":" +
                std::to_string(sm.getColumnNumber(sl)) + ": error: a second config '" +
                std::string(config.name.valueText()) + "' in the sources, after '" +
                std::string(first->name.valueText()) + "'; name the one to use with --top\n";
            return false;
        }
    }
    if (first != nullptr)
        driver.options.topModules.push_back(std::string(first->name.valueText()) + ":config");
    return true;
}

ParseResult parseAndElaborate(std::span<const std::string> files) {
    ParseResult result;
    result.numFiles = files.size();

    slang::driver::Driver driver;
    if (!setUpDriver(driver, files)) return result;

    // Collect diagnostics into a string instead of letting slang print them.
    driver.diagEngine.removeClient(driver.textDiagClient);
    auto client = std::make_shared<slang::TextDiagnosticClient>();
    driver.diagEngine.addClient(client);

    const bool parsed = driver.parseAllSources();
    result.numSyntaxTrees = driver.syntaxTrees.size();

    std::string configError;
    if (parsed && !chooseConfigTop(driver, configError)) {
        result.numErrors = driver.diagEngine.getNumErrors() + 1;
        result.numWarnings = driver.diagEngine.getNumWarnings();
        result.diagnostics = client->getString() + configError;
        return result;
    }
    bool compiled = false;
    if (parsed) compiled = driver.runFullCompilation(/* quiet */ true);

    result.numErrors = driver.diagEngine.getNumErrors();
    result.numWarnings = driver.diagEngine.getNumWarnings();
    result.diagnostics = client->getString();
    result.success = parsed && compiled && result.numErrors == 0;
    return result;
}

}  // namespace kirimasim::front
