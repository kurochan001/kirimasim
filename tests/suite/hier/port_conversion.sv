/*
:name: port_conversion
:description: Input and output port connections convert widths like assignments: truncation, sign and zero extension, and z passing through
:tags: hierarchy ports
:type: simulation
:lrm: 1800-2023 4.9.6 23.3.3 10.7 23.6
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @port_conversion.expected
*/
// Each connection acts as a continuous assignment (4.9.6): the value is
// extended or truncated to the target's width as in an assignment (10.7),
// with sign extension only when the source is signed. The child's ports
// are read with hierarchical names (23.6).
module child (input logic [3:0] a, input logic [3:0] b, output logic [7:0] o,
              output logic [1:0] n);
  assign o = {a, b};
  assign n = a[3:2];
endmodule

module top;
  logic [7:0] p8;         // wider than a: truncated
  logic signed [1:0] s2;  // narrower than b and signed: sign-extended
  wire [3:0] o4;          // narrower than o: truncated
  wire [3:0] n4;          // wider than n, which is unsigned: zero-extended
  child c (.a(p8), .b(s2), .o(o4), .n(n4));
  initial begin
    p8 = 8'ha5;
    s2 = -2'sd1;
    #1 $display("a=%b b=%b o=%b n=%b o4=%b n4=%b", c.a, c.b, c.o, c.n, o4, n4);
    p8 = 8'h3c;
    s2 = 2'sb01;
    #1 $display("a=%b b=%b o=%b n=%b o4=%b n4=%b", c.a, c.b, c.o, c.n, o4, n4);
    p8 = 8'bzzzz_zzzz;
    s2 = 2'sb00;
    #1 $display("a=%b b=%b o=%b n=%b o4=%b n4=%b", c.a, c.b, c.o, c.n, o4, n4);
  end
endmodule
