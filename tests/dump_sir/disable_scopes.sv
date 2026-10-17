// disable (SIR_SPEC.md 7.2): a block in a task, or one disabled from
// another process, is a scope with scope.enter and scope.leave, also on
// break and return; a block only its own body disables is left with br;
// a task is ended with disable.task.
module top;
  task automatic t(int n);
    for (int i = 0; i < n; i++) begin : lb
      if (i == 2) break;
      if (i == 3) return;
      #1 disable lb;
    end
  endtask
  initial begin : a
    begin : inner
      disable inner;
    end
    #5;
  end
  initial begin
    #2 disable a;
    disable t;
  end
endmodule
