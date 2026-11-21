// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <stddef.h>
#include <string.h>

#include "kernel_abi_model.h"
#include "kirimasim/kernel/io_abi.h"

_Static_assert(sizeof(kirima_file_row) == 16 && offsetof(kirima_file_row, serial) == 8,
               "file opening identity");
_Static_assert(sizeof(kirima_file_targets) == 496 && offsetof(kirima_file_targets, files) == 16,
               "snapshot layout");
_Static_assert(sizeof(kirima_scan_target) == 8 && sizeof(kirima_io_result) == 16,
               "scan and file result layouts");

static kirima_bytes text(const char* s) { return (kirima_bytes){s, strlen(s)}; }
struct sink {
    kirima_w4 words[3][3];
    uint32_t counts[3];
    char bytes[32];
    uint64_t size;
};
static void receive(void* user, kirima_bytes data) {
    struct sink* sink = user;
    sink->size = data.size;
    if (data.size && data.size <= sizeof(sink->bytes))
        memcpy(sink->bytes, data.data, (size_t)data.size);
}
static void assign(void* user, uint32_t i, const kirima_w4* words, uint32_t n) {
    struct sink* sink = user;
    if (i < 3 && n <= 3) {
        sink->counts[i] = n;
        memcpy(sink->words[i], words, sizeof(*words) * n);
    }
}

/* The native host owns the contexts and assigns an isolated temporary path. */
int check_c_io_model(const kirima_kernel_api* table, kirima_context* c, kirima_context* other,
                     const char* path, uint64_t* string_handle) {
    const kirima_io_api* io = table->query_service(KIRIMA_SERVICE_IO, KIRIMA_IO_VERSION);
    if (!io || table->query_service(KIRIMA_SERVICE_IO, 0)) return 0;
    struct sink sink = {0};
    kirima_io_result r = {0};
    kirima_scan_target targets[] = {
        {KIRIMA_SCAN_FOUR, 129}, {KIRIMA_SCAN_TWO, 65}, {KIRIMA_SCAN_STRING, 0}};
    kirima_scan_args args = {text("%d %c %s"), targets, 3, assign, receive, &sink};
    kirima_scan_result result = {0};
    if (io->scan_string(c, text("-1 B abc"), &args, &result) || result.assigned != 3 ||
        sink.counts[0] != 3 || sink.words[0][2].a != 1 || sink.words[0][1].a != UINT64_MAX ||
        sink.words[1][0].a != 'B' || sink.words[1][1].a || !sink.words[2][0].a)
        return 0;
    *string_handle = sink.words[2][0].a;
    if (io->open_fd(c, text(path), text("invalid"), &r) || r.value != 0 || !r.bad_mode ||
        io->open_fd(c, text(path), text("w+b"), &r) || r.value <= 0 || r.bad_mode)
        return 0;
    kirima_w4 fd = {(uint64_t)r.value, 0};
    kirima_file_targets saved;
    uint32_t live = 0;
    if (io->capture(c, fd, &saved) || saved.count != 1 || io->live(c, &saved, &live) || !live ||
        io->live(other, &saved, &live) != KIRIMA_VALUE_INVALID ||
        io->write(c, &saved, text("27\nabc")) || io->flush(c, fd, &r) || !r.value ||
        io->tell(c, fd, &r) || r.value != 6 || io->seek(c, fd, 0, 0, &r) || r.value)
        return 0;
    args.format = text("%q");
    if (io->scan_file(c, fd, &args, &result) || !result.bad_format || !sink.size ||
        io->tell(c, fd, &r) || r.value)
        return 0;
    args.format = text("%d %s");
    args.count = 1;
    if (io->scan_file(c, fd, &args, &result) || result.assigned != 1 || !result.missing_targets ||
        sink.words[0][0].a != 27 || io->unget_char(c, fd, 'Q', &r) || r.value ||
        io->seek(c, fd, INT64_MIN, 1, &r) || r.value != -1 || io->get_char(c, fd, &r) ||
        r.value != 'Q')
        return 0;
    if (io->seek(c, fd, 0, 0, &r) || io->read_line(c, fd, 0, receive, &sink, &r) || r.value != 3 ||
        memcmp(sink.bytes, "27\n", 3) || io->read_bytes(c, fd, 8, receive, &sink, &r) ||
        r.value != 3 || memcmp(sink.bytes, "abc", 3) || io->at_end(c, fd, &r) || r.value != 1)
        return 0;
    if (io->close(c, fd, &r) || !r.value || io->open_fd(c, text(path), text("w"), &r) ||
        (uint64_t)r.value != fd.a || io->live(c, &saved, &live) || live ||
        io->write(c, &saved, text("stale")) || io->tell(c, fd, &r) || r.value ||
        io->close(c, fd, &r) || io->error(c, fd, receive, &sink, &r) || !r.not_open || !r.value ||
        !sink.size || io->get_char(c, fd, &r) || !r.not_open || r.value != -1)
        return 0;
    if (io->open_mcd(c, text(path), &r) || r.value != 2) return 0;
    kirima_w4 mcd = {(uint64_t)r.value | 1, 0};
    if (io->capture(c, mcd, &saved) || saved.count != 1 || saved.flags != KIRIMA_FILE_OUT ||
        io->write(c, &saved, text("x")) || io->flush_all(c) || io->close(c, mcd, &r) ||
        io->at_line_start(c, &live) || live || io->output(c, KIRIMA_OUTPUT_WRITE, text("\n")) ||
        io->output(c, KIRIMA_OUTPUT_ERROR, text("err")) ||
        io->output(c, KIRIMA_OUTPUT_MESSAGE, text("!")) || io->flush_output(c) ||
        io->at_line_start(c, &live) || !live)
        return 0;
    targets[0].width = 0;
    return io->scan_string(c, text("1"), &args, &result) == KIRIMA_VALUE_INVALID &&
           io->read_bytes(c, fd, 0, NULL, NULL, &r) == KIRIMA_VALUE_INVALID &&
           io->write(c, &saved, (kirima_bytes){NULL, 4}) == KIRIMA_VALUE_INVALID;
}
