/*
:name: array_element_port_up_67_part_output_top_1
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
assign (weak1,weak0) p=d;
endmodule
module top;
logic [64:0] a,b;
wire [66:0] w[4:5];
leaf u(w[5][65:1],a);
assign (strong1,strong0) w[5][65:1]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[4] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
a='0; b='z; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w[4] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 67'bz00000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[4] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='0; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w[4] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 67'bz00000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a='x; b='z; #1;
if(u.p !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w[4] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 67'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[4] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[5] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
$display("errors=%0d",errors);
end
endmodule
