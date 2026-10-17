// Many small processes: 64 that wait for time with delays of different
// lengths, and 64 that wait for a named event triggered every 6 time units.
// Each counts its wakeups. All wakeups happen at even times, and the check at
// the end runs at an odd time, so no process races with it.
// +n=<time units> sets how long the processes run.
// bench-n: 1000000
// bench-expect: proc_wait n=1000000 sum=35376317874
module proc_wait;
  int timed[64];
  int evented[64];
  event tick;
  int n;

  initial forever #6 ->tick;

  for (genvar g = 0; g < 64; g++) begin : procs
    initial forever begin
      #((g % 7 + 1) * 2);
      timed[g] = timed[g] + g + 1;
    end
    initial forever begin
      @tick;
      evented[g] = evented[g] + 1;
    end
  end

  initial begin
    longint sum;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    #(n * 2 + 1);
    sum = 0;
    for (int g = 0; g < 64; g++) sum = sum + longint'(timed[g]) * (g + 3) + evented[g];
    $display("proc_wait n=%0d sum=%0d", n, sum);
    $finish;
  end
endmodule
