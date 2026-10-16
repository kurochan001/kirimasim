/*
:name: bit_vector_functions
:description: $countbits, $countones, $onehot, $onehot0 and $isunknown on values with 0, 1, x and z bits, with control bits that repeat, are wider than one bit or are known only at run time, and $bits and $size of fixed types
:tags: values
:type: simulation
:lrm: 1800-2023 20.9 20.6.2 20.7
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @bit_vector_functions.expected
*/
// $countbits counts the bits whose value is one of the control bits, using
// the LSB of each and counting a repeated value once; the other functions
// are defined by it (20.9): x and z bits are not '1, so 8'b0001x000 is
// one-hot. Icarus 13.0 stops with an internal error on a control bit wider
// than one bit (the "wide" line); the other lines match its output.
module top;
  logic [7:0] v;
  bit [7:0] b2;
  logic [63:0] w;
  logic c;
  int arr [3][5];

  function automatic int ones(logic [7:0] x);
    return $countones(x);
  endfunction

  initial begin
    v = 8'b0001x0z1;
    $display("mixed %0d %0d %0d %0d %0d", $countones(v), $countbits(v, '0), $countbits(v, 'x),
             $countbits(v, 'z), $countbits(v, 'x, 'z));
    $display("mixed %b %b %b", $onehot(v), $onehot0(v), $isunknown(v));
    $display("repeat %0d", $countbits(v, '1, '1, '0, '1));
    v = 8'b0001x000;
    $display("onehot-x %b %b %b %0d", $onehot(v), $onehot0(v), $isunknown(v), ones(v));
    v = 8'b00000000;
    $display("zero %b %b %b", $onehot(v), $onehot0(v), $isunknown(v));
    v = 8'bzzzzzzzz;
    $display("allz %0d %b %b", $countbits(v, 'z), $onehot0(v), $isunknown(v));
    c = 1'b0;
    v = 8'b10100101;
    $display("runtime %0d", $countbits(v, c));
    c = 1'bx;
    $display("runtime %0d", $countbits(v, c, 1'b1));
    b2 = 8'hF0;
    $display("2state %0d %b %b", $countones(b2), $onehot(b2), $isunknown(b2));
    w = {32'hFFFF_0000, 32'h0000_0001};
    $display("64 %0d %b", $countones(w), $onehot(w));
    $display("sizes %0d %0d %0d %0d", $bits(v), $bits(w), $size(arr), $size(arr, 2));
  end

  initial begin
    #1;
    v = 8'b10100101;
    $display("wide %0d", $countbits(v, 2'b10));
  end
endmodule
