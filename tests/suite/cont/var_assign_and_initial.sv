/*
:name: var_assign_and_initial
:description: a variable driven by a continuous assignment is also written by an initial procedure
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 10.3.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a variable driven by a continuous assignment is also written by an initial procedure
:expected_error: 17:11: error: cannot mix continuous and procedural assignments to variable 'v'
*/
module top;
  logic a, v;
  assign v = a;
  initial v = 1'b0;
endmodule
