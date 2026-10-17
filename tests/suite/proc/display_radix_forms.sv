/*
:name: display_radix_forms
:description: The b, o and h forms of $display, $write and $swrite print values without a format specifier in their radix with the automatic width, strings as text, and explicit specifiers as written; $printtimescale prints the unit and precision of the calling or named instance; %t takes a real time too
:tags: proc
:type: simulation
:lrm: 1800-2023 21.2.1 21.2.1.3 21.3.3 20.4.2 21.2.1.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @display_radix_forms.expected
*/
`timescale 1ns/1ps
module child;
endmodule
`timescale 10us/1ns
module other;
endmodule
`timescale 1ns/1ps
module top;
  logic [7:0] v = 8'hA5;
  logic [5:0] x = 6'b10x1z0;
  logic signed [7:0] n = -8'sd3;
  string s = "txt";
  string out;
  real t;
  child c ();
  other o ();
  initial begin
    $display(v, " ", x);
    $displayb(v, " ", x);
    $displayo(v, " ", x);
    $displayh(v, " ", x);
    $displayh(n, " ", s);
    $displayh("dec %0d hex ", v, " ", n);
    $writeb(v);
    $write("|");
    $writeo(v);
    $writeh(" ", v, "\n");
    $swriteb(out, v);
    $display("swriteb %s", out);
    $swriteo(out, "%0d ", v, v);
    $display("swriteo %s", out);
    $swriteh(out, x);
    $display("swriteh %s", out);
    $printtimescale;
    $printtimescale(c);
    $printtimescale(o);
    $timeformat(-9, 3, " ns", 10);
    #1.25;
    t = $realtime;
    $display("[%t] [%0t] [%0t]", t, t + 0.0004, t + 0.0006);
  end
endmodule
