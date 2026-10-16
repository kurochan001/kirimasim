/*
:name: packed_members
:description: Members of packed structures and unions are bits of the whole: read and written through nested members, parts of members, elements of unpacked and packed arrays, function arguments, and the assignment operators
:tags: values struct
:type: simulation
:lrm: 1800-2023 7.2.1 7.3.1 7.4 11.4.1 11.5.1
:oracle: spec iverilog@13.0 verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @packed_members.expected
*/
// A packed structure is a vector whose first member is the most
// significant (7.2.1); the members of a packed union all start at bit 0
// (7.3.1). Icarus 13.0 matches the lines without arrays of structures,
// which it does not accept ("elems" and "packed"). Verilator 5.052 matches
// every line but the last, where it shows the 3-bit signed member n as
// -897; -3 + 2 in three signed bits is -1.
module top;
  typedef struct packed { logic [3:0] hi; logic [3:0] lo; } byte_t;
  typedef struct packed { byte_t a; logic s; logic signed [2:0] n; } rec_t;
  typedef union packed { logic [7:0] raw; byte_t parts; } u_t;
  rec_t r;
  u_t u;
  byte_t arr [3];
  byte_t [2:0] pk;
  int i;

  function automatic logic [3:0] hiOf(byte_t b);
    return b.hi;
  endfunction

  initial begin
    r = '0;
    r.a.hi = 4'hA;
    r.a.lo = 4'h5;
    r.s = 1;
    r.n = -3;
    $display("fields %h %h %h %b %0d %h", r, r.a, r.a.hi, r.s, r.n, hiOf(r.a));
    r.a.lo[2:1] = 2'b11;
    r.a.hi[0] = 1'b1;
    $display("parts %h %b", r, r.a);
    u.raw = 8'h3C;
    $display("union %h %h %h", u.parts.hi, u.parts.lo, u.parts);
    u.parts.lo = 4'hF;
    $display("union %h", u.raw);
    arr[1] = 8'h12;
    arr[1].lo = 4'h9;
    i = 2;
    arr[i] = 8'h00;
    arr[i].hi = 4'h7;
    $display("elems %h %h %h", arr[1], arr[2], arr[2].hi);
    pk = '0;
    pk[1].hi = 4'hE;
    pk[i].lo = 4'h3;
    $display("packed %h %h %h", pk, pk[1], pk[i].lo);
    r.a.hi++;
    r.n += 2;
    $display("ops %h %0d", r.a, r.n);
  end
endmodule
