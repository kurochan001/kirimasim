/*
:name: casez_expr_dontcare
:description: casez ignores z bits and casex ignores x and z bits in the case expression as well as in the items
:tags: values case
:type: simulation
:lrm: 1800-2023 12.5.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @casez_expr_dontcare.expected
*/
module top;
  logic [3:0] v;
  initial begin
    v = 4'b10z1;
    casez (v)
      4'b1001: $display("casez_expr_z=match");
      default: $display("casez_expr_z=nomatch");
    endcase
    v = 4'b10x1;
    casez (v)
      4'b1001: $display("casez_expr_x=match");
      default: $display("casez_expr_x=nomatch");
    endcase
    casex (v)
      4'b1001: $display("casex_expr_x=match");
      default: $display("casex_expr_x=nomatch");
    endcase
    v = 4'b1011;
    casez (v)
      4'b10?1: $display("casez_item_q=match");
      default: $display("casez_item_q=nomatch");
    endcase
    $display("wildeq_lhs_z=%b", 4'b10z1 ==? 4'b1001);
  end
endmodule
