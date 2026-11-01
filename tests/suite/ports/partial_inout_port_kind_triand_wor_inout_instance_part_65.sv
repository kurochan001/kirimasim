/*
:name: partial_inout_port_kind_triand_wor_inout_instance_part_65
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
module leaf(inout triand [64:0] p,input logic [64:0] d);
assign (weak1,weak0) p=d;
endmodule
module holder;
logic [64:0] a,b;
wor [66:0] w;
leaf u(w[65:1],a);
assign (strong1,strong0) w[65:1]=b;

initial begin
int errors=0;
a='0; b='1; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
a='1; b='0; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 67'bz00000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a='x; b='z; #1;
if(u.p !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 67'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz) ++errors;
a='z; b='x; #1;
if(u.p !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 67'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a='1; b='z; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 67'bz11111111111111111111111111111111111111111111111111111111111111111z) ++errors;
a='z; b='0; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 67'bz00000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a='z; b='z; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111110; b=65'b11111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 67'bz11111111111111111111111111111111111111111111111111111111111111110z) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
