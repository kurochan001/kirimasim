/*
:name: converting_port_output_129_131_concat_instance_00_plain_ascending
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
module leaf(output wire logic [128:0] p,input logic [128:0] d);
assign (weak1,weak0) p=d;
endmodule
module holder;
logic [128:0] a;
logic [130:0] b;
wire logic [0:132] w;
leaf u({w[131],w[130],w[129],w[128],w[127],w[126],w[125],w[124],w[123],w[122],w[121],w[120],w[119],w[118],w[117],w[116],w[115],w[114],w[113],w[112],w[111],w[110],w[109],w[108],w[107],w[106],w[105],w[104],w[103],w[102],w[101],w[100],w[99],w[98],w[97],w[96],w[95],w[94],w[93],w[92],w[91],w[90],w[89],w[88],w[87],w[86],w[85],w[84],w[83],w[82],w[81],w[80],w[79],w[78],w[77],w[76],w[75],w[74],w[73],w[72],w[71],w[70],w[69],w[68],w[67],w[66],w[65],w[64],w[63],w[62],w[61],w[60],w[59],w[58],w[57],w[56],w[55],w[54],w[53],w[52],w[51],w[50],w[49],w[48],w[47],w[46],w[45],w[44],w[43],w[42],w[41],w[40],w[39],w[38],w[37],w[36],w[35],w[34],w[33],w[32],w[31],w[30],w[29],w[28],w[27],w[26],w[25],w[24],w[23],w[22],w[21],w[20],w[19],w[18],w[17],w[16],w[15],w[14],w[13],w[12],w[11],w[10],w[9],w[8],w[7],w[6],w[5],w[4],w[3],w[2],w[1]},a);
assign (weak1,weak0) {w[131],w[130],w[129],w[128],w[127],w[126],w[125],w[124],w[123],w[122],w[121],w[120],w[119],w[118],w[117],w[116],w[115],w[114],w[113],w[112],w[111],w[110],w[109],w[108],w[107],w[106],w[105],w[104],w[103],w[102],w[101],w[100],w[99],w[98],w[97],w[96],w[95],w[94],w[93],w[92],w[91],w[90],w[89],w[88],w[87],w[86],w[85],w[84],w[83],w[82],w[81],w[80],w[79],w[78],w[77],w[76],w[75],w[74],w[73],w[72],w[71],w[70],w[69],w[68],w[67],w[66],w[65],w[64],w[63],w[62],w[61],w[60],w[59],w[58],w[57],w[56],w[55],w[54],w[53],w[52],w[51],w[50],w[49],w[48],w[47],w[46],w[45],w[44],w[43],w[42],w[41],w[40],w[39],w[38],w[37],w[36],w[35],w[34],w[33],w[32],w[31],w[30],w[29],w[28],w[27],w[26],w[25],w[24],w[23],w[22],w[21],w[20],w[19],w[18],w[17],w[16],w[15],w[14],w[13],w[12],w[11],w[10],w[9],w[8],w[7],w[6],w[5],w[4],w[3],w[2],w[1]}=b;

initial begin
int errors=0;
a=129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000; b=131'b11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111; #1;
if(u.p !== 129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 133'bz00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111; b=131'b00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 133'bz11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111100z) ++errors;
a=129'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; b=131'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 129'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 133'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00z) ++errors;
a=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=131'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; #1;
if(u.p !== 129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 133'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00z) ++errors;
a=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=131'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 133'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz00z) ++errors;
a=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111; b=131'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 133'bz11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111100z) ++errors;
a=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=131'b00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 133'bz00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111110; b=131'b11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 133'bz01111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111100z) ++errors;
$display("errors=%0d",errors);
end
endmodule
module top; holder h(); endmodule
