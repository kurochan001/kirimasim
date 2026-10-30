/*
:name: unsized_xz_context_extension
:description: Unsized based X/Z literals fill their context across word boundaries without changing sized literals or variable extension
:tags: values literals
:type: simulation
:lrm: 1800-2023 5.7.1 11.6.1
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @unsized_xz_context_extension.expected
*/
// 5.7.1 extends an unsized unsigned based literal whose leading bit is X/Z
// to its expression's width. Ordinary unsigned values still zero-extend.
// The declaration, procedural and NBA contexts must apply the same rule.
module top;
  logic [30:0] below_word = 'hx;
  logic [31:0] word_value = 'hz;
  logic [32:0] above_word = 'bx;
  logic [47:0] declared_x = 'hx, declared_z = 'hZ;
  logic [63:0] word64 = 'oz;
  logic [64:0] above64 = 'h?;
  logic [128:0] wide_x = 'hx, wide_z = 'hz;
  logic [128:0] mixed_z = 'hZ5;
  logic [31:0] source_x = 32'hx;
  logic [47:0] result_x, result_z, sized_x, from_variable, leading_zero;
  logic [128:0] delayed_x, delayed_z;
  bit [128:0] two_state_x = 'hx, two_state_z = 'hz;

  initial begin
    if (below_word !== {31{1'bx}}) $fatal(1, "31-bit X context");
    if (word_value !== {32{1'bz}}) $fatal(1, "32-bit Z context");
    if (above_word !== {33{1'bx}}) $fatal(1, "33-bit X context");
    if (declared_x !== {48{1'bx}}) $fatal(1, "48-bit declaration X");
    if (declared_z !== {48{1'bz}}) $fatal(1, "48-bit declaration Z");
    if (word64 !== {64{1'bz}}) $fatal(1, "64-bit octal Z");
    if (above64 !== {65{1'bz}}) $fatal(1, "65-bit question-mark Z");
    if (wide_x !== {129{1'bx}}) $fatal(1, "129-bit X");
    if (wide_z !== {129{1'bz}}) $fatal(1, "129-bit Z");
    if (mixed_z !== {{125{1'bz}}, 4'h5}) $fatal(1, "leading Z with a payload");
    if (two_state_x !== '0 || two_state_z !== '0) $fatal(1, "2-state conversion");
    result_x = 'hx;
    result_z = 'hz;
    sized_x = 32'hx;
    from_variable = source_x;
    leading_zero = 'h0x;
    if (result_x !== {48{1'bx}} || result_z !== {48{1'bz}})
      $fatal(1, "procedural literal context");
    if (sized_x !== {16'b0, {32{1'bx}}}) $fatal(1, "sized literal zero extension");
    if (from_variable !== {16'b0, {32{1'bx}}}) $fatal(1, "variable zero extension");
    if (leading_zero !== 48'h00000000000x) $fatal(1, "leading known zero");
    if ($bits('hx) < 32 || $bits('hz) < 32) $fatal(1, "self-determined minimum width");
    delayed_x <= 'hx;
    delayed_z <= 'hz;
    #1;
    if (delayed_x !== {129{1'bx}} || delayed_z !== {129{1'bz}})
      $fatal(1, "nonblocking literal context");
    $display("unsized X/Z contexts passed");
  end
endmodule
