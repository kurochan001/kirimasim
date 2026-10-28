/*
:name: timeformat_unit_below_minimum
:description: a constant timeformat unit outside the allowed range is a positioned input error
:tags: proc
:type: elaboration
:lrm: 1800-2023 20.4.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: timeformat units must be between -15 and 2
:expected_error: 18:5: error: $timeformat unit must be between -15 and 2
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
module top;
  initial begin
    $timeformat(-16, 2, "ns", 10);
    $display("not reached");
  end
endmodule
