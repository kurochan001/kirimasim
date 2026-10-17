/*
:name: bits_hierarchical
:description: $bits of hierarchical names, a member of an interface instance and an element of a child's array comes from their types alone, without evaluating them
:tags: values
:type: simulation
:lrm: 1800-2023 20.6.2 23.6 25.3
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @bits_hierarchical.expected
*/
interface bus_if;
  logic [11:0] data;
  logic valid;
endinterface
module child #(parameter int W = 5);
  logic [W-1:0] v;
  wire [2*W-1:0] n;
  logic [3:0] mem [0:7];
  typedef struct packed { logic [2:0] a; logic [6:0] b; } pair_t;
  pair_t p;
endmodule
module top;
  bus_if bus ();
  child #(.W(9)) c ();
  generate
    if (1) begin : g
      logic [17:0] gv;
    end
  endgenerate
  initial
    $display("%0d %0d %0d %0d %0d %0d %0d %0d", $bits(c.v), $bits(c.n), $bits(c.mem[2]),
             $bits(c.mem), $bits(c.p), $bits(c.p.b), $bits(bus.data), $bits(g.gv));
endmodule
