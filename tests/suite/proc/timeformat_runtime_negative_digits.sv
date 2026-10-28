/*
:name: timeformat_runtime_negative_digits
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
:expected_stderr: timeformat_runtime_negative_digits.sv:23: error: $timeformat precision must be nonnegative [top @ 0ns]
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  int digits = -1;
  final $display("final ran");
  initial begin
    $timeformat(-9, digits, "ns", 10);
    $display("not reached");
  end
endmodule
