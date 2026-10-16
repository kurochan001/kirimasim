/*
:name: unsized_wide_literal
:description: An unsized decimal literal that needs more than 32 bits gets the width it needs instead of being truncated to 32 bits
:tags: values literals
:type: simulation
:lrm: 1800-2023 5.7.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @unsized_wide_literal.expected
*/
// 5.7.1: an unsized number that needs more than 32 bits has at least the
// width that represents it, with a sign bit for a signed (decimal) number.
// slang's 1800-2017 mode truncated such literals; haremasim runs slang as
// 1800-2023 (CLI.md 2.1).
module top;
  longint x;
  initial begin
    x = 1000000000000;
    $display("%0d", x);
    x = 4294967296;
    $display("%0d", x);
    $display("%0d", 2147483648 > 0);
  end
endmodule
