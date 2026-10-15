// A design for the command-line tests: unique, unique0 and priority
// violation reports (SYSTEM_TASKS.md 10.4) and the flush of a violation
// seen only during a zero-delay glitch (SCHEDULER_SPEC.md 15.3).
`timescale 1ns/1ns
module top;
  logic [1:0] sel;
  int hit, h, a;
  always_comb begin
    unique case (sel)
      2'b00: hit = 0;
      2'b01: hit = 1;
    endcase
  end
  initial begin
    sel = 2'b00;
    #1 sel = 2'b10;  // a glitch: the next run of always_comb flushes it
    #0 sel = 2'b01;
    #1 sel = 2'b11;  // stays to the Observed region: reported at 2ns
  end
  initial begin
    #5 a = 3;
    unique if (a > 1) h = 2;
    else if (a > 2) h = 3;
    priority if (a == 0) h = 4;
    unique0 if (a == 0) h = 5;
    unique0 if (a == 0) h = 5;
    else h = 6;
    priority case (a)
      1: h = 6;
      2: h = 7;
    endcase
    unique case (a)
      3, 4: h = 8;
      3: h = 9;
    endcase
    $display("h=%0d", h);
  end
endmodule
