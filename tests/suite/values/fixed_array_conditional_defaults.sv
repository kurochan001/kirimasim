/*
:name: fixed_array_conditional_defaults
:description: Ambiguous fixed-array conditionals retain equal complete elements and default unequal elements by their types
:tags: values arrays conditional
:type: simulation
:lrm: 1800-2023 11.4.11 7.4.5
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @fixed_array_conditional_defaults.expected
*/
// Table 7-1 supplies X for four-state integrals and zero for two-state
// integrals. This differs from merging the bits of an integral operand.
module top;
  logic choose;
  bit [7:0] ba[2], bb[2], br[2];
  int ia[2], ib[2], ir[2];
  integer xa[2], xb[2], xr[2];
  logic [3:0] la[2], lb[2], lr[2];
  logic [64:0] wa[2], wb[2], wr[2];
  real ra[2], rb[2], rr[2];
  string sa[2], sb[2], sr[2];
  initial begin
    ba = '{8'hc3, 8'h5a}; bb = '{8'hc3, 8'h5b};
    ia = '{7, 2}; ib = '{7, 3};
    xa = '{7, 2}; xb = '{7, 3};
    la = '{4'h1, 4'h2}; lb = '{4'h1, 4'h3};
    wa = '{65'h10000000000000001, 65'h10000000000000002};
    wb = '{65'h10000000000000001, 65'h10000000000000003};
    ra = '{1.25, 2.5}; rb = '{1.25, 3.5};
    sa = '{"same", "left"}; sb = '{"same", "right"};
    choose = 1'bx;
    br = choose ? ba : bb;
    ir = choose ? ia : ib;
    xr = choose ? xa : xb;
    lr = choose ? la : lb;
    wr = choose ? wa : wb;
    rr = choose ? ra : rb;
    sr = choose ? sa : sb;
    if (br[0] !== 8'hc3 || br[1] !== 0) $fatal(1, "2-state default");
    if (ir[0] !== 7 || ir[1] !== 0) $fatal(1, "int is two-state");
    if (xr[0] !== 7 || xr[1] !== 'x) $fatal(1, "integer is four-state");
    if (lr[0] !== 4'h1 || lr[1] !== 4'bxxxx) $fatal(1, "whole logic element");
    if (wr[0] !== 65'h10000000000000001 || wr[1] !== {65{1'bx}})
      $fatal(1, "wide whole element");
    if (rr[0] != 1.25 || rr[1] != 0.0) $fatal(1, "real default");
    if (sr[0] != "same" || sr[1] != "") $fatal(1, "string default");
    choose = 1'bz;
    br = choose ? ba : bb;
    ir = choose ? ia : ib;
    xr = choose ? xa : xb;
    if (br[1] !== 0 || ir[1] !== 0 || xr[1] !== 'x) $fatal(1, "Z predicate");
    choose = 1'b1;
    br = choose ? ba : bb;
    if (br[0] !== ba[0] || br[1] !== ba[1]) $fatal(1, "known true");
    choose = 1'b0;
    br = choose ? ba : bb;
    if (br[0] !== bb[0] || br[1] !== bb[1]) $fatal(1, "known false");
    $display("fixed array defaults passed");
  end
endmodule
