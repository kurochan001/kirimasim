/*
:name: partial_inout_port_kind_mixed_instance_inout
:description: Effective net kinds and implicit origins belong to connected physical bits
:tags: ports
:type: simulation
:lrm: 1800-2023 23.3.3.7 28.12 28.15
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_partial_inout_port_kind_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
module leaf(inout tri0 lo,inout tri1 hi,input logic d);
assign (weak1,weak0) lo=d;
assign (weak1,weak0) hi=d;
endmodule
module holder;
logic a=0,b=1;
wire [3:0] w[2];
leaf u(w[0][1],w[1][2],a);
assign (weak1,weak0) w[0][1]=b;
assign (weak1,weak0) w[1][2]=b;
initial begin
int errors=0;
#1; if(w[0] !== 4'bzz0z || w[1] !== 4'bz1zz || {u.hi,u.lo} !== 2'b10) ++errors;
a=1'bz; b=1'bz; #1;
if(w[0] !== 4'bzz0z || w[1] !== 4'bz1zz || {u.hi,u.lo} !== 2'b10) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
