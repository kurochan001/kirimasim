/*
:name: comb_sensitivity_longest_static_prefix
:description: always_comb uses the longest static prefix and excludes its block and function locals
:tags: proc
:type: simulation
:lrm: 1800-2023 9.2.2.2.1 11.5.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:issue: 64
:top_module: top
:expected_stdout: @comb_sensitivity_longest_static_prefix.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  logic [7:0] memory [0:3] = '{10, 20, 30, 40};
  logic [7:0] fixed_words [0:1] = '{1, 2};
  logic [7:0] vector = 0;
  int index = 1, runs = 0;
  logic [7:0] memory_value, function_value, local_value;
  logic selected_bit;

  task count_run();
    runs++;
  endtask

  function logic [7:0] read_word();
    bit [7:0] local_word;
    return fixed_words[1] + local_word;
  endfunction

  always_comb begin : comb
    bit [7:0] local_word;
    count_run();
    memory_value = memory[index];
    selected_bit = vector[2];
    function_value = read_word();
    local_value = local_word;
  end

  task automatic show(string label);
    #1 $display("%s runs=%0d values=%0d,%0d,%0d,%0d", label, runs,
                memory_value, selected_bit, function_value, local_value);
  endtask

  initial begin
    show("start");
    memory[3] = 41;
    show("dynamic other element");
    vector[0] = 1;
    show("other packed bit");
    vector[2] = 1;
    show("selected packed bit");
    fixed_words[0] = 9;
    show("other fixed element");
    fixed_words[1] = 4;
    show("selected fixed element");
    comb.local_word = 9;
    show("block local");
    read_word.local_word = 6;
    show("function local");
    index = 2;
    show("selector");
  end
endmodule
