/*
:name: comb_sensitivity_modport_expression
:description: A packed select of an expression modport retains sensitivity to every operand that can affect the selected value
:tags: proc hier interface
:type: simulation
:lrm: 1800-2023 9.2.2.2.1 11.5.3 25.5.4
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:issue: 64
:top_module: top
:expected_stdout: @comb_sensitivity_modport_expression.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
interface sum_bus;
  logic [3:0] a = 3, b = 0;
  modport view (input .sum(a + b));
endinterface

module reader(sum_bus.view bus);
  int runs = 0;
  logic observed;
  always_comb begin
    runs++;
    observed = bus.sum[2];
  end
endmodule

module top;
  sum_bus bus();
  reader worker(bus);

  task automatic show(string label);
    #1 $display("%s runs=%0d selected=%b", label, worker.runs, worker.observed);
  endtask

  initial begin
    show("start");
    bus.b = 1;
    show("carry into selected bit");
    bus.b = 0;
    show("remove carry");
    bus.b = 4;
    show("selected operand bit");
    bus.a = 4;
    show("first operand carry");
  end
endmodule
