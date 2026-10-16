/*
:name: case_inside
:description: case inside takes the first item whose set holds the case expression as the inside operator decides, with values, ranges, $ bounds, wildcard bits in items and arrays as members; an item whose match is x is not taken; unique case inside reports nothing when one item matches
:tags: proc
:type: simulation
:lrm: 1800-2023 12.5.4 11.4.13 12.5.3
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @case_inside.expected
*/
// Icarus Verilog 13.0 does not accept case inside. Verilator 5.052 does
// not compile an array as an item; with the array replaced by its values
// it matches the lines whose case expression has no x or z bits, except
// "255", where it does not take [8'd250:$] although $ is the largest value
// of the case expression's type (11.4.13).
module top;
  logic [7:0] v;
  int lim [2] = '{100, 200};
  function automatic string kind(logic [7:0] x);
    case (x) inside
      8'd0: return "zero";
      [8'd1:8'd9], 8'd11: return "small";
      8'b1000_00??: return "128-131";
      [8'd250:$]: return "top";
      lim: return "limit";
      default: return "other";
    endcase
  endfunction
  initial begin
    v = 8'd0;   $display("%0d %s", v, kind(v));
    v = 8'd5;   $display("%0d %s", v, kind(v));
    v = 8'd10;  $display("%0d %s", v, kind(v));
    v = 8'd11;  $display("%0d %s", v, kind(v));
    v = 8'd130; $display("%0d %s", v, kind(v));
    v = 8'd132; $display("%0d %s", v, kind(v));
    v = 8'd200; $display("%0d %s", v, kind(v));
    v = 8'd255; $display("%0d %s", v, kind(v));
    // An x in the case expression: x matches for ranges and exact values
    // are not taken; a wildcard bit of an item still matches it.
    v = 8'b1000_00x1;
    $display("x: %s", kind(v));
    v = 8'b0000_0x01;
    $display("x small: %s", kind(v));
    v = 8'd7;
    unique case (v) inside
      [8'd0:8'd3]: $display("unique low");
      [8'd4:8'd9]: $display("unique mid");
    endcase
  end
endmodule
