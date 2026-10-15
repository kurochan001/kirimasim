// Static variables of two unnamed blocks have the same hierarchical name;
// the second slot gets "~2" so that slot names stay unique (SIR_SPEC.md 3.1).
module top;
  initial begin
    begin
      int sum;
      sum = 1;
    end
    begin
      int sum;
      sum = 2;
    end
  end
endmodule
