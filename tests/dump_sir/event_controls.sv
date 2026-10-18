// Event controls (SIR_SPEC.md 7.1, 7.2): the iff of a trigger, an item with
// iff, an element at a constant index, an expression item (a watch) with
// an edge and an iff, and an or list that mixes a watch and a slot.
module top;
  logic clk, en, a, b, q;
  logic [3:0] m [0:3];
  always_ff @(posedge clk iff en) q <= a;
  initial begin
    @(posedge clk iff en);
    @(negedge m[2][1]);
    @(posedge (a & b) iff en or clk);
  end
endmodule
