/*
:name: array_element_port_up_65_whole_output_top_2
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
module leaf(output wire [64:0] p,input logic [64:0] d);
assign (strong1,strong0) p=d;
endmodule
module top;
logic [64:0] a,b;
wire [64:0] w[4:5];
leaf u(w[5],a);
assign (weak1,weak0) w[5]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w[4] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
a='0; b='z; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w[4] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[4] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='0; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[4] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
a='x; b='z; #1;
if(u.p !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w[4] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[4] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
$display("errors=%0d",errors);
end
endmodule
