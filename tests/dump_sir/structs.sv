// Unpacked structures (SIR_SPEC.md 4.2): one slot per innermost member,
// named after its path; member defaults (7.2.2) in static-init; an
// automatic structure as frame variables; a whole copy member by member.
module top;
  typedef struct { int x; int y = 2; } point_t;
  typedef struct { point_t a; logic [3:0] tag; byte b [2]; } box_t;
  box_t p, q;
  task automatic t();
    point_t local_p;
    local_p.x = 1;
  endtask
  initial begin
    p.a.x = 5;
    q = p;
    t();
  end
endmodule
