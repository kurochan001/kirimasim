// assign, deassign, force and release (SIR_SPEC.md 5.6, 7.1): an override
// process per assign and force, started and stopped by the statements.
module top;
  logic [7:0] a, v;
  wire [7:0] w;
  assign w = a;
  initial begin
    assign v = a;
    force w[3:0] = a[7:4];
    force {v, w[7:4]} = 12'hfff;
    release w[3:0];
    release {v, w[7:4]};
    deassign v;
  end
endmodule
