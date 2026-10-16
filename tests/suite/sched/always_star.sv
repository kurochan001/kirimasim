/*
:name: always_star
:description: always @* does not run at time 0 and wakes on the nets and variables its statement reads, including left-hand indices, case items, subroutine arguments and variables it also writes, but not what a called function reads inside
:tags: sched
:type: simulation
:lrm: 1800-2023 9.4.2.2 9.2.2.2.2
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @always_star.expected
*/
// The implicit event list of @* holds every net and variable the statement
// reads, except those that only appear as the variable of a left-hand side
// or in timing controls (9.4.2.2), and not what a called function reads
// inside (9.2.2.2.2; SCHEDULER_SPEC.md 9.2). Each change below wakes the
// block or not; the count shows which.
module top;
  logic [1:0] sel = 0, idx = 0;
  logic [3:0] a = 0, b = 0, item = 4'd3, hidden = 0;
  logic [3:0] y, tmp;
  logic [3:0] arr [4];
  int runs;
  function automatic logic [3:0] f(logic [3:0] v);
    return v ^ hidden;
  endfunction
  always @* begin
    runs++;
    tmp = a + 4'd1;
    y = tmp;
    arr[idx] = f(b);
    case (sel)
      2'd0: y = y;
      item[1:0]: y = 4'd9;
      default: ;
    endcase
  end
  task automatic step(input string what);
    #1 $display("%s runs=%0d y=%0d", what, runs, y);
  endtask
  initial begin
    step("start");
    a = 4'd5;
    step("a");
    b = 4'd1;
    step("b");
    idx = 2'd2;
    step("idx");
    sel = 2'd3;
    step("sel");
    item = 4'd0;
    step("item");
    hidden = 4'd7;
    step("hidden");
    $display("arr %0d", arr[2]);
  end
endmodule
