// Packages and compilation units (SIR_SPEC.md 2, 3.3, 7.1): a row of the
// instance table for each package and compilation unit with variables,
// their slots named pkg::x and $unit::x, their static-init processes in
// source order before those of the instances, and a package function
// that belongs to its package's row.
package p1;
  int base = 5;
  function automatic int twice(int v);
    return 2 * v;
  endfunction
endpackage
package p2;
  int derived = p1::base + 1;
endpackage
int g = 7;
module top;
  int x = p1::twice(p2::derived);
endmodule
