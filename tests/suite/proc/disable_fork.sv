/*
:name: disable_fork
:description: disable fork ends every descendant of the calling process, including a grandchild whose parent has already finished
:tags: process fork disable
:type: simulation
:lrm: 1800-2023 9.6.3 9.3.2
:oracle: spec verilator@5.052
:fourstate: no
:nondeterministic: no
:expected_stdout: @disable_fork.expected
*/
// child2 starts a grandchild and finishes at time 5. disable fork at time 7
// ends child1 and the grandchild (9.6.3), so neither prints.
// Icarus Verilog 13.0 still prints "[20] grandchild done": it does not end
// the descendants of a child that has already finished.
module top;
  initial begin
    fork
      #10 $display("[%0t] child1 done", $time);
      begin
        fork
          #20 $display("[%0t] grandchild done", $time);
        join_none
        #5 $display("[%0t] child2 done", $time);
      end
    join_none
    #7;
    disable fork;
    $display("[%0t] after disable fork", $time);
    #30 $display("[%0t] end", $time);
  end
endmodule
