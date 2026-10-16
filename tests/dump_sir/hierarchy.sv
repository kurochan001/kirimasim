// Instances, ports, nets and generate blocks (SIR_SPEC.md 2, 3.1, 3.5, 7.1):
// the instance table with parents, net kinds, a cont process for each port
// connection and continuous assignment, process names inside generate
// blocks, and the write set of an element with a constant index.
module inv (input wire i, output wire o);
  assign o = ~i;
endmodule

module top;
  logic a;
  tri1 pulled;
  wire [1:0] w [2];
  for (genvar k = 0; k < 2; k++) begin : g
    inv u (.i(a), .o(w[k][0]));
    assign w[k][1] = a;
  end
endmodule
