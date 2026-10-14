/*
:name: pulse_history
:description: A process waiting on a signal written twice inside one always_comb run sees the intermediate 0 to 1 to 0 pulse
:tags: scheduling always_comb events
:type: simulation
:lrm: 1800-2023 9.2.2.2 9.4.2 4.7
:oracle: iverilog@13.0
:fourstate: no
:nondeterministic: no
:expected_stdout: hits=1
*/
// Whether the waiting process observes the intermediate value depends on the
// interleaving that 4.7 leaves open. haremasim runs always_comb to completion
// before waking other processes (SCHEDULER_SPEC.md 4.3), but the wakeup is
// already scheduled when pulse goes to 1, so hits is 1. The basis is the
// Icarus Verilog result; the clause numbers still need a human check.
// Verilator 5.052 (--binary) prints hits=0: it does not show the
// intermediate value to the waiting process.
module top;
  logic trigger, pulse;
  int hits;
  always_comb begin
    pulse = trigger;
    pulse = 1'b0;
  end
  always @(posedge pulse) hits++;
  initial begin
    trigger = 0;
    #1 trigger = 1;
    #1 $display("hits=%0d", hits);
  end
endmodule
