// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/text_abi.h"
#include "abi_boundary.h"
#include "abi_format_view.h"
#include "kirimasim/kernel/pattern.h"
#include "kirimasim/kernel/random.h"
#include "kirimasim/kernel/scan.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
constexpr U32 ok = KIRIMA_VALUE_OK, invalid = KIRIMA_VALUE_INVALID;
U32 parse(kirima_bytes src, kirima_piece_sink output, kirima_byte_sink diagnostic, void* frame,
          U32* valid) {
    if (!abi::valid(src) || !output || !diagnostic || !valid) return invalid;
    std::vector<FormatPiece> pieces;
    std::string error;
    *valid = parseFormat(abi::view(src), pieces, error);
    if (!*valid) {
        diagnostic(frame, abi::bytesOf(error));
        return ok;
    }
    for (const auto& p : pieces) {
        const auto& s = p.spec;
        kirima_format_piece v{abi::bytesOf(p.text),
                              abi::bytesOf(s.cformat),
                              p.isSpec,
                              static_cast<unsigned char>(s.conv),
                              s.left,
                              s.zero,
                              s.width};
        output(frame, &v);
    }
    return ok;
}
TimeFormat timeSettings(const kirima_format_context& c) {
    return {c.unit, c.decimals, std::string(abi::view(c.suffix)), c.min_width};
}
U32 scalar(kirima_bytes src, const kirima_format_arg* a, const kirima_format_context* ctx,
           kirima_byte_sink output, kirima_byte_sink diagnostic, void* frame) {
    if (!ctx || !a || !output || !diagnostic || !abi::valid(src) || !abi::valid(ctx->suffix) ||
        ctx->precision < -15 || ctx->precision > 2)
        return invalid;
    auto time = timeSettings(*ctx);
    const auto timeError = timeFormatError(time);
    if (!timeError.empty()) {
        diagnostic(frame, abi::bytesOf(timeError));
        return ok;
    }
    FormatSpec spec;
    if (src.size) {
        std::vector<FormatPiece> pieces;
        std::string error;
        if (!parseFormat(abi::view(src), pieces, error)) {
            diagnostic(frame, abi::bytesOf(error));
            return ok;
        }
        if (pieces.size() != 1 || !pieces.front().isSpec) return invalid;
        spec = std::move(pieces.front().spec);
    }
    FormatValue value;
    std::vector<std::uint64_t> words;
    if (!abi::captureFormat(*a, value, words)) return invalid;
    if (spec.conv == 'v' && !value.strength.empty()) {
        auto result = formatStrength(value.strength, value.strengthWidth, spec);
        if (!result.error.empty())
            diagnostic(frame, abi::bytesOf(result.error));
        else
            output(frame, abi::bytesOf(result.text));
        return ok;
    }
    if (a->kind > KIRIMA_FORMAT_STRING || spec.conv == 'm' || spec.conv == 'l' || spec.conv == 'p')
        return invalid;
    output(frame, abi::bytesOf(formatStaticValue(value, spec, ctx->precision, time)));
    return ok;
}
bool integral(const kirima_typed_integer& v, IntValue& out, std::vector<std::uint64_t>& storage) {
    kirima_format_arg src{};
    src.kind = KIRIMA_FORMAT_INTEGRAL;
    src.is_signed = v.is_signed;
    src.four_state = v.four_state;
    src.integral = v.value;
    FormatValue value;
    if (!abi::captureFormat(src, value, storage)) return false;
    out = value.integral;
    return true;
}
bool name(kirima_pattern_name s, PatternName& out) {
    if (!abi::valid(s.text) || s.escaped > 1) return false;
    out = {abi::view(s.text), s.escaped != 0};
    return true;
}
struct PatternData {
    std::vector<std::uint64_t> words;
    std::vector<std::vector<std::uint64_t>> enumWords;
    std::vector<PatternEnumName> enums;
    std::vector<PatternName> names;
};
bool node(const kirima_pattern_node& s, PatternValue& out, PatternData& data,
          std::span<const PatternValue> nodes) {
    if (s.kind > KIRIMA_PATTERN_HANDLE || !abi::valid(s.text) || !name(s.tag, out.tag) ||
        s.child_first > nodes.size() || s.child_count > nodes.size() - s.child_first ||
        s.key_first > nodes.size() || s.key_count > nodes.size() - s.key_first ||
        (s.name_count && !s.names) || (s.enum_count && !s.enumerators))
        return false;
    out.kind = static_cast<PatternValue::Kind>(s.kind);
    out.children = nodes.subspan(s.child_first, s.child_count);
    out.keys = nodes.subspan(s.key_first, s.key_count);
    out.real = s.real;
    out.ordinal = s.ordinal;
    out.text = abi::view(s.text);
    if ((s.kind == KIRIMA_PATTERN_INTEGER || s.kind == KIRIMA_PATTERN_ENUM) &&
        !integral(s.integer, out.integral, data.words))
        return false;
    data.names.resize(s.name_count);
    for (U32 i = 0; i < s.name_count; ++i)
        if (!name(s.names[i], data.names[i])) return false;
    out.names = data.names;
    data.enums.resize(s.enum_count);
    data.enumWords.resize(s.enum_count);
    for (U32 i = 0; i < s.enum_count; ++i) {
        const auto& e = s.enumerators[i];
        if (!integral(e.integer, data.enums[i].value, data.enumWords[i]) ||
            !name(e.name, data.enums[i].name))
            return false;
    }
    out.enumerators = data.enums;
    return true;
}
U32 pattern(const kirima_pattern_node* src, U32 count, U32 root, U32 positional,
            kirima_byte_sink output, void* frame, U32* result) {
    if (!src || root >= count || positional > 1 || !output || !result) return invalid;
    std::vector<PatternValue> nodes(count);
    std::vector<PatternData> data(count);
    for (U32 i = 0; i < count; ++i)
        if (!node(src[i], nodes[i], data[i], nodes)) return invalid;
    const auto r = formatPattern(nodes[root], positional != 0);
    *result = static_cast<U32>(r.error);
    if (r) output(frame, abi::bytesOf(r.text));
    return ok;
}
U32 asString(kirima_bytes b, kirima_byte_sink sink, void* f) {
    if (!abi::valid(b) || !sink) return invalid;
    sink(f, abi::bytesOf(formattedString(std::string(abi::view(b)))));
    return ok;
}
U32 packedScan(kirima_value_in v, kirima_byte_sink sink, void* frame, U32* known) {
    if (!abi::input(v) || !sink || !known) return invalid;
    const auto value = packedScanFormat(v.words, v.width);
    *known = value.has_value();
    if (value) sink(frame, abi::bytesOf(*value));
    return ok;
}
U32 timeError(const kirima_format_context* c, kirima_byte_sink sink, void* frame) {
    if (!c || !sink || !abi::valid(c->suffix)) return invalid;
    sink(frame, abi::bytesOf(timeFormatError(timeSettings(*c))));
    return ok;
}
U32 distribution(kirima_bytes b, U32* out) {
    if (!out || !abi::valid(b)) return invalid;
    const auto d = distributionOf(abi::view(b));
    if (!d) return invalid;
    *out = static_cast<U32>(*d);
    return ok;
}
U32 argument(U32 bad, kirima_byte_sink sink, void* frame) {
    if (!sink || bad > KIRIMA_DIST_STAGE) return invalid;
    sink(frame, abi::bytesOf(argumentName(static_cast<DistArgument>(bad))));
    return ok;
}
}  // namespace
const void* textService() noexcept {
    static constexpr kirima_text_api table{Boundary<parse>::call,
                                           Boundary<scalar>::call,
                                           Boundary<pattern>::call,
                                           Boundary<asString>::call,
                                           Boundary<packedScan>::call,
                                           Boundary<timeError>::call,
                                           distribution,
                                           argument};
    return &table;
}
}  // namespace kirimasim::kernel
