// Deferred immediate assertions (SIR_SPEC.md 5.7): the action is a
// function that makes the call with the values passed now; report.call
// puts it on the process's queue; without an action, assert calls $error.
module top;
  int v;
  initial begin
    assert #0 (v == 0) $display("v=%0d", v + 1); else $display("fail");
    assume final (v != 0);
  end
endmodule
