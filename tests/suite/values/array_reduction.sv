/*
:name: array_reduction
:description: sum, product, and, or and xor of an unpacked array compute in the element type, or in the type of the with clause, over the elements of an array or of a row of one; a with clause sees each element as item or a named iterator; an x element makes the arithmetic x
:tags: values
:type: simulation
:lrm: 1800-2023 7.12.3 7.12
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @array_reduction.expected
*/
// Icarus Verilog 13.0 does not accept these methods on fixed-size arrays;
// the lines without x match Verilator 5.052.
module top;
  byte unsigned b [4] = '{200, 50, 10, 1};
  int a [2:0] = '{3, 4, 5};
  int m [2][3] = '{'{1, 2, 3}, '{4, 5, 6}};
  bit [3:0] bits [3] = '{4'b1100, 4'b1010, 4'b0110};
  logic [7:0] l [3] = '{8'd1, 8'bx, 8'd2};
  int s;
  initial begin
    $display("byte sum %0d", b.sum());
    $display("byte sum as int %0d", b.sum() with (int'(item)));
    $display("int sum %0d product %0d", a.sum(), a.product());
    $display("with item %0d", a.sum() with (item > 3 ? item : 0));
    $display("with named %0d", a.sum(x) with (x * x));
    $display("row sum %0d product %0d", m[1].sum(), m[1].product());
    $display("and %b or %b xor %b", bits.and(), bits.or(), bits.xor());
    $display("x sum %b", l.sum());
    $display("x or %b", l.or());
    s = a.sum() with (item == 4);
    $display("count %0d", s);
  end
endmodule
