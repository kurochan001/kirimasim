// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/io_abi.h"

#include <algorithm>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abi_boundary.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/scalar_value.h"
#include "kirimasim/kernel/wide.h"
#include "value_services.h"

namespace kirimasim::kernel {
namespace {

bool valid(kirima_bytes bytes) {
    return (bytes.data || bytes.size == 0) && bytes.size <= std::string{}.max_size();
}
std::string_view view(kirima_bytes bytes) {
    return {bytes.data ? bytes.data : "", static_cast<std::size_t>(bytes.size)};
}
kirima_bytes bytesOf(std::string_view text) { return {text.data(), text.size()}; }

std::uint32_t openMcd(kirima_context* c, kirima_bytes path, kirima_io_result* out) {
    if (!c || !out || !valid(path) || view(path).find('\0') != std::string_view::npos)
        return KIRIMA_VALUE_INVALID;
    *out = {fromC(c).files().openMcd(std::string(view(path))), 0, 0};
    return KIRIMA_VALUE_OK;
}
std::uint32_t openFd(kirima_context* c, kirima_bytes path, kirima_bytes mode,
                     kirima_io_result* out) {
    if (!c || !out || !valid(path) || !valid(mode) ||
        view(path).find('\0') != std::string_view::npos)
        return KIRIMA_VALUE_INVALID;
    bool bad = false;
    const auto fd = fromC(c).files().openFd(std::string(view(path)), view(mode), bad);
    *out = {fd, 0, bad};
    return KIRIMA_VALUE_OK;
}
std::uint32_t capture(kirima_context* c, W4 descriptor, kirima_file_targets* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    const auto t = fromC(c).files().targets(descriptor);
    if (t.files.size() > KIRIMA_MCD_FILES) return KIRIMA_VALUE_RESOURCE;
    *out = {};
    out->owner = c;
    out->flags = (t.out ? KIRIMA_FILE_OUT : 0U) | (t.err ? KIRIMA_FILE_ERR : 0U) |
                 (t.notOpen ? KIRIMA_FILE_NOT_OPEN : 0U);
    for (const auto& [row, serial] : t.files)
        out->files[out->count++] = {row, 0, serial};
    return KIRIMA_VALUE_OK;
}
bool restore(kirima_context* c, const kirima_file_targets* src, FileTargets& t) {
    if (!c || !src || src->owner != c || src->count > KIRIMA_MCD_FILES || (src->flags & ~7U))
        return false;
    t.out = (src->flags & KIRIMA_FILE_OUT) != 0;
    t.err = (src->flags & KIRIMA_FILE_ERR) != 0;
    t.notOpen = (src->flags & KIRIMA_FILE_NOT_OPEN) != 0;
    for (const auto& f : std::span(src->files, src->count)) {
        if (f.reserved || !f.serial || (!t.files.empty() && t.files.back().first >= f.row))
            return false;
        t.files.emplace_back(f.row, f.serial);
    }
    return true;
}
std::uint32_t write(kirima_context* c, const kirima_file_targets* target, kirima_bytes text) {
    FileTargets t;
    if (!valid(text) || !restore(c, target, t)) return KIRIMA_VALUE_INVALID;
    fromC(c).files().write(t, view(text));
    return KIRIMA_VALUE_OK;
}
std::uint32_t live(kirima_context* c, const kirima_file_targets* target, std::uint32_t* out) {
    FileTargets t;
    if (!out || !restore(c, target, t)) return KIRIMA_VALUE_INVALID;
    *out = fromC(c).files().live(t);
    return KIRIMA_VALUE_OK;
}
template <auto Member> std::uint32_t fileAction(kirima_context* c, W4 fd, kirima_io_result* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    const bool ok = (fromC(c).files().*Member)(fd);
    *out = {ok, !ok, 0};
    return KIRIMA_VALUE_OK;
}
std::uint32_t flushAll(kirima_context* c) {
    if (!c) return KIRIMA_VALUE_INVALID;
    fromC(c).files().flushAll();
    return KIRIMA_VALUE_OK;
}
template <auto Member> std::uint32_t fileQuery(kirima_context* c, W4 fd, kirima_io_result* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    bool closed = false;
    const auto value = (fromC(c).files().*Member)(fd, closed);
    *out = {value, closed, 0};
    return KIRIMA_VALUE_OK;
}
std::uint32_t ungetChar(kirima_context* c, W4 fd, std::int32_t byte, kirima_io_result* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    bool closed = false;
    const auto value = fromC(c).files().ungetChar(byte, fd, closed);
    *out = {value, closed, 0};
    return KIRIMA_VALUE_OK;
}
template <auto Member>
std::uint32_t readData(kirima_context* c, W4 fd, std::uint64_t max, kirima_byte_sink sink,
                       void* user, kirima_io_result* out) {
    if (!c || !sink || !out || max > std::string{}.max_size()) return KIRIMA_VALUE_INVALID;
    bool closed = false;
    std::string bytes;
    (fromC(c).files().*Member)(fd, static_cast<std::size_t>(max), bytes, closed);
    *out = {static_cast<std::int64_t>(bytes.size()), closed, 0};
    sink(user, bytesOf(bytes));
    return KIRIMA_VALUE_OK;
}
std::uint32_t seek(kirima_context* c, W4 fd, std::int64_t offset, std::int64_t origin,
                   kirima_io_result* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    bool closed = false;
    const auto value = fromC(c).files().seek(fd, offset, origin, closed);
    *out = {value, closed, 0};
    return KIRIMA_VALUE_OK;
}
std::uint32_t error(kirima_context* c, W4 fd, kirima_byte_sink sink, void* user,
                    kirima_io_result* out) {
    if (!c || !sink || !out) return KIRIMA_VALUE_INVALID;
    bool closed = false;
    std::string text;
    const int code = fromC(c).files().error(fd, text, &closed);
    *out = {code, closed, 0};
    sink(user, bytesOf(text));
    return KIRIMA_VALUE_OK;
}

ScanTarget targetKind(kirima_scan_target t) {
    switch (t.domain) {
        case KIRIMA_SCAN_TWO:
        case KIRIMA_SCAN_FOUR:
            if (t.width) return ScanTarget::Integral;
            break;
        case KIRIMA_SCAN_REAL:
        case KIRIMA_SCAN_SHORTREAL:
            if (t.width == (t.domain == KIRIMA_SCAN_REAL ? 64U : 32U)) return ScanTarget::Real;
            break;
        case KIRIMA_SCAN_STRING:
            if (!t.width) return ScanTarget::String;
            break;
        default: break;
    }
    return ScanTarget::Other;
}
template <class Read>
std::uint32_t scan(kirima_context* c, const kirima_scan_args* args, kirima_scan_result* out,
                   Read read) {
    if (!c || !out || !args || !valid(args->format) || (!args->targets && args->count) ||
        (!args->assign && args->count) || !args->diagnostic)
        return KIRIMA_VALUE_INVALID;
    std::vector<ScanTarget> kinds;
    std::vector<std::uint32_t> widths;
    for (const auto t : std::span(args->targets, args->count)) {
        const auto kind = targetKind(t);
        if (kind == ScanTarget::Other) return KIRIMA_VALUE_INVALID;
        kinds.push_back(kind);
        widths.push_back(t.width);
    }
    const ScanPlan plan = prepareScanFormat(view(args->format), kinds);
    *out = {0, 0, plan.missingTargets, !plan.error.empty()};
    if (!plan.error.empty()) {
        args->diagnostic(args->user, bytesOf(plan.error));
        return KIRIMA_VALUE_OK;
    }
    std::vector<ScanValue> values;
    bool closed = false;
    out->assigned = read(plan.format, values, closed, widths);
    out->not_open = closed;
    std::vector<W4> cells;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto target = args->targets[i];
        const auto& v = values[i];
        cells.resize(kinds[i] == ScanTarget::Integral ? wordsFor(target.width) : 1);
        switch (kinds[i]) {
            case ScanTarget::Integral:
                assignScannedIntegral(cells.data(), target.width, target.domain == KIRIMA_SCAN_FOUR,
                                      v);
                break;
            case ScanTarget::Real:
                cells[0] = writeReal(v.real, target.domain == KIRIMA_SCAN_SHORTREAL);
                break;
            case ScanTarget::String: cells[0] = {fromC(c).strings().internRead(v.text), 0}; break;
            case ScanTarget::Other: return KIRIMA_VALUE_INVALID;
        }
        args->assign(args->user, static_cast<std::uint32_t>(i), cells.data(),
                     static_cast<std::uint32_t>(cells.size()));
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t scanFile(kirima_context* c, W4 fd, const kirima_scan_args* args,
                       kirima_scan_result* out) {
    return scan(c, args, out, [&](const auto& fmt, auto& values, bool& closed, const auto& widths) {
        return fromC(c).files().scan(fd, fmt, values, closed, widths);
    });
}
std::uint32_t scanText(kirima_context* c, kirima_bytes text, const kirima_scan_args* args,
                       kirima_scan_result* out) {
    if (!valid(text)) return KIRIMA_VALUE_INVALID;
    return scan(c, args, out, [&](const auto& fmt, auto& values, bool&, const auto& widths) {
        return scanString(view(text), fmt, values, widths);
    });
}
std::uint32_t output(kirima_context* c, std::uint32_t kind, kirima_bytes text) {
    if (!c || !valid(text)) return KIRIMA_VALUE_INVALID;
    auto& out = fromC(c).output();
    switch (kind) {
        case KIRIMA_OUTPUT_WRITE: out.write(view(text)); break;
        case KIRIMA_OUTPUT_ERROR: out.writeError(view(text)); break;
        case KIRIMA_OUTPUT_MESSAGE: out.message(view(text)); break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t flushOutput(kirima_context* c) {
    if (!c) return KIRIMA_VALUE_INVALID;
    fromC(c).output().flush();
    return KIRIMA_VALUE_OK;
}
std::uint32_t atLineStart(kirima_context* c, std::uint32_t* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    *out = fromC(c).output().atLineStart();
    return KIRIMA_VALUE_OK;
}
constexpr kirima_io_api api{Boundary<openMcd>::call,
                            Boundary<openFd>::call,
                            Boundary<capture>::call,
                            Boundary<write>::call,
                            Boundary<live>::call,
                            Boundary<fileAction<&Files::close>>::call,
                            Boundary<fileAction<&Files::flush>>::call,
                            Boundary<flushAll>::call,
                            Boundary<fileQuery<&Files::getChar>>::call,
                            Boundary<ungetChar>::call,
                            Boundary<readData<&Files::readLine>>::call,
                            Boundary<readData<&Files::readBytes>>::call,
                            Boundary<fileQuery<&Files::tell>>::call,
                            Boundary<seek>::call,
                            Boundary<fileQuery<&Files::atEnd>>::call,
                            Boundary<error>::call,
                            Boundary<scanFile>::call,
                            Boundary<scanText>::call,
                            Boundary<output>::call,
                            Boundary<flushOutput>::call,
                            Boundary<atLineStart>::call};
}  // namespace
const void* queryIoService(std::uint32_t id, std::uint32_t version) noexcept {
    return id == KIRIMA_SERVICE_IO && version == KIRIMA_IO_VERSION ? &api : nullptr;
}
}  // namespace kirimasim::kernel
