/*
:name: converting_port_output_63_65_whole_instance_11_plain
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
module leaf(output wire logic signed [62:0] p,input logic [62:0] d);
assign (weak1,weak0) p=d;
endmodule
module holder;
logic [62:0] a;
logic [64:0] b;
wire logic signed [64:0] w;
leaf u(w,a);
assign (weak1,weak0) w=b;

initial begin
int errors=0;
a=63'b000000000000000000000000000000000000000000000000000000000000000; b=65'b11111111111111111111111111111111111111111111111111111111111111111; #1;
if(u.p !== 63'b000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
a=63'b111111111111111111111111111111111111111111111111111111111111111; b=65'b00000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 63'b111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
a=63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; b=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
a=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; #1;
if(u.p !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
a=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=63'b111111111111111111111111111111111111111111111111111111111111111; b=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 63'b111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 65'b11111111111111111111111111111111111111111111111111111111111111111) ++errors;
a=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=65'b00000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
a=63'b111111111111111111111111111111111111111111111111111111111111110; b=65'b11111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 63'b111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 65'b11111111111111111111111111111111111111111111111111111111111111110) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
