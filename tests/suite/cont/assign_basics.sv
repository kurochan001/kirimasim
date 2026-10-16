/*
:name: assign_basics
:description: Continuous assignments follow their operands: a net declaration assignment, assignments to separate bits of one net, to a variable, a chain written before its source, and a function call
:tags: continuous assignment
:type: simulation
:lrm: 1800-2023 10.3.1 10.3.2 11.4.8
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @assign_basics.expected
*/
// Every display runs one time unit after the last change, so all the
// continuous assignments have settled.
module top;
  logic [3:0] a, b;
  wire [3:0] sum = a + b;  // a net declaration assignment (10.3.1)
  wire [7:0] both;
  assign both[3:0] = a;  // two assignments to separate bits of one net
  assign both[7:4] = b;
  logic [3:0] v;  // a variable driven by one continuous assignment
  assign v = ~a;
  wire [3:0] c1, c2;
  assign c2 = c1 + 1;  // the chain is written before its source
  assign c1 = a;
  function automatic logic [3:0] inc(input logic [3:0] x);
    return x + 1;
  endfunction
  wire [3:0] f = inc(b);
  initial begin
    a = 1;
    b = 2;
    #1 $display("sum=%0d both=%h v=%b c2=%0d f=%0d", sum, both, v, c2, f);
    a = 4'hf;
    #1 $display("sum=%0d both=%h v=%b c2=%0d f=%0d", sum, both, v, c2, f);
    b = 4'bx;
    #1 $display("sum=%0d both=%h v=%b c2=%0d f=%0d", sum, both, v, c2, f);
  end
endmodule
