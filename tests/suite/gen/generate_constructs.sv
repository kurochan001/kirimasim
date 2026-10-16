/*
:name: generate_constructs
:description: Loop, if and case generate constructs with nets, continuous assignments and instances in their blocks, read by hierarchical names
:tags: generate hierarchy
:type: simulation
:lrm: 1800-2023 27.4 27.5 23.6
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @generate_constructs.expected
*/
// Every value is read at time 1, after the continuous assignments of time
// 0 have settled.
module leaf #(parameter int K = 0) (output logic [7:0] o);
  assign o = K * 3;
endmodule

module top;
  localparam int N = 3;
  localparam int MODE = 2;
  wire [7:0] outs [N];
  for (genvar i = 0; i < N; i++) begin : g
    leaf #(.K(i + 1)) u (.o(outs[i]));
    wire [7:0] twice = outs[i] * 2;
  end
  if (MODE == 1) begin : m_one
    wire [7:0] sel = 8'd1;
  end else begin : m_other
    wire [7:0] sel = 8'd2;
  end
  case (MODE)
    1: begin : c1
      wire [3:0] k = 4'd10;
    end
    2: begin : c2
      wire [3:0] k = 4'd11;
    end
    default: begin : cd
      wire [3:0] k = 4'd12;
    end
  endcase
  for (genvar i = 0; i < 2; i++) begin : outer
    for (genvar j = 0; j < 2; j++) begin : inner
      wire [3:0] ij = i * 2 + j;
    end
  end
  initial begin
    #1;
    $display("outs %0d %0d %0d", outs[0], outs[1], outs[2]);
    $display("twice %0d %0d %0d", g[0].twice, g[1].twice, g[2].twice);
    $display("u.o %0d", g[2].u.o);
    $display("if %0d case %0d", m_other.sel, c2.k);
    $display("nested %0d %0d %0d %0d", outer[0].inner[0].ij, outer[0].inner[1].ij,
             outer[1].inner[0].ij, outer[1].inner[1].ij);
  end
endmodule
