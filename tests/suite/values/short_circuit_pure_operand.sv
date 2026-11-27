/*
:name: short_circuit_pure_operand
:description: A skipped pure system function skips its effectful argument
:tags: values
:type: simulation
:lrm: 1800-2023 11.3.5 11.4.7
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:generator: gen_short_circuit_matrix.py@1
:expected_stdout: @short_circuit_pure_operand.expected
*/
module top;
  bit lo, hi, result;
  int n;
  initial begin
    lo = 0;
    hi = 1;
    n = 4;
    result = lo && ($clog2(n++) != 0);
    $display("and_skip result=%0d n=%0d", result, n);
    n = 4;
    result = hi && ($clog2(n++) != 0);
    $display("and_take result=%0d n=%0d", result, n);
    n = 4;
    result = hi || ($clog2(n++) != 0);
    $display("or_skip result=%0d n=%0d", result, n);
    n = 4;
    result = lo || ($clog2(n++) != 0);
    $display("or_take result=%0d n=%0d", result, n);
  end
endmodule
