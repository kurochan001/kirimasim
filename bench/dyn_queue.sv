// A dynamic array and a queue: each iteration pushes a value onto the queue,
// pops the oldest once it holds more than 32, and writes back into the array.
// +n=<iterations> sets how many times the loop runs.
// bench-n: 4000000
// bench-expect: dyn_queue n=4000000 sum=3888724928
module dyn_queue;
  int n;

  initial begin
    int q[$];
    int d[];
    longint sum;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    d = new[64];
    foreach (d[i]) d[i] = i * 3 + 1;
    sum = 0;
    for (int i = 0; i < n; i++) begin
      q.push_back(d[i % 64] + i);
      if (q.size() > 32) sum = sum + q.pop_front();
      d[i % 64] = d[i % 64] ^ q[q.size() - 1];
    end
    while (q.size() > 0) sum = sum + q.pop_front();
    $display("dyn_queue n=%0d sum=%0d", n, sum);
    $finish;
  end
endmodule
