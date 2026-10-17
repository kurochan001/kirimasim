/*
:name: specparam_defparam
:description: specparams read as constants in the module body and in a specify block that holds only specparams, and defparam overriding parameters of instances, nested instances and an instance in a generate block
:tags: values
:type: simulation
:lrm: 1800-2023 6.20.5 23.10.1 30.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @specparam_defparam.expected
*/
module leaf;
  parameter int W = 4;
  parameter int K = 1;
  logic [W-1:0] bus;
endmodule
module mid;
  parameter int D = 2;
  leaf l ();
endmodule
module top;
  specparam tpd = 10;
  specparam [7:0] twidth = 8'd20;
  specparam tsum = tpd + twidth;
  specify
    specparam tsetup = 5;
  endspecify
  mid m1 ();
  mid m2 ();
  generate
    if (1) begin : g
      leaf l ();
    end
  endgenerate
  defparam m1.D = 7;
  defparam m1.l.W = 16;
  defparam m2.l.K = 3;
  defparam g.l.W = 32;
  initial begin
    $display("tpd=%0d twidth=%0d tsum=%0d", tpd, twidth, tsum);
    $display("m1 D=%0d W=%0d K=%0d bits=%0d", m1.D, m1.l.W, m1.l.K, $bits(m1.l.bus));
    $display("m2 D=%0d W=%0d K=%0d bits=%0d", m2.D, m2.l.W, m2.l.K, $bits(m2.l.bus));
    $display("g W=%0d K=%0d bits=%0d", g.l.W, g.l.K, $bits(g.l.bus));
    #tpd $display("after tpd at %0t", $time);
  end
endmodule
