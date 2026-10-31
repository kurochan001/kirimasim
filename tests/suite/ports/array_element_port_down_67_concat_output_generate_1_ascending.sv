/*
:name: array_element_port_down_67_concat_output_generate_1_ascending
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
module top; if(1) begin:g
logic [64:0] a,b;
wire [0:66] w[-1:-2];
leaf u({w[-1][2:34],w[-2][35:66]},a);
assign (strong1,strong0) {w[-1][2:34],w[-2][35:66]}=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w[-1] !== 67'bzz111111111111111111111111111111111zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-2] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz11111111111111111111111111111111) ++errors;
a='0; b='z; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w[-1] !== 67'bzz000000000000000000000000000000000zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-2] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz00000000000000000000000000000000) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-2] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='0; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w[-1] !== 67'bzz000000000000000000000000000000000zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-2] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz00000000000000000000000000000000) ++errors;
a='x; b='z; #1;
if(u.p !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w[-1] !== 67'bzzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-2] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[-2] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
$display("errors=%0d",errors);
end
end endmodule
