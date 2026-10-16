/*
:name: unit_variables
:description: Variables declared in the compilation-unit scope are static, start with their initializers before time 0, and are shared by every module that names them
:tags: hier
:type: simulation
:lrm: 1800-2023 3.12.1 6.21 26.3
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @unit_variables.expected
*/
// The compilation-unit scope ($unit) holds declarations outside every
// module (3.12.1); its variables are static (6.21) and are reached by a
// short name or by $unit:: (26.3).
int count = 3;
logic [7:0] tag = 8'hA5;

module bump;
  initial #1 count = count + 10;
endmodule

module top;
  bump u ();
  function int twice();
    return 2 * $unit::count;
  endfunction
  initial begin
    $display("start %0d %0d %h", count, $unit::count, tag);
    #2;
    $display("after %0d twice %0d", count, twice());
    tag = 8'h3C;
    $display("tag %h", tag);
  end
endmodule
