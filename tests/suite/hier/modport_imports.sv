/*
:name: modport_imports
:description: A modport imports a task and a function of the interface, and modport expressions name a part of a member and a member under another name
:tags: hier interface
:type: simulation
:lrm: 1800-2023 25.5 25.5.4 25.7
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @modport_imports.expected
*/
// A task or function that a modport imports is called through the port
// (25.7). A modport expression gives a port name to an expression of the
// interface's members (25.5.4): reading it reads the expression, writing
// it writes the member. Icarus 13.0 does not accept interface ports. The
// first two lines match the output of Verilator 5.052, which does not
// accept the modport expressions; their line follows 25.5.4.
interface bus_if;
  logic [7:0] data;
  logic [3:0] low;
  task automatic put(input logic [7:0] v);
    data = v;
  endtask
  function automatic logic [7:0] peek();
    return data;
  endfunction
  modport src (output data, import put);
  modport dst (input data, import peek);
  modport nib (output .lo(low), input .hi(data[7:4]));
endinterface

module producer (bus_if.src b);
  initial #1 b.put(8'h5A);
endmodule

module consumer (bus_if.dst b, output logic [7:0] seen);
  initial #2 seen = b.peek();
endmodule

module nibbler (bus_if.nib n);
  initial #3 n.lo = n.hi;
endmodule

module top;
  bus_if i ();
  logic [7:0] s;
  producer p (.b(i));
  consumer c (.b(i), .seen(s));
  nibbler n (.n(i));
  initial begin
    #2 $display("data %h", i.data);
    #1 $display("seen %h", s);
    #1 $display("low %h", i.low);
  end
endmodule
