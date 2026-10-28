/*
:name: timeformat_runtime_unit_above
:description: an invalid nonconstant timeformat argument ends with a positioned design error and runs final
:tags: proc
:type: simulation
:lrm: 1800-2023 20.4.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:top_module: top
:expected_stdout: final ran
:expected_exit: 2
:expected_stderr: timeformat_runtime_unit_above.sv:23: error: $timeformat unit must be between -15 and 2 [top @ 0ns]
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  int unit_value = 3;
  final $display("final ran");
  initial begin
    $timeformat(unit_value, 2, "ns", 10);
    $display("not reached");
  end
endmodule
