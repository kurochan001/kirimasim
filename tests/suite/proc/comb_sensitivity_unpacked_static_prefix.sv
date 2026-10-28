/*
:name: comb_sensitivity_unpacked_static_prefix
:description: constant unpacked array prefixes retain only their sub-array before the first dynamic index
:tags: proc array
:type: simulation
:lrm: 1800-2023 9.2.2.2.1 9.2.2.3 11.5.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:issue: 64
:top_module: top
:expected_stdout: @comb_sensitivity_unpacked_static_prefix.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  logic [7:0] rows [3:2][-1:1] = '{'{10, 11, 12}, '{20, 21, 22}};
  logic [7:0] cubes [-1:0][2:1][4:5] = '{
    '{'{10, 11}, '{20, 21}}, '{'{30, 31}, '{40, 41}}
  };
  int row_index = 0, cube_index = 4, outer_index = -1;
  int row_runs = 0, cube_runs = 0, root_runs = 0;
  logic [7:0] row_value, cube_value, root_value, initial_copy;

  function automatic logic [7:0] read_row();
    return rows[2][row_index];
  endfunction

  // The function is first lowered for a caller without implicit sensitivity.
  initial initial_copy = read_row();

  always_comb begin
    row_runs++;
    row_value = read_row();
  end

  always_latch begin
    cube_runs++;
    cube_value = cubes[-1][1][cube_index];
  end

  always_comb begin
    root_runs++;
    root_value = cubes[outer_index][1][4];
  end

  task automatic show(string label);
    #1 $display("%s runs=%0d,%0d,%0d values=%0d,%0d,%0d", label,
                row_runs, cube_runs, root_runs, row_value, cube_value, root_value);
  endtask

  initial begin
    show("start");
    rows[3][0] = 99;
    show("outside row prefix");
    rows[2][-1] = 23;
    show("inside row prefix low");
    rows[2][1] = 24;
    show("inside row prefix high");
    row_index = -1;
    show("row selector");
    cubes[0][1][4] = 42;
    show("outside first cube prefix");
    cubes[-1][2][4] = 12;
    show("outside second cube prefix");
    cubes[-1][1][5] = 25;
    show("inside cube prefix");
    cube_index = 5;
    show("cube selector");
    outer_index = 0;
    show("root selector");
    cubes[0][2][5] = 99;
    show("constant suffix after dynamic index");
  end
endmodule
