// Structure arguments and aggregate returns (SIR_SPEC.md 8): a structure
// argument is one argument per leaf; a function returning a structure
// gives it back through $return, which the caller receives in frame
// variables and copies to the target.
module top;
  typedef struct {int a; int b;} pair_t;
  pair_t p;
  function automatic pair_t swap(pair_t x);
    return '{x.b, x.a};
  endfunction
  initial p = swap(p);
endmodule
