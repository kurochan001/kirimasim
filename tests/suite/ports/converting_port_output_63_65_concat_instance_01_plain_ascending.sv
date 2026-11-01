/*
:name: converting_port_output_63_65_concat_instance_01_plain_ascending
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
module leaf(output wire logic [62:0] p,input logic [62:0] d);
assign (weak1,weak0) p=d;
endmodule
module holder;
logic [62:0] a;
logic [64:0] b;
wire logic signed [0:66] w;
leaf u({w[65],w[64],w[63],w[62],w[61],w[60],w[59],w[58],w[57],w[56],w[55],w[54],w[53],w[52],w[51],w[50],w[49],w[48],w[47],w[46],w[45],w[44],w[43],w[42],w[41],w[40],w[39],w[38],w[37],w[36],w[35],w[34],w[33],w[32],w[31],w[30],w[29],w[28],w[27],w[26],w[25],w[24],w[23],w[22],w[21],w[20],w[19],w[18],w[17],w[16],w[15],w[14],w[13],w[12],w[11],w[10],w[9],w[8],w[7],w[6],w[5],w[4],w[3],w[2],w[1]},a);
assign (weak1,weak0) {w[65],w[64],w[63],w[62],w[61],w[60],w[59],w[58],w[57],w[56],w[55],w[54],w[53],w[52],w[51],w[50],w[49],w[48],w[47],w[46],w[45],w[44],w[43],w[42],w[41],w[40],w[39],w[38],w[37],w[36],w[35],w[34],w[33],w[32],w[31],w[30],w[29],w[28],w[27],w[26],w[25],w[24],w[23],w[22],w[21],w[20],w[19],w[18],w[17],w[16],w[15],w[14],w[13],w[12],w[11],w[10],w[9],w[8],w[7],w[6],w[5],w[4],w[3],w[2],w[1]}=b;

initial begin
int errors=0;
a=63'b000000000000000000000000000000000000000000000000000000000000000; b=65'b11111111111111111111111111111111111111111111111111111111111111111; #1;
if(u.p !== 63'b000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 67'bz00000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=63'b111111111111111111111111111111111111111111111111111111111111111; b=65'b00000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 63'b111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 67'bz11111111111111111111111111111111111111111111111111111111111111100z) ++errors;
a=63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; b=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 63'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 67'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00z) ++errors;
a=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=65'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; #1;
if(u.p !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 67'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00z) ++errors;
a=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 67'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz00z) ++errors;
a=63'b111111111111111111111111111111111111111111111111111111111111111; b=65'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 63'b111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 67'bz11111111111111111111111111111111111111111111111111111111111111100z) ++errors;
a=63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=65'b00000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 63'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 67'bz00000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=63'b111111111111111111111111111111111111111111111111111111111111110; b=65'b11111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 63'b111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 67'bz01111111111111111111111111111111111111111111111111111111111111100z) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
