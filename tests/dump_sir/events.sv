// Named events (SIR_SPEC.md 3.2.1, 5.2, 7.2): event slots, an element of an
// event array, ->, ->> with and without a delay, @ev and triggered.
module top;
  event e;
  event ea [1:2];
  bit t;
  initial begin
    @(ea[2]);
    t = e.triggered;
  end
  initial begin
    -> e;
    ->> ea[2];
    ->> #3 e;
  end
endmodule
