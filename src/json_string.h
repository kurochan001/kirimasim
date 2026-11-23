// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <string>
#include <string_view>

namespace kirimasim {
/// Append a JSON string. SIR names can contain arbitrary bytes. Preserve UTF-8
/// sequences and escape an isolated non-UTF-8 byte by its numeric value.
inline void appendJsonString(std::string& out, std::string_view text) {
    constexpr std::string_view hex = "0123456789abcdef";
    out += '"';
    while (!text.empty()) {
        const auto c = static_cast<unsigned char>(text.front());
        if (c >= 0x80) {
            const unsigned length = c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
            bool valid = c >= 0xc2 && c <= 0xf4 && text.size() >= length;
            for (unsigned i = 1; valid && i < length; ++i) {
                const auto b = static_cast<unsigned char>(text[i]);
                valid = (b & 0xc0) == 0x80;
                if (i == 1)
                    valid = valid && !(c == 0xe0 && b < 0xa0) && !(c == 0xed && b >= 0xa0) &&
                            !(c == 0xf0 && b < 0x90) && !(c == 0xf4 && b >= 0x90);
            }
            if (valid) {
                out.append(text.substr(0, length));
                text.remove_prefix(length);
                continue;
            }
        }
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20 || c >= 0x80) {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 15];
                } else {
                    out += static_cast<char>(c);
                }
        }
        text.remove_prefix(1);
    }
    out += '"';
}
}  // namespace kirimasim
