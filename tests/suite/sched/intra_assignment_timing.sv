/*
:name: intra_assignment_timing
:description: A delay or event control inside an assignment computes the right side when the statement runs; a blocking one assigns when the control is met, choosing the target's element then, and a nonblocking one with a delay updates the target chosen when the statement runs in the NBA region that many time steps later, every one of them taking effect
:tags: sched
:type: simulation
:lrm: 1800-2023 9.4.5 10.4.1 10.4.2 4.9.4
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @intra_assignment_timing.expected
*/
module top;
  int a, b, y, idx;
  int arr [4];
  logic [7:0] w;
  logic clk = 0;
  always #5 clk = ~clk;
  initial begin
    b = 1;
    a = #3 b;               // the value 1, taken at time 0
    $display("%0t blocking a=%0d", $time, a);
    b = 2;
    fork
      #1 b = 20;
    join_none
    a = #2 b;               // still 2: b changes during the wait
    $display("%0t blocking sampled a=%0d", $time, a);
    idx = 0;
    fork
      #1 idx = 3;
    join_none
    arr[idx] = #2 7;        // the element chosen when it resumes: arr[3]
    $display("%0t element chosen late arr0=%0d arr3=%0d", $time, arr[0], arr[3]);
    a = @(posedge clk) b + 1;
    $display("%0t event a=%0d", $time, a);
    // Nonblocking with delays: the value of now, the update in the NBA
    // region of a later time step, after that step's active processes.
    y = 0;
    y <= #2 b;              // 20, two steps from now
    b = 0;
    arr[1] <= #1 99;
    w <= #1 8'h00;
    w[3:0] <= #3 4'hf;
    $display("%0t nba issued y=%0d", $time, y);
    #1 $display("%0t step1 y=%0d arr1=%0d w=%h", $time, y, arr[1], w);
    #1 $display("%0t step2 y=%0d arr1=%0d w=%h", $time, y, arr[1], w);
    #1 $display("%0t step3 y=%0d w=%h", $time, y, w);
    #1 $display("%0t step4 w=%h", $time, w);
    // A pipeline: every update takes effect, in its own time step.
    for (int i = 1; i <= 3; i++) begin
      y <= #3 i * 10;
      #1;
    end
    repeat (4) #1 $display("%0t pipeline y=%0d", $time, y);
    $finish;
  end
endmodule
