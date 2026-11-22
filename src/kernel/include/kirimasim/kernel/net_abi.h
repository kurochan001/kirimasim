// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/signal_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_NET 13u
#define KIRIMA_NET_VERSION 1u
enum kirima_net_status {
    KIRIMA_NET_OK,
    KIRIMA_NET_TARGET,
    KIRIMA_NET_SHAPE,
    KIRIMA_NET_VALUE,
    KIRIMA_NET_RESOURCE,
    KIRIMA_NET_REENTRANT,
    KIRIMA_NET_HOLD,
    KIRIMA_NET_INTERRUPTED
};
enum kirima_net_kind {
    KIRIMA_WIRE,
    KIRIMA_TRI,
    KIRIMA_WAND,
    KIRIMA_TRIAND,
    KIRIMA_WOR,
    KIRIMA_TRIOR,
    KIRIMA_TRI0,
    KIRIMA_TRI1,
    KIRIMA_SUPPLY0,
    KIRIMA_SUPPLY1,
    KIRIMA_UWIRE
};
enum kirima_transfer { KIRIMA_IDENTITY, KIRIMA_NONRESISTIVE, KIRIMA_RESISTIVE };
enum kirima_conduction { KIRIMA_SWITCH_OFF, KIRIMA_SWITCH_ON, KIRIMA_SWITCH_UNCERTAIN };
enum kirima_path_enable { KIRIMA_PATH_ALWAYS, KIRIMA_PATH_CONDITION, KIRIMA_PATH_IFNONE };
enum kirima_net_write { KIRIMA_NET_DEPOSIT, KIRIMA_NET_FORCE, KIRIMA_NET_RELEASE };
typedef struct kirima_strength {
    int8_t low, high;
} kirima_strength;
typedef struct kirima_net_node {
    kirima_slot_bits target;
    uint32_t kind, component;
} kirima_net_node;
typedef struct kirima_net_origin {
    uint32_t node;
    kirima_strength initial;
} kirima_net_origin;
typedef struct kirima_net_edge {
    uint32_t first, second, transfer, initial;
} kirima_net_edge;
typedef struct kirima_net_force {
    uint32_t node, force;
    kirima_w4 value;
} kirima_net_force;
typedef struct kirima_publication {
    uint32_t committed, changed;
} kirima_publication;
typedef struct kirima_path {
    kirima_slot_bits source;
    uint32_t origin, edge, enable, condition, group, source_identity, destination_identity;
    uint32_t overflow_mask;
    uint64_t ticks[12];
} kirima_path;
typedef struct kirima_path_evaluator {
    uint32_t (*read)(void*, uint32_t, kirima_w4*);
    void* frame;
    uint32_t count;
} kirima_path_evaluator;
typedef struct kirima_udp_row {
    kirima_bytes inputs;
    uint32_t state, output;
} kirima_udp_row;
typedef uint32_t (*kirima_udp_reader)(void*, uint32_t port);
typedef struct kirima_udp_source {
    kirima_udp_reader read;
    void* frame;
} kirima_udp_source;
/* Source readers return 0,1,X=2 or Z=3. Their inputs stay stable during the
 * call. Path evaluators stay live until detached. The kernel copies all
 * static rows, ranges and delays. A zero overflow-mask bit means a valid tick. */
typedef struct kirima_net_api {
    uint32_t (*enable)(kirima_context*, uint32_t slot, const kirima_strength*, uint32_t);
    uint32_t (*configure)(kirima_context*, const kirima_net_node*, uint32_t,
                          const kirima_net_origin*, uint32_t, const kirima_net_edge*, uint32_t);
    uint32_t (*start)(kirima_context*);
    uint32_t (*drive)(kirima_context*, uint32_t origin, kirima_strength);
    uint32_t (*drive_delayed)(kirima_context*, uint32_t, kirima_strength, uint64_t rise,
                              uint64_t fall, uint64_t off);
    uint32_t (*edge)(kirima_context*, uint32_t, uint32_t value);
    uint32_t (*edge_delayed)(kirima_context*, uint32_t, uint32_t value, uint64_t on, uint64_t off);
    uint32_t (*forces)(kirima_context*, const kirima_net_force*, uint32_t, kirima_publication*);
    uint32_t (*write)(kirima_context*, kirima_slot_bits, kirima_words_in, uint32_t kind,
                      kirima_publication*);
    uint32_t (*read)(kirima_context*, kirima_slot_bits, kirima_index elem, kirima_index pos,
                     uint32_t width, kirima_words_out);
    uint32_t (*wait)(kirima_context*, kirima_process, const kirima_wait_item*, uint32_t,
                     const kirima_slot_part*, uint32_t);
    uint32_t (*track_display)(kirima_context*, kirima_slot_bits, uint32_t* tracked);
    uint32_t (*paths)(kirima_context*, const kirima_path*, uint32_t, kirima_path_evaluator*);
    uint32_t (*detach_paths)(kirima_context*, kirima_path_evaluator*);
    uint32_t (*udp_compile)(kirima_context*, uint32_t inputs, const kirima_udp_row*, uint32_t,
                            kirima_resource*);
    uint32_t (*udp_output)(kirima_context*, kirima_resource, kirima_udp_source, uint32_t*);
    uint32_t (*udp_step)(kirima_context*, kirima_resource, uint32_t state,
                         kirima_udp_source previous, kirima_udp_source current, uint32_t*);
    uint32_t (*udp_release)(kirima_context*, kirima_resource);
} kirima_net_api;
#ifdef __cplusplus
}
#endif
