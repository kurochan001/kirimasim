/*
:name: converting_port_input_129_129_concat_generate_00_cast
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
module leaf(input wire logic [128:0] p,input logic [128:0] d);
assign (weak1,weak0) p=d;
endmodule
module top; if(1) begin:g
logic [128:0] a;
logic [128:0] b;
wire logic [130:0] w;
leaf u(129'({w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19],w[20],w[21],w[22],w[23],w[24],w[25],w[26],w[27],w[28],w[29],w[30],w[31],w[32],w[33],w[34],w[35],w[36],w[37],w[38],w[39],w[40],w[41],w[42],w[43],w[44],w[45],w[46],w[47],w[48],w[49],w[50],w[51],w[52],w[53],w[54],w[55],w[56],w[57],w[58],w[59],w[60],w[61],w[62],w[63],w[64],w[65],w[66],w[67],w[68],w[69],w[70],w[71],w[72],w[73],w[74],w[75],w[76],w[77],w[78],w[79],w[80],w[81],w[82],w[83],w[84],w[85],w[86],w[87],w[88],w[89],w[90],w[91],w[92],w[93],w[94],w[95],w[96],w[97],w[98],w[99],w[100],w[101],w[102],w[103],w[104],w[105],w[106],w[107],w[108],w[109],w[110],w[111],w[112],w[113],w[114],w[115],w[116],w[117],w[118],w[119],w[120],w[121],w[122],w[123],w[124],w[125],w[126],w[127],w[128],w[129]}),a);
assign (weak1,weak0) {w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19],w[20],w[21],w[22],w[23],w[24],w[25],w[26],w[27],w[28],w[29],w[30],w[31],w[32],w[33],w[34],w[35],w[36],w[37],w[38],w[39],w[40],w[41],w[42],w[43],w[44],w[45],w[46],w[47],w[48],w[49],w[50],w[51],w[52],w[53],w[54],w[55],w[56],w[57],w[58],w[59],w[60],w[61],w[62],w[63],w[64],w[65],w[66],w[67],w[68],w[69],w[70],w[71],w[72],w[73],w[74],w[75],w[76],w[77],w[78],w[79],w[80],w[81],w[82],w[83],w[84],w[85],w[86],w[87],w[88],w[89],w[90],w[91],w[92],w[93],w[94],w[95],w[96],w[97],w[98],w[99],w[100],w[101],w[102],w[103],w[104],w[105],w[106],w[107],w[108],w[109],w[110],w[111],w[112],w[113],w[114],w[115],w[116],w[117],w[118],w[119],w[120],w[121],w[122],w[123],w[124],w[125],w[126],w[127],w[128],w[129]}=b;

initial begin
int errors=0;
a=129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000; b=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111; #1;
if(u.p !== 129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 131'bz111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111z) ++errors;
a=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111; b=129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 131'bz000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=129'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; b=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 129'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 131'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=129'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; #1;
if(u.p !== 129'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx) ++errors;
if(w !== 131'bzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz) ++errors;
a=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
if(w !== 131'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111; b=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; #1;
if(u.p !== 129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111) ++errors;
if(w !== 131'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz) ++errors;
a=129'bzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz; b=129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000; #1;
if(u.p !== 129'b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000) ++errors;
if(w !== 131'bz000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000z) ++errors;
a=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111110; b=129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111110; #1;
if(u.p !== 129'b111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111110) ++errors;
if(w !== 131'bz011111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111z) ++errors;
$display("errors=%0d",errors);
end
end endmodule
