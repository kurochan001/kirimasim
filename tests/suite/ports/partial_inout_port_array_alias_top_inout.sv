/*
:name: partial_inout_port_array_alias_top_inout
:description: Static array-element ports preserve physical origins, bounds and untouched bits
:tags: ports
:type: simulation
:lrm: 1800-2023 7.4 23.3.3.7 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_partial_inout_port_array_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
module leaf(inout wire p,input logic d);
assign (weak1,weak0) p=d;
endmodule
module middle(inout wire [1:0] bus[5:4], input logic d);
leaf u(bus[4][1],d);
endmodule
module top;
logic a=0,b=1;
wire [1:0] w[-1:0];
middle m(w,a);
assign (weak1,weak0) w[0][1]=b;
initial begin
int errors=0;
#1; if(w[0] !== 2'bxz || m.u.p !== 1'bx) ++errors;
b=1'bz; #1; if(w[0] !== 2'b0z || m.u.p !== 1'b0) ++errors;
a=1'bz; #1; if(w[0] !== 2'bzz || m.u.p !== 1'bz) ++errors;
if(w[-1] !== 2'bzz) ++errors;
if($left(m.bus)!=5 || $right(m.bus)!=4) ++errors;
$display("errors=%0d",errors);
end

endmodule
