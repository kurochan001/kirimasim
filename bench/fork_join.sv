// Processes started and joined in a loop: each round forks four branches
// that wait for time, trigger and wait for a named event, and update shared
// variables, then waits for all of them with join.
// +n=<rounds> sets the number of rounds.
// bench-n: 4000000
// bench-expect: fork_join n=4000000 a=4000000 b=8000002000000 c=84764928 d=13057792
module fork_join;
  event e;
  int a, d;
  longint b, c;
  int i;
  int n;

  initial begin
    if (!$value$plusargs("n=%d", n)) n = 1000;
    a = 0;
    b = 0;
    c = 0;
    d = 0;
    for (i = 0; i < n; i++) begin
      fork
        begin
          #1 a = a + 1;
          ->e;
        end
        begin
          @e b = b + a;
        end
        begin
          #2 c = c + (a ^ i);
        end
        d = d ^ (i * 7);
      join
    end
    $display("fork_join n=%0d a=%0d b=%0d c=%0d d=%0d", n, a, b, c, d);
    $finish;
  end
endmodule
