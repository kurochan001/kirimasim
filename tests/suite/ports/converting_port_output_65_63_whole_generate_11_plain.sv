/*
:name: converting_port_output_65_63_whole_generate_11_plain
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
module leaf(output wire logic signed [64:0] p,input logic [64:0] d);
assign (weak1,weak0) p=d;
endmodule
module top; if(1) begin:g
logic [64:0] a;
logic [62:0] b;
wire logic signed [62:0] w;
leaf u(w,a);
assign (weak1,weak0) w=b;

initial begin
int errors=0;
a=65'b00000000000000000000000000000000000000000000000000000000000000000; b=63'b111111111111111111111111111111111111111111111111111111111111111; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 63'b000000000000000000000000000000000000000000000000000000000000000) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111111; b=63'b000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 63'b111111111111111111111111111111111111111111111111111111111111111) ++errors;
a=65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; b=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
a=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
a=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111111; b=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 63'b111111111111111111111111111111111111111111111111111111111111111) ++errors;
a=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=63'b000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 63'b000000000000000000000000000000000000000000000000000000000000000) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111110; b=63'b111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 65'b11111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 63'b111111111111111111111111111111111111111111111111111111111111110) ++errors;
$display("errors=%0d",errors);
end
end endmodule
