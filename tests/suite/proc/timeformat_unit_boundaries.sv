/*
:name: timeformat_unit_boundaries
:description: both legal timeformat unit endpoints remain usable
:tags: proc
:type: simulation
:lrm: 1800-2023 20.4.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: 0fs|0s|0fs|0s
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
module top;
  int unit_value = -15;
  initial begin
    $timeformat(-15, 0, "fs", 0);
    $write("%0t|", 0);
    $timeformat(2, 0, "s", 0);
    $write("%0t|", 0);
    $timeformat(unit_value, 0, "fs", 0);
    $write("%0t|", 0);
    unit_value = 2;
    $timeformat(unit_value, 0, "s", 0);
    $display("%0t", 0);
  end
endmodule
