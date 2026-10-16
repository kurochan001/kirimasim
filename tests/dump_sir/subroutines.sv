// Functions and tasks (SIR_SPEC.md 8): a static function keeps its argument
// and variable in slots, an automatic one in its frame; an output goes back
// through the caller's frame when the actual is not a whole variable of the
// formal's type; always_comb is sensitive to what a called function reads
// (9.2.2.2.1) and writes what it writes.
module top;
  int g, k, sum;
  logic [7:0] b;
  function int addg(int a);
    static int calls = 0;
    calls = calls + 1;
    return a + g;
  endfunction
  function automatic int twice(int a);
    return 2 * a;
  endfunction
  task automatic split(input logic [7:0] x, output logic [3:0] hi);
    hi = x[7:4];
  endtask
  always_comb sum = addg(k);
  initial begin
    k = twice(3);
    split(8'hA5, b[3:0]);
  end
endmodule
