/*
:name: var_assign_and_decl_init
:description: a variable driven by a continuous assignment has an initial value in its declaration
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 10.3.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a variable driven by a continuous assignment has an initial value in its declaration
:expected_error: 17:10: error: cannot mix continuous and procedural assignments to variable 'v'
*/
module top;
  logic a;
  logic v = 1'b1;
  assign v = a;
endmodule
