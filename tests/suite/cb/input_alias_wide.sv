/*
:name: input_alias_wide
:description: One-step inputs retain four-state planes, signedness, two-state data and packed selections
:tags: clocking sampling wide
:type: simulation
:lrm: 1800-2023 14.4 14.10 14.13
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @input_alias_wide.expected
*/
module top;
  bit clk = 0;
  logic [128:0] d = 'z;
  logic [0:128] ascending = 'x;
  int signed_data = -7;
  bit [31:0] two = '1;
  clocking cb @(posedge clk);
    input wide = d;
    input low = d[64:0];
    input selected = ascending[2:66];
    input signed_data, two;
    input unsigned_full = signed_data[31:0];
  endclocking
  clocking nb @(negedge clk);
    default input #1step;
    input d;
  endclocking
  initial begin
    #1 d = '1;
    #100 clk = 1; d = '0; ascending = '1; signed_data = 9; two = 0;
    #10 clk = 0;
    #5 d = 'x;
    #5 clk = 1; d = 'z;
    #5 clk = 0;
    #5 clk = 1; d = '0;
  end
  initial begin
    if (cb.wide !== {129{1'bx}} || cb.two !== 0) $fatal(1, "clockvar default");
    @(cb);
    if (cb.wide !== {129{1'b1}} || cb.low !== {65{1'b1}} ||
        cb.selected !== {65{1'bx}} || cb.signed_data != -7 ||
        cb.signed_data >= 0 || cb.unsigned_full !== 32'hfffffff9 || cb.unsigned_full < 0 ||
        cb.two !== 32'hffffffff || d !== 0)
      $fatal(1, "first snapshot");
    @(nb);
    if (nb.d !== 0) $fatal(1, "negedge snapshot");
    @(cb);
    if (cb.wide !== {129{1'bx}} || cb.selected !== {65{1'b1}})
      $fatal(1, "X snapshot");
    @(cb);
    if (cb.wide !== {129{1'bz}} || cb.low !== {65{1'bz}})
      $fatal(1, "Z snapshot");
    $display("PASS");
    $finish;
  end
endmodule
