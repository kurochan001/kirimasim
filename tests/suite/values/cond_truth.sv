/*
:name: cond_truth
:description: A condition with a 1 bit is true; with no 1 bit but an x or z bit it is ambiguous, and if treats that as false
:tags: values if logical
:type: simulation
:lrm: 1800-2023 12.4 11.4.5 11.4.7
:oracle: spec iverilog@13.0
:fourstate: yes
:nondeterministic: no
:expected_stdout: @cond_truth.expected
*/
module top;
  logic [1:0] v;
  initial begin
    v = 2'b1x;
    if (v) $display("1x:true"); else $display("1x:false");
    v = 2'b0x;
    if (v) $display("0x:true"); else $display("0x:false");
    v = 2'b0z;
    if (v) $display("0z:true"); else $display("0z:false");
    v = 2'b00;
    if (v) $display("00:true"); else $display("00:false");
    $display("not0x=%b", !(2'b0x));
    $display("not1x=%b", !(2'b1x));
    $display("and=%b", (2'b0x) && 1'b1);
    $display("or=%b", (2'b0x) || 1'b1);
  end
endmodule
