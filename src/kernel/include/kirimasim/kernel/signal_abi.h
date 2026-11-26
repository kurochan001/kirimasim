// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/event_abi.h"
#include "kirimasim/kernel/scalar_task_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIRIMA_SERVICE_SIGNAL 10u
#define KIRIMA_SIGNAL_VERSION 1u
#define KIRIMA_SIGNAL_COUNTED_VERSION 2u
enum kirima_hold_level { KIRIMA_HOLD_ASSIGN, KIRIMA_HOLD_FORCE };
typedef struct kirima_slot_bits {
    uint32_t slot, element, lsb, width;
} kirima_slot_bits;
typedef struct kirima_domain_value {
    const uint64_t* words;
    uint32_t count;
} kirima_domain_value;
typedef struct kirima_signal_driver {
    const kirima_context* owner;
    uint32_t row, reserved;
} kirima_signal_driver;
typedef struct kirima_side_update {
    kirima_slot_bits at;
    uint32_t value;
} kirima_side_update;
typedef struct kirima_clocking_input {
    kirima_slot_bits source, target;
} kirima_clocking_input;
typedef struct kirima_sample_history {
    kirima_wait_item clock;
    uint32_t previous;
} kirima_sample_history;
typedef uint32_t (*kirima_pending_visitor)(void*, uint32_t slot, const uint64_t* words,
                                           uint64_t count);
/* Word spans start at bit zero of the selected range. Four-state words are
 * interleaved a/b. A zero width selects a whole element. Output bit zero is
 * the selected lsb. Pending visitor data is borrowed only during the call;
 * returning zero stops publication. Return VALUE_OK, INVALID or RESOURCE. */
typedef struct kirima_signal_api {
    uint32_t (*write)(kirima_context*, kirima_slot_bits, kirima_domain_value, uint32_t* changed);
    uint32_t (*mark_held)(kirima_context*, uint32_t slot, uint32_t net, uint32_t driven,
                          uint32_t strength);
    uint32_t (*note_driven)(kirima_context*, uint32_t slot);
    uint32_t (*hold)(kirima_context*, uint32_t level, kirima_process, kirima_slot_part);
    uint32_t (*write_held)(kirima_context*, uint32_t level, kirima_process, kirima_slot_bits,
                           kirima_domain_value);
    uint32_t (*release)(kirima_context*, uint32_t level, kirima_slot_part);
    uint32_t (*holds_any)(kirima_context*, uint32_t level, kirima_process, uint32_t*);
    uint32_t (*add_driver)(kirima_context*, kirima_signal_driver*);
    uint32_t (*drive_delayed)(kirima_context*, kirima_signal_driver, kirima_slot_bits,
                              kirima_domain_value, uint64_t rise, uint64_t fall, uint64_t off,
                              const kirima_side_update*, uint32_t count);
    uint32_t (*nba)(kirima_context*, kirima_process, kirima_slot_bits, kirima_domain_value,
                    uint64_t ticks);
    uint32_t (*nba_string_char)(kirima_context*, kirima_process, kirima_slot_bits, kirima_index,
                                uint32_t byte, uint64_t ticks);
    uint32_t (*sampled)(kirima_context*, kirima_slot_bits source, uint32_t current,
                        kirima_domain_value initial, const kirima_sample_history*);
    uint32_t (*clocking)(kirima_context*, kirima_wait_item clock, kirima_event_value,
                         const kirima_clocking_input*, uint32_t count);
    uint32_t (*visit_pending)(kirima_context*, kirima_pending_visitor, void*, uint32_t* completed);
} kirima_signal_api;

enum kirima_counted_slot_status {
    KIRIMA_COUNTED_SLOT_OK,
    KIRIMA_COUNTED_SLOT_DECLINED,
    KIRIMA_COUNTED_SLOT_INVALID
};
typedef struct kirima_counted_slot {
    const kirima_context* owner;
    uint64_t serial;
} kirima_counted_slot;

typedef struct kirima_signal_api_v2 {
    kirima_signal_api v1;
    uint32_t (*prepare_counted_slot)(kirima_context*, uint32_t process_row, uint32_t slot,
                                     kirima_counted_slot*, kirima_w4* initial);
    uint32_t (*commit_counted_slot)(kirima_context*, kirima_counted_slot, kirima_w4 final_value,
                                    uint64_t writes, uint64_t changes);
    // Null during ordinary execution. The checker receives each logical write.
    uint32_t (*check_counted_write)(kirima_context*, kirima_counted_slot, kirima_w4 value);
} kirima_signal_api_v2;
#ifdef __cplusplus
}
#endif
