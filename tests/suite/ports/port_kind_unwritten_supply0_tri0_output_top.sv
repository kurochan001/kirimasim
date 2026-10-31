/*
:name: port_kind_unwritten_supply0_tri0_output_top
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
module leaf(output supply0 p); endmodule
module top;
tri0 [2:0] w;
leaf u(w[1]);
initial begin
int errors=0;
if(w !== 3'b000 || u.p !== 1'b0) ++errors;
#1; if(w !== 3'b000 || u.p !== 1'b0) ++errors;
$display("errors=%0d",errors);
end
endmodule
