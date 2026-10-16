/*
:name: uwire_two_assigns
:description: a uwire net is driven by two continuous assignments
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 6.6.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a uwire net is driven by two continuous assignments
:expected_error: 18:10: error: 'uwire' net 'u' cannot have multiple drivers
*/
module top;
  logic a, b;
  uwire u;
  assign u = a;
  assign u = b;
endmodule
