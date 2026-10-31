/*
:name: port_kind_supply1_wand_input_generate_array_65
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
module leaf(input supply1 [64:0] p,input logic [64:0] d);
assign (weak1,weak0) p=d;
endmodule
module top; if(1) begin:g
logic [64:0] a,b;
wand [66:0] w[-1:0];
leaf u(w[0][65:1],a);
assign (strong1,strong0) w[0][65:1]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='0; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='x; b='z; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='z; b='x; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='z; b='z; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='z; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='z; b='0; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='z; b='z; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111110; b=65'b11111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[0] !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
$display("errors=%0d",errors);
end
end endmodule
