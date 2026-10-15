/*
:name: missing_semicolon
:description: A declaration without its semicolon is a syntax error
:tags: syntax error
:lrm: 1800-2023 A.2.1.3 6.8
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: the declaration of a on line 15 is not terminated by a semicolon
:type: parsing
:expected_error: 15:10: error: expected ';'
*/
module top;
  logic a
  logic b;
endmodule
