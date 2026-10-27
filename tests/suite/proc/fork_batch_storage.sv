/*
:name: fork_batch_storage
:description: Small and larger fork batches preserve complete creation and join semantics
:tags: proc fork
:type: simulation
:lrm: 1800-2023 9.3.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @fork_batch_storage.expected
*/
module top;
  int sum;
  task automatic add(input int n);
    #1 sum += n;
  endtask
  initial begin
    repeat (3) begin
      fork
        add(1); add(2); add(3); add(4);
        add(5); add(6); add(7); add(8);
        add(9); add(10); add(11); add(12);
      join
    end
    $display("large=%0d", sum);
    fork add(2); add(3); join
    $display("small=%0d", sum);
  end
endmodule
