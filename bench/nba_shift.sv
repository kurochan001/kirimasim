// A clocked design with many nonblocking assignments: a 64-stage shift
// register of 16-bit words fed by an LFSR, and 16 accumulators that each add
// one stage of it on every rising edge. The first rising edge resets them.
// +n=<cycles> sets the number of clock cycles.
// bench-n: 1000000
// bench-expect: nba_shift n=1000000 sum=1b0c71c3
module nba_shift;
  logic clk = 1'b0;
  logic rst = 1'b1;
  logic [15:0] lfsr;
  logic [15:0] sr[64];
  logic [31:0] acc[16];
  int n;

  always #5 clk = ~clk;
  initial #10 rst = 1'b0;

  always_ff @(posedge clk) begin
    if (rst) begin
      lfsr <= 16'hace1;
      for (int i = 0; i < 64; i++) sr[i] <= '0;
    end else begin
      lfsr <= {lfsr[14:0], lfsr[15] ^ lfsr[13] ^ lfsr[12] ^ lfsr[10]};
      sr[0] <= lfsr;
      for (int i = 1; i < 64; i++) sr[i] <= sr[i-1] + 16'(i);
    end
  end

  for (genvar g = 0; g < 16; g++) begin : accs
    always_ff @(posedge clk) begin
      if (rst) acc[g] <= '0;
      else acc[g] <= acc[g] + 32'(sr[g*4+3]);
    end
  end

  initial begin
    logic [31:0] sum;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    #(n * 10);
    sum = '0;
    for (int g = 0; g < 16; g++) sum = sum ^ (acc[g] << g);
    $display("nba_shift n=%0d sum=%h", n, sum);
    $finish;
  end
endmodule
