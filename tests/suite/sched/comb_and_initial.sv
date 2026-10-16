/*
:name: comb_and_initial
:description: a variable written by always_comb is also written by an initial procedure
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 9.2.2.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a variable written by always_comb is also written by an initial procedure
:expected_error: 16:15: error: variable 'v' driven by always_comb procedure cannot be written to by any other process
*/
module top;
  logic a, v;
  always_comb v = a;
  initial v = 1'b0;
endmodule
