/*
:name: fixed_array_port_program_65
:description: Whole fixed-array ports preserve source bounds, ordinal identity and per-element sensitivity
:tags: ports
:type: simulation
:lrm: 1800-2023 7.6 23.3.3.5 23.3.3.7
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_fixed_array_port_matrix.py@1
:top_module: top
:expected_stdout: @fixed_array_port_program_65.expected
*/
program worker(input logic clk,input var logic [64:0] p[7:6],output var logic [64:0] q[-2:-1]);
initial begin @(posedge clk);$display("%0d %0d",p[7],p[6]);q=p;#0;$display("%0d %0d",top.q[4],top.q[5]);#1;end endprogram
module top;logic clk=0;logic [64:0] a[4:5];wire [64:0] q[4:5];worker p(clk,a,q);initial begin a='{0,0};#2;a='{5,9};a[4]<=7;clk=1;#10;end endmodule
