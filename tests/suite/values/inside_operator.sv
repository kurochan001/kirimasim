/*
:name: inside_operator
:description: inside matches values, ranges with $ bounds, empty ranges and unpacked array elements; x and z in a list element match any bit, x and z in the left operand do not, and the result is x when nothing matches but some comparison is x; reals and strings compare with ==
:tags: values
:type: simulation
:lrm: 1800-2023 11.4.13 11.4.6
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @inside_operator.expected
*/
// Icarus Verilog 13.0 does not accept inside. Verilator 5.052 matches the
// lines whose left operand has no x or z bits (it reads x as 0); its
// generated C++ does not compile with the two-dimensional array of the
// "nested array" line, which was checked without it.
module top;
  logic [3:0] v;
  int i;
  int arr [3] = '{10, 20, 30};
  int arr2 [2][2] = '{'{1, 2}, '{3, 4}};
  real r;
  string s;
  logic [7:0] b;
  initial begin
    v = 4'd5;
    $display("list hit %b", v inside {4'd1, 4'd3, 4'd5});
    $display("list miss %b", v inside {4'd1, 4'd3});
    $display("range hit %b", v inside {[4'd4:4'd6]});
    $display("range above below %b %b", v inside {[4'd1:4'd4]}, v inside {[4'd6:4'd9]});
    $display("range edge %b %b", v inside {[4'd5:4'd9]}, v inside {[4'd0:4'd5]});
    $display("empty range %b", v inside {[4'd9:4'd1]});
    $display("dollar %b %b", v inside {[$:4'd5]}, v inside {[4'd6:$]});
    i = 20;
    $display("array %b", i inside {arr});
    i = 4;
    $display("nested array %b %b", i inside {arr2}, (i + 1) inside {arr2, 7});
    i = -3;
    $display("signed range %b", i inside {[-5:-1]});
    // x and z in a list element match any value of that bit (==?).
    v = 4'b1010;
    $display("wild element %b %b", v inside {4'b1x10}, v inside {4'bz01z});
    // x in the left operand is not a wildcard.
    v = 4'b1x10;
    $display("left x exact %b", v inside {4'b1010});
    $display("left x differs %b", v inside {4'b0010});
    $display("left x one of them %b", v inside {4'b0000, 4'b1110});
    $display("left x wild %b", v inside {4'b1x10});
    $display("left x range %b", v inside {[4'd0:4'd15]});
    // A match wins over an x comparison.
    $display("x then hit %b", v inside {4'b1010, 4'b1x10});
    // Non-integral elements compare with ==.
    r = 2.5;
    $display("real %b %b", r inside {1.5, 2.5}, r inside {[2.0:3.0]});
    s = "beta";
    $display("string %b %b", s inside {"alpha", "beta"}, s inside {"gamma"});
    // In a condition.
    b = 8'd200;
    if (b inside {[8'd100:8'd250]}) $display("if inside");
    if (!(b inside {8'd1, 8'd2})) $display("if not inside");
  end
endmodule
