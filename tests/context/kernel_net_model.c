// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/net_abi.h"
#include <stddef.h>
static uint32_t port(void* frame, uint32_t at) { return ((uint32_t*)frame)[at]; }
int check_c_nets(const kirima_kernel_api* api, kirima_context* c, kirima_context* other,
                 void (*run)(kirima_context*)) {
    const kirima_net_api* n = api->query_service(KIRIMA_SERVICE_NET, KIRIMA_NET_VERSION);
    if (!n || api->query_service(KIRIMA_SERVICE_NET, 2)) return 0;
    const kirima_udp_row edge = {{"r?", 2}, '?', '1'}, level = {{"1b", 2}, 0, '1'};
    uint32_t before[] = {0, 0}, after[] = {1, 1}, result = 0;
    const kirima_udp_source prev = {port, before}, cur = {port, after};
    kirima_resource seq, comb;
    if (n->udp_compile(c, 2, &edge, 1, &seq) || n->udp_compile(c, 2, &level, 1, &comb) ||
        n->udp_output(c, comb, cur, &result) || result != 1 ||
        n->udp_output(other, comb, cur, &result) != KIRIMA_NET_TARGET)
        return 0;
    after[1] = 0;
    if (n->udp_step(c, seq, 0, prev, cur, &result) || result != 1) return 0;
    after[0] = 3;
    if (n->udp_output(c, comb, cur, &result) || result != 2 || n->udp_release(c, comb) ||
        n->udp_release(c, seq) || n->udp_output(c, comb, cur, &result) != KIRIMA_NET_TARGET)
        return 0;
    const kirima_udp_row malformed = {{"(01X", 4}, '?', '1'};
    if (n->udp_compile(c, 1, &malformed, 1, &seq) != KIRIMA_NET_VALUE) return 0;
    const kirima_strength z = {0, 0};
    const kirima_slot_bits bit = {0, 0, 0, 1};
    const kirima_net_node node = {bit, KIRIMA_WIRE, 0};
    const kirima_net_origin origin = {0, {-6, -6}};
    if (n->enable(c, 0, &z, 1) || n->configure(c, &node, 1, &origin, 1, NULL, 0) ||
        n->paths(c, NULL, 0, NULL) != KIRIMA_NET_SHAPE || n->start(c))
        return 0;
    run(c);
    kirima_w4 interval = {0, 0};
    const kirima_index known = {0, 1, 0};
    const kirima_words_out sample = {&interval, 1};
    if (n->read(c, bit, known, known, 1, sample) || interval.a != 250 || interval.b != 250)
        return 0;
    if (n->drive_delayed(c, 0, (kirima_strength){4, 5}, 3, 3, 3)) return 0;
    run(c);
    if (api->time(c) != 3 || n->read(c, bit, known, known, 1, sample) || interval.a != 4 ||
        interval.b != 5)
        return 0;
    kirima_w4 value = {0, 0};
    kirima_publication changed = {0};
    if (n->write(c, bit, (kirima_words_in){&value, 1}, KIRIMA_NET_FORCE, &changed) ||
        !changed.committed || n->read(c, bit, known, known, 1, sample) || interval.a != 250 ||
        interval.b != 250 ||
        n->write(c, bit, (kirima_words_in){NULL, 0}, KIRIMA_NET_RELEASE, &changed) ||
        n->read(c, bit, known, known, 1, sample) || interval.a != 4 || interval.b != 5)
        return 0;
    return n->read(c, bit, (kirima_index){0, 0, 0}, known, 1, sample) == 0 && interval.a == 250 &&
           interval.b == 6;
}
