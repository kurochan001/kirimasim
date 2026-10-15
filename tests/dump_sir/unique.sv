// unique case: every item is compared, then the violation checks put
// report.defer on the pending queue, then the first match runs
// (SCHEDULER_SPEC.md 15.1).
module top;
  logic [1:0] sel;
  int hit;
  always_comb begin
    unique case (sel)
      2'b00: hit = 0;
      2'b01, 2'b11: hit = 1;
    endcase
  end
endmodule
