/*
:name: input_sample_1step
:description: A clocking block input with #1step skew samples the value from before the clock edge, not a change made at the edge
:tags: clocking sampling
:type: simulation
:lrm: 1800-2023 14.13 14.4 14.10
:oracle: spec verilator@5.052
:fourstate: no
:nondeterministic: no
:expected_stdout: @input_sample_1step.expected
*/
// With a #1step input skew, the sampled value is the one in the Postponed
// region of the previous time step (14.13). A process that waits for the
// clocking block event reads the updated sampled values (14.13), and the
// signal itself already holds the value written at the edge.
module top;
  logic clk = 0;
  int d;
  clocking cb @(posedge clk);
    default input #1step;
    input d;
  endclocking

  initial begin
    d = 1;
    #10 clk = 1;  // edge 1: d changes in the same time step as the edge
    d = 2;
    #10 clk = 0;
    #5 d = 3;     // changes 5 time units before edge 2
    #5 clk = 1;   // edge 2
  end

  initial begin
    @(cb);
    $display("[%0t] edge 1: cb.d=%0d d=%0d", $time, cb.d, d);
    @(cb);
    $display("[%0t] edge 2: cb.d=%0d d=%0d", $time, cb.d, d);
    $finish;
  end
endmodule
