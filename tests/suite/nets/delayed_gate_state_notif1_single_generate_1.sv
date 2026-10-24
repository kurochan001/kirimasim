/*
:name: delayed_gate_state_notif1_single_generate_1
:description: Complete inertial gate states including side-only pulses and identical reevaluation
:tags: nets
:type: simulation
:lrm: 1800-2023 28.6 28.16
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_delayed_gate_state_matrix.py@1
:top_module: top
:expected_stdout: @delayed_gate_state_notif1_single_generate_1.expected
*/
module top;
timeunit 1ns; timeprecision 1ps;
logic [0:0] data = '0;
logic control = 1'bx, permit = 1;
wire [0:0] q;
function automatic logic read_control(input logic v); return v; endfunction
if (1) begin: nested
notif1 #3 g[0:0](q,data,control);
end
assign (weak1,weak0) q = '1;
initial begin
  data = '0; control = 1'bx;
  #1ps;
  $display("s0 %b", q);
  #1000ps;
  $display("s1 %b", q);
  #1000ps;
  $display("s2 %b", q);
  #1000ps;
  $display("s3 %b", q);
  #1000ps;
  $display("s4 %b", q);
  #1000ps;
  $display("s5 %b", q);
  #4999ps;
  data = '1; control = 1'bx;
  #1ps;
  $display("s6 %b", q);
  #999ps;
  data = '1; control = 1'bz;
  #1ps;
  $display("s7 %b", q);
  #1000ps;
  $display("s8 %b", q);
  #1000ps;
  $display("s9 %b", q);
  #1000ps;
  $display("s10 %b", q);
  #1000ps;
  $display("s11 %b", q);
  #1000ps;
  $display("s12 %b", q);
  #3999ps;
  data = '0; control = 1'bx;
  #1ps;
  $display("s13 %b", q);
  #499ps;
  data = '0; control = 1'bz;
  #1ps;
  $display("s14 %b", q);
  #499ps;
  data = '1; control = 1'bx;
  #1ps;
  $display("s15 %b", q);
  #500ps;
  $display("s16 %b", q);
  #500ps;
  $display("s17 %b", q);
  #500ps;
  $display("s18 %b", q);
  #500ps;
  $display("s19 %b", q);
  #500ps;
  $display("s20 %b", q);
  #500ps;
  $display("s21 %b", q);
  #500ps;
  $display("s22 %b", q);
  #500ps;
  $display("s23 %b", q);
  #500ps;
  $display("s24 %b", q);
  #500ps;
  $display("s25 %b", q);
  #3999ps;
  data = '0; control = 1'b1;
  #1ps;
  $display("s26 %b", q);
  #1000ps;
  $display("s27 %b", q);
  #1000ps;
  $display("s28 %b", q);
  #1000ps;
  $display("s29 %b", q);
  #1000ps;
  $display("s30 %b", q);
  #1000ps;
  $display("s31 %b", q);
  #4999ps;
  data = '1; control = 1'b1;
  #1ps;
  $display("s32 %b", q);
  #1000ps;
  $display("s33 %b", q);
  #1000ps;
  $display("s34 %b", q);
  #1000ps;
  $display("s35 %b", q);
  #1000ps;
  $display("s36 %b", q);
  #1000ps;
  $display("s37 %b", q);
  #4999ps;
  data = '1; control = 1'b0;
  #1ps;
  $display("s38 %b", q);
  #1000ps;
  $display("s39 %b", q);
  #1000ps;
  $display("s40 %b", q);
  #1000ps;
  $display("s41 %b", q);
  #1000ps;
  $display("s42 %b", q);
  #1000ps;
  $display("s43 %b", q);
  #4999ps;
  data = 'x; control = 1'bx;
  #1ps;
  $display("s44 %b", q);
  #1000ps;
  $display("s45 %b", q);
  #1000ps;
  $display("s46 %b", q);
  #1000ps;
  $display("s47 %b", q);
  #1000ps;
  $display("s48 %b", q);
  #1000ps;
  $display("s49 %b", q);
  #4999ps;
  data = 'z; control = 1'bz;
  #1ps;
  $display("s50 %b", q);
  #1000ps;
  $display("s51 %b", q);
  #1000ps;
  $display("s52 %b", q);
  #1000ps;
  $display("s53 %b", q);
  #1000ps;
  $display("s54 %b", q);
  #1000ps;
  $display("s55 %b", q);
  #4999ps;
  data = '0; control = 1'bx;
  #1ps;
  $display("s56 %b", q);
  #1000ps;
  $display("s57 %b", q);
  #1000ps;
  $display("s58 %b", q);
  #1000ps;
  $display("s59 %b", q);
  #1000ps;
  $display("s60 %b", q);
  #1000ps;
  $display("s61 %b", q);
  #4999ps;
  data = '1; control = 1'bz;
  #1ps;
  $display("s62 %b", q);
  #1000ps;
  $display("s63 %b", q);
  #1000ps;
  $display("s64 %b", q);
  #1000ps;
  $display("s65 %b", q);
  #1000ps;
  $display("s66 %b", q);
  #1000ps;
  $display("s67 %b", q);
end
endmodule
