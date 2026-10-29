/*
:name: partial_port_strength_identity
:description: Static part/concat ports retain driver identity, strengths and ascending packed positions
:tags: nets strength ports
:type: simulation
:lrm: 1800-2023 23.3.3.7 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @partial_port_strength_identity.expected
*/
module source(output wire [1:0] o);
  assign (weak1, weak0) o = 2'b10;
endmodule
module sink(input wire [1:0] i);
  assign (weak1, weak0) i = 2'b10;
endmodule
module top;
  wire [0:3] a;
  wire [3:0] b;
  wire [1:0][1:0] packed_net;
  source out_part(a[1:2]);
  sink in_concat({b[3],b[1]});
  source out_packed(packed_net[1]);
  assign (weak1, weak0) a[1:2] = 2'b01;
  assign (strong1, strong0) {b[3],b[1]} = 2'b01;
  assign (weak1, weak0) packed_net[1] = 2'b01;
  initial begin
    #1;
    if (a !== 4'bzxxz || b !== 4'b0z1z || packed_net !== 4'bxxzz)
      $fatal(1, "partial port resolution");
    if (in_concat.i !== 2'b01) $fatal(1, "input port driver strength");
    $display("PASS");
  end
endmodule
