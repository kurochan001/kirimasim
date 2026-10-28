/*
:name: conditional_shortreal_context
:description: Conditional shortreal results round before conversion to an enclosing real context
:tags: values
:type: simulation
:lrm: 1800-2023 11.4.11 6.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @conditional_shortreal_context.expected
*/
module top;
  bit yes = 1, no = 0;
  logic ambiguous = 1'bx;
  int number = 16777217;
  shortreal value = 3.0, zero = 0.0, rounded = 16777216.0;
  shortreal negative_zero, nan_value;
  real result;
  int calls = 0;

  function automatic int next_value();
    calls++;
    return number;
  endfunction

  initial begin
    result = no ? value : number;
    $display("variable %.1f", result);
    result = yes ? number : value;
    $display("first %.1f", result);
    result = no ? value : 16777217;
    $display("literal %.1f", result);
    result = 0 ? shortreal'(0.0) : 16777217;
    $display("constant %.1f", result);
    result = yes ? real'(number) : value;
    $display("real %.1f", result);
    result = ambiguous ? number : rounded;
    $display("equal %.1f", result);
    result = ambiguous ? number : value;
    $display("unequal %.1f", result);
    result = yes ? ((zero + number) - 16777216) : zero;
    $display("intermediate %.1f", result);
    result = yes ? -number : value;
    $display("negative %.1f", result);
    result = yes ? (no ? value : number) : zero;
    $display("nested %.1f", result);
    result = (yes ? number : value) + real'(1);
    $display("enclosing %.1f", result);
    result = no ? next_value() : value;
    $display("skipped %.1f calls=%0d", result, calls);
    result = yes ? next_value() : value;
    $display("called %.1f calls=%0d", result, calls);
    ambiguous = 1'bz;
    result = ambiguous ? next_value() : rounded;
    $display("both %.1f calls=%0d", result, calls);
    negative_zero = $bitstoshortreal(32'h80000000);
    result = yes ? negative_zero : zero;
    $display("zero %016h", $realtobits(result));
    nan_value = $bitstoshortreal(32'h7fc00000);
    result = yes ? nan_value : number;
    $display("nan %0d", result != result);
    result = ambiguous ? nan_value : nan_value;
    $display("nan merge %.1f", result);
  end
endmodule
