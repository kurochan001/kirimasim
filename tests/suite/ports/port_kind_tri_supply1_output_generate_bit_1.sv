/*
:name: port_kind_tri_supply1_output_generate_bit_1
:description: Effective net kinds and implicit origins belong to connected physical bits
:tags: ports
:type: simulation
:lrm: 1800-2023 23.3.3.7 28.12 28.15
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_port_kind_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
module leaf(output tri [0:0] p,input logic [0:0] d);
assign (weak1,weak0) p=d;
endmodule
module top; if(1) begin:g
logic [0:0] a,b;
supply1 [0:2] w;
leaf u(w[1],a);
assign (weak1,weak0) w[1]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='1; b='0; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='x; b='z; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='z; b='x; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='z; b='z; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='1; b='z; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='z; b='0; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a='z; b='z; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
a=1'b0; b=1'b0; #1;
if(u.p !== 1'b1) ++errors;
if(w !== 3'b111) ++errors;
$display("errors=%0d",errors);
end
end endmodule
