/*
:name: immediate_assertions
:description: Simple immediate assert, assume and cover: x, z and 0 fail, the pass and fail statements run at once, and an assert or assume without else calls $error
:tags: proc assertion
:type: simulation
:lrm: 1800-2023 16.3 12.4 20.10
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @immediate_assertions.expected
:expected_stderr: @immediate_assertions.stderr
*/
// The condition is read as the condition of an if (16.3): a known nonzero
// value passes; 0, x and z fail. A label makes a named block (16.3), so
// it is the scope in the message of the default $error, whose form is
// kirimasim's (SYSTEM_TASKS.md 10.1). Icarus 13.0 reports the same
// failures in its own form, but runs no pass statement of a cover, which
// 16.3 runs when the condition is true ("covered one"), and names the
// scope of the labelled assertion top, not top.chk.
`timescale 1ns/1ps
module top;
  logic one = 1'b1, zero = 1'b0, unk = 1'bx, hiz = 1'bz;
  logic [3:0] v = 4'b1x00;
  logic [1:0] sel = 2'b00;
  int passes, fails;

  function automatic bit check(int k);
    assert (k > 0) else $display("check failed for %0d", k);
    return k > 0;
  endfunction

  always @(sel)
    assert (sel != 2'b11) $display("sel %b ok", sel); else $display("sel %b bad", sel);

  initial begin
    assert (one) passes++; else fails++;
    assert (zero) passes++; else fails++;
    assert (unk) passes++; else fails++;
    assert (hiz) passes++; else fails++;
    assert (v) passes++; else fails++;
    $display("passes %0d fails %0d", passes, fails);
    assert (one) $display("pass statement only");
    assert (zero) $display("not printed");
    chk: assert (unk);
    assume (one == 1'b1) else $display("not printed");
    assume (zero) else $error("assumption zero=%b", zero);
    cover (one) $display("covered one");
    cover (zero) $display("not printed");
    cover (unk) $display("not printed");
    void'(check(2));
    void'(check(-1));
    #1 sel = 2'b01;
    #1 sel = 2'b11;
  end
endmodule
