/*
:name: array_element_port_rank2_67_bit_input_instance_0
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
module holder;
logic [0:0] a,b;
wire [66:0] w[2:1][-1:1];
leaf u(w[1][0][65],a);
assign (weak1,weak0) w[1][0][65]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 1'bx) ++errors;
if(w[2][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][0] !== 67'bzxzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='0; b='z; #1;
if(u.p !== 1'b0) ++errors;
if(w[2][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][0] !== 67'bz0zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='z; b='z; #1;
if(u.p !== 1'bz) ++errors;
if(w[2][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='0; #1;
if(u.p !== 1'bx) ++errors;
if(w[2][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][0] !== 67'bzxzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='x; b='z; #1;
if(u.p !== 1'bx) ++errors;
if(w[2][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][0] !== 67'bzxzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='z; b='z; #1;
if(u.p !== 1'bz) ++errors;
if(w[2][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[2][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][-1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][0] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w[1][1] !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
