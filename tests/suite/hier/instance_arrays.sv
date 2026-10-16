/*
:name: instance_arrays
:description: An array of instances is one instance per element; a connection as wide as all the ports together is sliced among them, the leftmost instance taking the leftmost bits, for descending and ascending ranges and two dimensions; a connection as wide as one port goes to every instance; an unpacked array gives one element to each instance; elements are reached by hierarchical names and print their names with %m
:tags: hier
:type: simulation
:lrm: 1800-2023 23.3.3.5 23.3.2 21.2.1.5
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @instance_arrays.expected
*/
// Icarus Verilog 13.0 accepts neither the two-dimensional array of
// instances nor the unpacked array connected to one; with them removed, the
// other lines match it. Verilator 5.052 rejects the connection of the
// two-dimensional array. The grid and each lines rest on 23.3.3.5 alone.
module inv2 (input logic [1:0] a, input logic en, output logic [1:0] y);
  string name;
  assign y = en ? ~a : a;
  initial $sformat(name, "%m");
endmodule

module top;
  logic [7:0] a8 = 8'b11_10_01_00;
  logic en = 1'b1;
  logic [7:0] down_y, up_y;
  logic [3:0][1:0] grid_y [2];
  logic [1:0] per [3] = '{2'd1, 2'd2, 2'd3};
  logic [5:0] per_y;
  inv2 down [3:0] (.a(a8), .en(en), .y(down_y));
  inv2 up [0:3] (.a(a8), .en(1'b0), .y(up_y));
  inv2 grid [1:0][1:0] (.a(a8), .en(en), .y());
  inv2 each [2:0] (.a(per), .en(1'b0), .y(per_y));
  initial begin
    #1;
    $display("down_y=%b up_y=%b", down_y, up_y);
    $display("down[3].a=%b down[0].a=%b up[0].a=%b up[3].a=%b",
             down[3].a, down[0].a, up[0].a, up[3].a);
    $display("grid[1][0].a=%b grid[0][1].a=%b", grid[1][0].a, grid[0][1].a);
    $display("each: [2].a=%b [0].a=%b per_y=%b", each[2].a, each[0].a, per_y);
    $display("names %s %s %s %s", down[3].name, up[0].name, grid[1][0].name, each[1].name);
  end
endmodule
