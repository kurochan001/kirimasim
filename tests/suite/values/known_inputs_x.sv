/*
:name: known_inputs_x
:description: Division by zero and an out-of-range array read give x even when every input is known
:tags: values arithmetic arrays
:type: simulation
:lrm: 1800-2023 11.4.3 7.4.5
:oracle: spec iverilog@13.0
:fourstate: yes
:nondeterministic: no
:expected_stdout: div=xxxxxxxx idx=xxxxxxxx
*/
module top;
  logic [7:0] a, b, q_div, q_idx;
  logic [7:0] mem [0:3];
  int idx;
  always_comb q_div = a / b;
  always_comb q_idx = mem[idx];
  initial begin
    foreach (mem[i]) mem[i] = 8'(i);
    a = 8'd10; b = 8'd0; idx = 7;
    #1 $display("div=%b idx=%b", q_div, q_idx);
  end
endmodule
