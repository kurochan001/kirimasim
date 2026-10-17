// User-defined primitives (SIR_SPEC.md 4.3, 5.5): the table rows in the
// type table, udp for a combinational one, and udp.seq with the state and
// the previous inputs in slots of the instance for a sequential one, whose
// initial state static-init writes.
primitive and2 (y, a, b);
  output y;
  input a, b;
  table
    0 ? : 0;
    ? 0 : 0;
    1 1 : 1;
  endtable
endprimitive
primitive dff (q, d, clk);
  output q;
  reg q;
  input d, clk;
  initial q = 1'b1;
  table
    0 r : ? : 0;
    1 r : ? : 1;
    ? f : ? : -;
    * ? : ? : -;
  endtable
endprimitive
module top;
  logic a, b;
  wire y, q;
  and2 u1 (y, a, b);
  dff u2 (q, a, b);
endmodule
