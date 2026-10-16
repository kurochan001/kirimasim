/*
:name: port_merge_uwire
:description: An input port of a wand, wor, tri0 or tri1 net on a uwire merges into a uwire, with the warning of table 23-1, and the uwire's one driver gives its value
:tags: hier nets
:type: simulation
:lrm: 1800-2023 23.3.3.7 23.3.3.6 6.6.2
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: yes
:expected_stdout: @port_merge_uwire.expected
:expected_stderr: @port_merge_uwire.stderr
*/
// Table 23-1: an internal wand, wor, tri0 or tri1 net on an external uwire
// takes the uwire kind, with a warning (slang gives it). The merged net
// has the uwire's one driver, so it takes that driver's value, z
// included, with no pull from tri0 or tri1 (SIR_SPEC.md 3.5). 23.3.3.7
// permits the merge but does not require it for input and output ports;
// a simulator that keeps the two nets (Icarus 13.0) gives the pull of
// tri0 and tri1, the second set of lines. An output port would be a
// second driver of the uwire, which slang rejects.
module c_wand (input wand w);
endmodule
module c_wor (input wor w);
endmodule
module c_tri0 (input tri0 w);
endmodule
module c_tri1 (input tri1 w);
endmodule
module top;
  logic a;
  uwire n0, n1, n2, n3;
  assign n0 = a;
  assign n1 = a;
  assign n2 = a;
  assign n3 = a;
  c_wand u0 (.w(n0));
  c_wor u1 (.w(n1));
  c_tri0 u2 (.w(n2));
  c_tri1 u3 (.w(n3));
  initial begin
    a = 1'bz;
    #1 $display("%b %b %b %b", u0.w, u1.w, u2.w, u3.w);
    a = 1'b1;
    #1 $display("%b %b %b %b", u0.w, u1.w, u2.w, u3.w);
  end
endmodule
