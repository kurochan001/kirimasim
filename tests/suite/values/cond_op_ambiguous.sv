/*
:name: cond_op_ambiguous
:description: With an x condition, ?: merges both results bit by bit and a z bit on both sides becomes x
:tags: values conditional
:type: simulation
:lrm: 1800-2023 11.4.11
:oracle: spec
:fourstate: yes
:nondeterministic: no
:expected_stdout: @cond_op_ambiguous.expected
*/
// Icarus Verilog 13.0 prints zx for the second line; Table 11-20 of
// 1800-2023 combines z with z into x, so the expected value follows the
// standard.
module top;
  logic c;
  initial begin
    c = 1'bx;
    $display("r1=%b", c ? 4'b0110 : 4'b0101);
    $display("r2=%b", c ? 2'bz1 : 2'bz0);
    $display("r3=%b", c ? 4'b1010 : 4'b1010);
    c = 1'b1;
    $display("r4=%b", c ? 2'bz1 : 2'b00);
  end
endmodule
