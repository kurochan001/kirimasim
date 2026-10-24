/*
:name: fixed_array_port_typed_real_var
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
:expected_stdout: @fixed_array_port_typed_real_var.expected
*/
module leaf(input var real p[7:6], output var real q[-2:-1]); assign q[-2]=p[7]; assign q[-1]=p[6]; endmodule
module top; real a[4:5], q[4:5]; leaf l(a,q); initial begin #1; $display("%f|%f",q[4],q[5]); a[4]=1.25;a[5]=2.5;#1;$display("%f|%f",q[4],q[5]);a[4]=2.5;#1;$display("%f|%f",q[4],q[5]);$display("%0d %0d %0d",$left(l.p),$right(l.p),$dimensions(l.p));end endmodule
