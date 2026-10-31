/*
:name: array_element_port_spelling_genvar
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
module leaf(output wire [64:0] p);
assign (weak1,weak0) p='0;
endmodule
module top;
wire [64:0] w[-1:0];
for(genvar i=-1;i<=0;i++) begin:g
leaf u(w[i]);
assign (weak1,weak0) w[i]='1;
end
initial begin #1;
if(w[-1] === {65{1'bx}} && w[0] === {65{1'bx}})
  $display("errors=0"); else $display("errors=1");
end
endmodule
