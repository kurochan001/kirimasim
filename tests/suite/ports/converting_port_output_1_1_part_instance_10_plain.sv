/*
:name: converting_port_output_1_1_part_instance_10_plain
:description: Converting net ports resolve directional strong carriers without identity collapse
:tags: ports
:type: simulation
:lrm: 1800-2023 4.9.6 23.3.3.7
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_converting_port_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
module leaf(output wire logic signed [0:0] p,input logic [0:0] d);
assign (weak1,weak0) p=d;
endmodule
module holder;
logic [0:0] a;
logic [0:0] b;
wire logic [2:0] w;
leaf u(w[1:1],a);
assign (weak1,weak0) w[1:1]=b;

initial begin
int errors=0;
a=1'b0; b=1'b1; #1;
if(u.p !== 1'b0) ++errors;
if(w !== 3'bz0z) ++errors;
a=1'b1; b=1'b0; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'bz1z) ++errors;
a=1'bx; b=1'bz; #1;
if(u.p !== 1'bx) ++errors;
if(w !== 3'bzxz) ++errors;
a=1'bz; b=1'bx; #1;
if(u.p !== 1'bz) ++errors;
if(w !== 3'bzxz) ++errors;
a=1'bz; b=1'bz; #1;
if(u.p !== 1'bz) ++errors;
if(w !== 3'bzzz) ++errors;
a=1'b1; b=1'bz; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'bz1z) ++errors;
a=1'bz; b=1'b0; #1;
if(u.p !== 1'bz) ++errors;
if(w !== 3'bz0z) ++errors;
a=1'b0; b=1'b0; #1;
if(u.p !== 1'b0) ++errors;
if(w !== 3'bz0z) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
