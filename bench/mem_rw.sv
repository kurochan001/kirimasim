// A memory of 1024 words written and read on every clock edge at addresses
// that move by different strides, with the read data folded into the next
// write. The initial values are set at time 0, so the clocked blocks are plain
// always blocks (always_ff would forbid the other writer). The clock rises at
// odd times and the check runs at an even time, so nothing races with it.
// +n=<clock cycles> sets how many cycles run.
// bench-n: 4000000
// bench-expect: mem_rw n=4000000 acc=b55c516d wdata=4df93a12
module mem_rw;
  int n;
  logic clk = 0;
  logic [31:0] mem[1024];
  logic [9:0] waddr, raddr;
  logic [31:0] wdata, rdata, acc;

  always #1 clk = ~clk;

  always @(posedge clk) begin
    mem[waddr] <= wdata;
    rdata <= mem[raddr];
  end

  always @(posedge clk) begin
    waddr <= waddr + 10'd7;
    raddr <= raddr + 10'd13;
    wdata <= {wdata[30:0], wdata[31] ^ wdata[21] ^ wdata[1] ^ wdata[0]} + rdata;
    acc <= acc ^ (rdata + 32'(raddr));
  end

  initial begin
    for (int i = 0; i < 1024; i++) mem[i] = 32'(i) * 32'd2654435761;
    waddr = 0;
    raddr = 5;
    wdata = 32'h12345678;
    rdata = 0;
    acc = 0;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    #(n * 2);
    $display("mem_rw n=%0d acc=%h wdata=%h", n, acc, wdata);
    $finish;
  end
endmodule
