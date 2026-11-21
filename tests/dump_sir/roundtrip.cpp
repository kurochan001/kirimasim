// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "kirimasim/front/front.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/text_v1.h"

int main(int argc, char** argv) try {
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string> sourceArgs(arguments.begin() + 1, arguments.end());
    const auto original = kirimasim::front::compile(sourceArgs);
    if (!original.success || !original.verified) {
        std::cerr << original.diagnostics << original.verifyErrors;
        return original.unsupported ? 77 : 1;
    }

    using namespace kirimasim::sir;
    const auto text = print1(*original.verified);
    if (!text) {
        std::cerr << "the draft schema cannot represent this design\n";
        return 1;
    }
    const auto parsed = parse1(*text);
    if (!parsed.verified) {
        for (const auto& error : parsed.errors)
            std::cerr << error.where << ": " << error.message << '\n';
        return 1;
    }
    if (print1(*parsed.verified) != text) {
        std::cerr << "format 1 differs after readback\n";
        return 1;
    }
    if (toText(parsed.verified->design()) != toText(original.verified->design())) {
        std::cerr << "format 0 changes after format 1 readback\n";
        return 1;
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
