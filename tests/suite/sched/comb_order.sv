/*
:name: comb_order
:description: always_comb runs its statements in order, each reading values from before later writes, and does not retrigger itself on variables it writes
:tags: scheduling always_comb
:type: simulation
:lrm: 1800-2023 9.2.2.2 9.2.2.2.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: b=7 c=0
*/
// Verilator 5.052 (--binary) prints b=7 c=7. 9.2.2.2.1 leaves b out of the
// sensitivity list because the block writes it, so the block does not run
// again after b changes and c keeps the old value of b (0).
module top;
  int a, b, c;
  always_comb begin
    c = b;
    b = a;
  end
  initial begin
    #1 a = 7;
    #1 $display("b=%0d c=%0d", b, c);
  end
endmodule
