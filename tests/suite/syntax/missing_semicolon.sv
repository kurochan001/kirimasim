/*
:name: missing_semicolon
:description: A declaration without its semicolon is a syntax error
:tags: syntax error
:lrm: 1800-2023 6.8
:oracle: spec
:fourstate: no
:nondeterministic: no
:should_fail_because: the declaration of a on line 14 is not terminated by a semicolon
:type: parsing
:expected_error: 14:10: error: expected ';'
*/
module top;
  logic a
  logic b;
endmodule
