/*
:name: enum_methods
:description: Enumeration constants and the methods first, last, num, next, prev and name, with wrap-around, counts, and values that are not members
:tags: values enum
:type: simulation
:lrm: 1800-2023 6.19 6.19.5 6.19.5.1 6.19.5.2 6.19.5.3 6.19.5.4 6.19.5.5 6.19.5.6
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @enum_methods.expected
*/
// A value that is not a member gives the default initial value of the base
// type from next() and prev() (6.19.5.3, 6.19.5.4, table 6-7): 0 for the
// 2-state int of `color_t`, x for the 4-state `state_t`. name() gives the
// empty string (6.19.5.6).
module top;
  typedef enum {RED, GREEN = 5, BLUE} color_t;
  typedef enum logic [1:0] {IDLE = 2'b00, RUN = 2'b01, STOP = 2'b11} state_t;
  color_t c, d;
  state_t s;

  function automatic string walk(color_t from, int n);
    color_t k = from;
    string out = "";
    repeat (n) begin
      out = {out, k.name(), " "};
      k = k.next();
    end
    return out;
  endfunction

  initial begin
    c = RED;
    $display("const %0d %0d %0d", RED, GREEN, BLUE);
    $display("first %0d last %0d num %0d", c.first(), c.last(), c.num());
    $display("next %0d %0d %0d", c.next(), c.next(2), c.next(3));
    $display("prev %0d %0d %0d", c.prev(), c.prev(2), c.prev(4));
    c = BLUE;
    d = c.next();
    $display("wrap %0d %0d %s %s", c.next(), c.prev(), d.name(), c.name);
    $display("walk %s", walk(GREEN, 4));
    c = color_t'(3);
    $display("nonmember %0d %0d [%s]", c.next(), c.prev(), c.name());
    s = STOP;
    $display("state %b %b %s num %0d", s.next(), s.prev(), s.name(), s.num());
    s = state_t'(2'b10);
    $display("state nonmember %b %b [%s]", s.next(), s.prev(), s.name());
    s = state_t'(2'bx1);
    $display("state x %b %b [%s]", s.next(), s.prev(), s.name());
    s = s.first();
    for (int i = 0; i < 4; i++) begin
      $write("%s ", s.name());
      s = s.next();
    end
    $display("");
  end
endmodule
