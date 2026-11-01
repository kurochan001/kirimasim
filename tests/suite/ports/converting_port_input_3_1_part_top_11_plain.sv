/*
:name: converting_port_input_3_1_part_top_11_plain
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
module leaf(input wire logic signed [2:0] p,input logic [2:0] d);
assign (weak1,weak0) p=d;
endmodule
module top;
logic [2:0] a;
logic [0:0] b;
wire logic signed [2:0] w;
leaf u(w[1:1],a);
assign (weak1,weak0) w[1:1]=b;

initial begin
int errors=0;
a=3'b000; b=1'b1; #1;
if(u.p !== 3'b001) ++errors;
if(w !== 3'bz1z) ++errors;
a=3'b111; b=1'b0; #1;
if(u.p !== 3'b000) ++errors;
if(w !== 3'bz0z) ++errors;
a=3'bxxx; b=1'bz; #1;
if(u.p !== 3'b00x) ++errors;
if(w !== 3'bzzz) ++errors;
a=3'bzzz; b=1'bx; #1;
if(u.p !== 3'b00x) ++errors;
if(w !== 3'bzxz) ++errors;
a=3'bzzz; b=1'bz; #1;
if(u.p !== 3'b00z) ++errors;
if(w !== 3'bzzz) ++errors;
a=3'b111; b=1'bz; #1;
if(u.p !== 3'b001) ++errors;
if(w !== 3'bzzz) ++errors;
a=3'bzzz; b=1'b0; #1;
if(u.p !== 3'b000) ++errors;
if(w !== 3'bz0z) ++errors;
a=3'b110; b=1'b0; #1;
if(u.p !== 3'b000) ++errors;
if(w !== 3'bz0z) ++errors;
$display("errors=%0d",errors);
end
endmodule
