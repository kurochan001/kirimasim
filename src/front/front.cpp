#include "haremasim/front/front.h"

#include <memory>
#include <vector>

#include "slang/ast/Compilation.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/driver/Driver.h"

namespace haremasim::front {

ParseResult parseAndElaborate(std::span<const std::string> files) {
    ParseResult result;
    result.numFiles = files.size();

    slang::driver::Driver driver;
    driver.addStandardArgs();

    std::vector<const char*> argv;
    argv.push_back("haremasim");
    argv.push_back("--std=1800-2023");  // always IEEE 1800-2023 (CLI.md 2.1)
    for (const auto& f : files)
        argv.push_back(f.c_str());
    if (!driver.parseCommandLine(static_cast<int>(argv.size()), argv.data())) return result;
    if (!driver.processOptions()) return result;

    // Collect diagnostics into a string instead of letting slang print them.
    driver.diagEngine.removeClient(driver.textDiagClient);
    auto client = std::make_shared<slang::TextDiagnosticClient>();
    driver.diagEngine.addClient(client);

    const bool parsed = driver.parseAllSources();
    result.numSyntaxTrees = driver.syntaxTrees.size();

    bool compiled = false;
    if (parsed) compiled = driver.runFullCompilation(/* quiet */ true);

    result.numErrors = driver.diagEngine.getNumErrors();
    result.numWarnings = driver.diagEngine.getNumWarnings();
    result.diagnostics = client->getString();
    result.success = parsed && compiled && result.numErrors == 0;
    return result;
}

}  // namespace haremasim::front
