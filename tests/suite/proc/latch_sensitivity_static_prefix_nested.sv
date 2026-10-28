/*
:name: latch_sensitivity_static_prefix_nested
:description: nested static selections narrow always_latch sensitivity while dynamic selections retain their static parent
:tags: proc
:type: simulation
:lrm: 1800-2023 9.2.2.2.1 9.2.2.3 11.5.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:issue: 64
:top_module: top
:expected_stdout: @latch_sensitivity_static_prefix_nested.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  typedef struct packed {
    logic [3:0] upper;
    logic [3:0] lower;
  } pair_t;
  pair_t packet = '0;
  logic [0:7] ascending = 0;
  logic [1:0][3:0] packed_words = 0;
  logic [7:0] fixed_words [0:1] = '{0, 0};
  logic [7:0] dynamic_words [0:1] = '{0, 0};
  int index = 0, bit_index = 0, runs = 0;
  logic [5:0] observed;

  always_latch begin : latch_block
    bit local_bit;
    runs++;
    observed = {packet.upper[2], |ascending[2:3],
                packed_words[1][bit_index], fixed_words[1][3],
                dynamic_words[index][3], local_bit};
  end

  task automatic show(string label);
    #1 $display("%s runs=%0d", label, runs);
  endtask

  initial begin
    show("start");
    packet.lower[0] = 1;
    show("other member");
    packet.upper[0] = 1;
    show("other member bit");
    packet.upper[2] = 1;
    show("selected member bit");
    ascending[7] = 1;
    show("outside range");
    ascending[1] = 1;
    show("other range bit");
    ascending[2] = 1;
    show("selected range bit");
    packed_words[0][0] = 1;
    show("other packed element");
    packed_words[1][1] = 1;
    show("dynamic parent bit");
    packed_words[1][0] = 1;
    show("dynamic selected bit");
    fixed_words[0][3] = 1;
    show("other unpacked element");
    fixed_words[1][0] = 1;
    show("other unpacked bit");
    fixed_words[1][3] = 1;
    show("selected unpacked bit");
    dynamic_words[1][0] = 1;
    show("dynamic ancestor");
    latch_block.local_bit = 1;
    show("block local");
    index = 1;
    show("unpacked selector");
    bit_index = 1;
    show("packed selector");
    $display("observed=%b", observed);
  end
endmodule
