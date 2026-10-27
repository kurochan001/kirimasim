/*
:name: atoreal_scan_rules
:description: Real string conversion follows a real-constant prefix rather than C floating syntax
:tags: values string
:type: simulation
:lrm: 1800-2023 6.16.10 5.7.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @atoreal_scan_rules.expected
*/
module top;
  string s;
  function automatic void show(input string text);
    $display("[%s]=%0.3f", text, text.atoreal());
  endfunction
  initial begin
    s="7_2.5"; $display("module=%0.3f", s.atoreal());
    show("7_2.5");
    show(" 4.5");
    show(".75");
    show("2.5e3x");
    show("1E-2");
    show("1e+2");
    show("1e1_2");
    show("nan");
    show("0x1p4");
    show("_12");
    show("");
  end
endmodule
