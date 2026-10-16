/*
:name: interface_ports
:description: Interface instances reached through interface ports, with and without a modport, by short names, by a task and a function of the interface, and by hierarchical names
:tags: hier interface
:type: simulation
:lrm: 1800-2023 25.3 25.4 25.5 25.7 23.6
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @interface_ports.expected
*/
// A name through an interface port is the member of the interface
// instance connected to it (25.3); a modport port is the member it names
// (25.5). Each module instance reaches its own interface instance, and the
// interface's own processes run in each instance.
interface bus_if;
  logic [7:0] data;
  logic valid;
  wire [7:0] twice;
  assign twice = {data[6:0], 1'b0};
  int writes;
  always @(data) if ($time > 0) writes++;
  task automatic put(input logic [7:0] v);
    data = v;
    valid = 1'b1;
  endtask
  function automatic logic [7:0] peek();
    return data;
  endfunction
  modport dst (input data, input valid, input twice);
endinterface

module producer (bus_if b, input logic [7:0] v);
  initial #1 b.put(v);
endmodule

module consumer (bus_if.dst b, output logic [7:0] seen, output logic [7:0] dbl);
  always @(posedge b.valid) begin
    seen = b.data;
    #1 dbl = b.twice;
  end
endmodule

module plain (bus_if b);
  initial #4 b.data = b.peek() + 8'd1;
endmodule

module top;
  bus_if i1 ();
  bus_if i2 ();
  logic [7:0] s1, s2, d1, d2;
  producer p1 (.b(i1), .v(8'h5A));
  producer p2 (.b(i2), .v(8'hC3));
  consumer c1 (.b(i1), .seen(s1), .dbl(d1));
  consumer c2 (.b(i2.dst), .seen(s2), .dbl(d2));
  plain q2 (.b(i2));
  initial begin
    i1.valid = 1'b0;
    i2.valid = 1'b0;
    #3 $display("seen %h %h valid %b %b twice %h %h", s1, s2, i1.valid, i2.valid, d1, d2);
    #2 $display("data %h %h writes %0d %0d", i1.data, top.i2.data, i1.writes, i2.writes);
  end
endmodule
