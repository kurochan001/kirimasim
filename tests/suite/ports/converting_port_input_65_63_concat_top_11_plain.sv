/*
:name: converting_port_input_65_63_concat_top_11_plain
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
module leaf(input wire logic signed [64:0] p,input logic [64:0] d);
assign (weak1,weak0) p=d;
endmodule
module top;
logic [64:0] a;
logic [62:0] b;
wire logic signed [64:0] w;
leaf u({w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19],w[20],w[21],w[22],w[23],w[24],w[25],w[26],w[27],w[28],w[29],w[30],w[31],w[32],w[33],w[34],w[35],w[36],w[37],w[38],w[39],w[40],w[41],w[42],w[43],w[44],w[45],w[46],w[47],w[48],w[49],w[50],w[51],w[52],w[53],w[54],w[55],w[56],w[57],w[58],w[59],w[60],w[61],w[62],w[63]},a);
assign (weak1,weak0) {w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19],w[20],w[21],w[22],w[23],w[24],w[25],w[26],w[27],w[28],w[29],w[30],w[31],w[32],w[33],w[34],w[35],w[36],w[37],w[38],w[39],w[40],w[41],w[42],w[43],w[44],w[45],w[46],w[47],w[48],w[49],w[50],w[51],w[52],w[53],w[54],w[55],w[56],w[57],w[58],w[59],w[60],w[61],w[62],w[63]}=b;

initial begin
int errors=0;
a=65'b00000000000000000000000000000000000000000000000000000000000000000; b=63'b111111111111111111111111111111111111111111111111111111111111111; #1;
if(u.p !== 65'b00111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 65'bz111111111111111111111111111111111111111111111111111111111111111z) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111111; b=63'b000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 65'bz000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; b=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 65'b00xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; #1;
if(u.p !== 65'b00xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 65'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz) ++errors;
a=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 65'b00zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111111; b=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 65'b00111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=63'b000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 65'b00000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 65'bz000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=65'b11111111111111111111111111111111111111111111111111111111111111110; b=63'b111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 65'b00111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 65'bz011111111111111111111111111111111111111111111111111111111111111z) ++errors;
$display("errors=%0d",errors);
end
endmodule
