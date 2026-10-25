// Calls of automatic functions and tasks: a task with inout and output
// arguments calls a function that recurses four deep, once per iteration.
// +n=<iterations> sets how many times the loop runs.
// bench-n: 1500000
// bench-expect: func_call n=1500000 s=46234688 sum=-983312896
module func_call;
  int n;

  function automatic int mix(input int a, input int b);
    return (a * 31) ^ (b >>> 3) ^ (a + b);
  endfunction

  function automatic int depth(input int k, input int acc);
    if (k == 0) return acc;
    return depth(k - 1, mix(acc, k));
  endfunction

  task automatic step(input int i, inout int s, output int t);
    s = s + mix(i, s);
    t = depth(4, s);
  endtask

  initial begin
    int s, t, sum;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    s = 1;
    sum = 0;
    for (int i = 0; i < n; i++) begin
      step(i, s, t);
      sum = sum ^ t;
    end
    $display("func_call n=%0d s=%0d sum=%0d", n, s, sum);
    $finish;
  end
endmodule
