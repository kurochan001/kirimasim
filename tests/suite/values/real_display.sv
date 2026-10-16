/*
:name: real_display
:description: A real without a format specifier prints like %g; an integer specifier prints the real rounded to a 64-bit signed integer; a real specifier prints an integer as a real
:tags: values real display
:type: simulation
:lrm: 1800-2023 21.2.1.1 6.12.1
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @real_display.expected
*/
// The standard leaves the form of a real without a specifier open; kirimasim
// prints it like %g (SYSTEM_TASKS.md 3.6). With %d the real is first rounded
// to a 64-bit signed integer (6.12.1: 1.5 rounds to 2), so the automatic
// width is 20 characters. Icarus Verilog 13.0 prints 1.50000 for the first
// line and no automatic width for %d.
module top;
  real r;
  initial begin
    r = 1.5;
    $display(r);
    $display("[%d]", r);
    $display("[%0d] [%h]", -r, r);
    $display("[%f] [%e]", 5, 2);
  end
endmodule
