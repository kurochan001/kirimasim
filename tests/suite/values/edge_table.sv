/*
:name: edge_table
:description: posedge and negedge for every transition between 0, 1, x and z; changes between x and z are not edges
:tags: values events edge
:type: simulation
:lrm: 1800-2023 9.4.2
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @edge_table.expected
*/
module top;
  logic s;
  logic v [0:3];
  int pos, neg;
  always @(posedge s) pos++;
  always @(negedge s) neg++;
  initial begin
    v[0] = 1'b0; v[1] = 1'b1; v[2] = 1'bx; v[3] = 1'bz;
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        if (i != j) begin
          s = v[i];
          #1 pos = 0; neg = 0;
          s = v[j];
          #1 $display("%b->%b pos=%0d neg=%0d", v[i], v[j], pos, neg);
        end
  end
endmodule
