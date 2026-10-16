/*
:name: package_variables
:description: Package variables are static, one in the design, start with their initializers before time 0, and are reached by imports and by package names, from every instance, event controls and subroutines
:tags: hier package
:type: simulation
:lrm: 1800-2023 26.2 26.3 6.21 6.8 13.4.2
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @package_variables.expected
*/
// A package variable is one variable for the whole design (26.2), static,
// with its initial value set before any initial or always starts (6.8).
// Both instances of child add to the same counter, at different times so
// that the event control sees each change; the static variable of a
// package function is shared too. No initializer here reads a variable
// of another package or of $unit, whose order the standard leaves open.
package p;
  int counter = 5;
  logic [7:0] tag = 8'hA0;
  function int bump();
    static int calls = 0;
    calls++;
    return calls;
  endfunction
  function automatic int twice(int v);
    return 2 * v;
  endfunction
endpackage

module child (input logic go);
  import p::*;
  always @(posedge go) counter = counter + 10;
endmodule

module top;
  import p::counter;
  import p::tag;
  logic g1 = 1'b0, g2 = 1'b0;
  int changes;
  child c1 (.go(g1));
  child c2 (.go(g2));
  always @(p::counter) changes++;
  initial begin
    $display("start %0d %h", counter, p::tag);
    #1 g1 = 1'b1;
    #1 g2 = 1'b1;
    #1 $display("after %0d changes %0d twice %0d", p::counter, changes, p::twice(counter));
    tag = 8'h3C;
    $display("tag %h bump %0d %0d %0d", p::tag, p::bump(), p::bump(), p::bump());
  end
endmodule
