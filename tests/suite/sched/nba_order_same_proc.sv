/*
:name: nba_order_same_proc
:description: 同じプロセスの 2 つの NBA は、文を実行した順に反映される
:tags: scheduling nba
:type: simulation
:lrm: 1800-2023 4.6
:oracle: spec
:fourstate: no
:nondeterministic: no
:expected_stdout: q=2
*/
module top;
  logic [1:0] q;
  initial begin
    q <= 2'd1;
    q <= 2'd2;
    #1 $display("q=%0d", q);
  end
endmodule
