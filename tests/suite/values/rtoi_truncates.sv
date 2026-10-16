/*
:name: rtoi_truncates
:description: $rtoi truncates toward zero, also for negative values and when the argument is a constant
:tags: values real conversion
:type: simulation
:lrm: 1800-2023 20.5
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @rtoi_truncates.expected
*/
// $rtoi truncates (20.5), unlike the assignment of a real to an integer,
// which rounds (6.12.1). kirimasim computes $rtoi of a constant at run time
// because slang's constant evaluation gives 0 for negative values (slang
// issue #2023, SIR_SPEC.md 6).
module top;
  int i;
  real r;
  initial begin
    i = $rtoi(-3.7);
    $display("%0d", i);
    i = $rtoi(3.7);
    $display("%0d", i);
    r = -2.5;
    i = $rtoi(r);
    $display("%0d", i);
    i = r;
    $display("%0d", i);
  end
endmodule
