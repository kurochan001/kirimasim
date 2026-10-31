/*
:name: array_element_port_down_3_part_input_generate_1
:description: Static array-element ports preserve physical origins, bounds and untouched bits
:tags: ports
:type: simulation
:lrm: 1800-2023 7.4 23.3.3.7 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_array_element_port_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
module leaf(input wire [0:0] p,input logic [0:0] d);
assign (weak1,weak0) p=d;
endmodule
module top; if(1) begin:g
logic [0:0] a,b;
wire [2:0] w[-1:-2];
leaf u(w[-2][1:1],a);
assign (strong1,strong0) w[-2][1:1]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 1'b1) ++errors;
if(w[-1] !== 3'bzzz) ++errors;
if(w[-2] !== 3'bz1z) ++errors;
a='0; b='z; #1;
if(u.p !== 1'b0) ++errors;
if(w[-1] !== 3'bzzz) ++errors;
if(w[-2] !== 3'bz0z) ++errors;
a='z; b='z; #1;
if(u.p !== 1'bz) ++errors;
if(w[-1] !== 3'bzzz) ++errors;
if(w[-2] !== 3'bzzz) ++errors;
a='1; b='0; #1;
if(u.p !== 1'b0) ++errors;
if(w[-1] !== 3'bzzz) ++errors;
if(w[-2] !== 3'bz0z) ++errors;
a='x; b='z; #1;
if(u.p !== 1'bx) ++errors;
if(w[-1] !== 3'bzzz) ++errors;
if(w[-2] !== 3'bzxz) ++errors;
a='z; b='z; #1;
if(u.p !== 1'bz) ++errors;
if(w[-1] !== 3'bzzz) ++errors;
if(w[-2] !== 3'bzzz) ++errors;
$display("errors=%0d",errors);
end
end endmodule
