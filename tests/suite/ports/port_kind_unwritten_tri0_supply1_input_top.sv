/*
:name: port_kind_unwritten_tri0_supply1_input_top
:description: Effective net kinds and implicit origins belong to connected physical bits
:tags: ports
:type: simulation
:lrm: 1800-2023 23.3.3.7 28.12 28.15
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_port_kind_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
module leaf(input tri0 p); endmodule
module top;
supply1 [2:0] w;
leaf u(w[1]);
initial begin
int errors=0;
if(w !== 3'b111 || u.p !== 1'b1) ++errors;
#1; if(w !== 3'b111 || u.p !== 1'b1) ++errors;
$display("errors=%0d",errors);
end
endmodule
