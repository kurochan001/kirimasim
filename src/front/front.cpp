// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/front/front.h"

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lowerer.h"
#include "query_checks.h"
#include "slang/analysis/AnalysisManager.h"
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
    // slang's errors on code the standard allows are warnings (CLI.md 2.1),
    // before the user's options, so that -Werror=<name> wins.
    for (const char* allowed : {"-Wno-error=index-oob", "-Wno-error=range-oob",
                                "-Wno-error=string-int-concat", "-Wno-error=string-in-constraint"})
        argv.push_back(allowed);
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
    std::vector<const syntax::ConfigDeclarationSyntax*> configs;
    for (const auto& tree : driver.syntaxTrees) {
        if (tree->isLibraryUnit || tree->root().kind != syntax::SyntaxKind::CompilationUnit)
            continue;
        for (const syntax::MemberSyntax* member :
             tree->root().as<syntax::CompilationUnitSyntax>().members) {
            if (member->kind == syntax::SyntaxKind::ConfigDeclaration)
                configs.push_back(&member->as<syntax::ConfigDeclarationSyntax>());
        }
    }
    if (configs.empty()) return true;

    // A config that another config uses (33.4.2) configures a part of the
    // design below its user; the others are the top configs.
    std::vector<std::string_view> used;
    for (const auto* config : configs) {
        for (const syntax::ConfigRuleSyntax* rule : config->rules) {
            const syntax::ConfigRuleClauseSyntax* clause = nullptr;
            if (rule->kind == syntax::SyntaxKind::InstanceConfigRule)
                clause = rule->as<syntax::InstanceConfigRuleSyntax>().ruleClause;
            else if (rule->kind == syntax::SyntaxKind::CellConfigRule)
                clause = rule->as<syntax::CellConfigRuleSyntax>().ruleClause;
            if (clause == nullptr || clause->kind != syntax::SyntaxKind::ConfigUseClause) continue;
            const auto& use = clause->as<syntax::ConfigUseClauseSyntax>();
            if (use.name != nullptr && use.config.valid() && !use.config.isMissing())
                used.push_back(use.name->cell.valueText());
        }
    }
    std::vector<const syntax::ConfigDeclarationSyntax*> tops;
    for (const auto* config : configs) {
        if (std::find(used.begin(), used.end(), config->name.valueText()) == used.end())
            tops.push_back(config);
    }

    // The second of two configs that fit, which the standard leaves open.
    const auto ambiguous = [&](const syntax::ConfigDeclarationSyntax& first,
                               const syntax::ConfigDeclarationSyntax& second, std::string_view why,
                               std::string_view ask) {
        const slang::SourceManager& sm = driver.sourceManager;
        const slang::SourceLocation sl = second.name.location();
        diagnostics += std::string(sm.getFileName(sl)) + ":" +
                       std::to_string(sm.getLineNumber(sl)) + ":" +
                       std::to_string(sm.getColumnNumber(sl)) + ": error: a second config '" +
                       std::string(second.name.valueText()) + "'" + std::string(why) + ", after '" +
                       std::string(first.name.valueText()) + "'; name the one to use with " +
                       std::string(ask) + "\n";
        return false;
    };

    auto& topModules = driver.options.topModules;
    if (topModules.empty()) {
        if (tops.size() > 1) return ambiguous(*tops[0], *tops[1], " in the sources", "--top");
        if (tops.size() == 1)
            topModules.push_back(std::string(tops[0]->name.valueText()) + ":config");
        return true;
    }
    // --top naming a module takes the top config whose design statement
    // names it, as putting that config among the sources gives it (33.5.4).
    for (std::string& top : topModules) {
        if (top.ends_with(":config")) continue;
        const syntax::ConfigDeclarationSyntax* chosen = nullptr;
        for (const auto* config : tops) {
            const bool names = std::any_of(config->topCells.begin(), config->topCells.end(),
                                           [&](const syntax::ConfigCellIdentifierSyntax* cell) {
                                               return cell->cell.valueText() == top;
                                           });
            if (!names) continue;
            if (chosen != nullptr) {
                return ambiguous(*chosen, *config, " names '" + top + "' in its design statement",
                                 "--top=<config>:config");
            }
            chosen = config;
        }
        if (chosen != nullptr) top = std::string(chosen->name.valueText()) + ":config";
    }
    // Two modules that one config names give it once.
    std::vector<std::string> unique;
    for (const std::string& top : topModules) {
        if (std::find(unique.begin(), unique.end(), top) == unique.end()) unique.push_back(top);
    }
    topModules = std::move(unique);
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
    QueryCheckResult queries;
    if (parsed) {
        auto compilation = driver.createCompilation();
        driver.reportCompilation(*compilation, /* quiet */ true);
        driver.runAnalysis(*compilation);
        compiled = driver.reportDiagnostics(/* quiet */ true);
        if (compiled) queries = checkQueryExpressions(*compilation);
    }

    result.numErrors = driver.diagEngine.getNumErrors() + queries.numErrors;
    result.numWarnings = driver.diagEngine.getNumWarnings();
    result.diagnostics = client->getString() + queries.diagnostics;
    result.success = parsed && compiled && result.numErrors == 0;
    return result;
}

}  // namespace kirimasim::front
