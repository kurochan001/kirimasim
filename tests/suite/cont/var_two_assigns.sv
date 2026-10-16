/*
:name: var_two_assigns
:description: a variable is driven by two continuous assignments
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 10.3.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a variable is driven by two continuous assignments
:expected_error: 17:10: error: cannot have multiple continuous assignments to variable 'v'
*/
module top;
  logic a, b, v;
  assign v = a;
  assign v = b;
endmodule
