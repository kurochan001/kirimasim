// $stacktrace (SYSTEM_TASKS.md 10.3): where it is called, with its scope
// as %m gives it, then each call that led there, innermost first: the task
// or function, or the module instance of the procedure, with the place of
// the call. As a function it returns the lines with no newline after the
// last. A fork branch is a process of its own, which no call led to.
module sub;
  function automatic string f();
    return $stacktrace;
  endfunction
  task t;
    begin : blk
      $display("%s", f());
    end
  endtask
  task automatic w;
    #1 $stacktrace;
  endtask
endmodule
module top;
  sub u();
  if (1) begin : g
    task gt;
      $stacktrace;
    endtask
  end
  initial begin
    $stacktrace;
    u.t;
    fork
      begin : fb
        $stacktrace;
      end
    join
    g.gt;
    u.w;
  end
endmodule
