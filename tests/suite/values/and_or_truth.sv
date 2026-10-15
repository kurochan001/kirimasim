/*
:name: and_or_truth
:description: Four-state truth tables of bitwise and, or, xor for every pair of 0, 1, x, z
:tags: values bitwise
:type: simulation
:lrm: 1800-2023 11.4.8
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @and_or_truth.expected
*/
module top;
  logic v [0:3];
  initial begin
    v[0] = 1'b0; v[1] = 1'b1; v[2] = 1'bx; v[3] = 1'bz;
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        $display("%b%b and=%b or=%b xor=%b", v[i], v[j], v[i] & v[j], v[i] | v[j], v[i] ^ v[j]);
  end
endmodule
