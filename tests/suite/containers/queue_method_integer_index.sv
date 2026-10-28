/*
:name: queue_method_integer_index
:description: Queue method indices convert to signed 32-bit integer before bounds checks
:tags: containers
:type: simulation
:lrm: 1800-2023 7.10.2.2 7.10.2.3
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @queue_method_integer_index.expected
*/
module top;
  int values[$] = '{10, 20, 30};
  logic [63:0] wide_index = 64'h1_0000_0001;
  logic [64:0] wider_index = 65'h1_00000000_00000002;
  logic [63:0] unknown_high = {32'hxxxxxxxx, 32'd1};
  int calls = 0;

  function automatic logic [63:0] next_index();
    calls++;
    return 64'h1_0000_0000;
  endfunction

  initial begin
    values.insert(64'h1_0000_0002, 7);
    $display("literal %0d: %0d %0d %0d %0d", values.size(),
             values[0], values[1], values[2], values[3]);
    values.insert(wide_index, 8);
    $display("variable %0d: %0d %0d %0d %0d %0d", values.size(),
             values[0], values[1], values[2], values[3], values[4]);
    values.delete(wider_index);
    $display("wide delete %0d: %0d %0d %0d %0d", values.size(),
             values[0], values[1], values[2], values[3]);
    values.delete(unknown_high);
    $display("unknown high %0d: %0d %0d %0d", values.size(),
             values[0], values[1], values[2]);
    values.insert(next_index(), 9);
    $display("once %0d: %0d %0d %0d %0d calls=%0d", values.size(),
             values[0], values[1], values[2], values[3], calls);
    values.delete(64'h1_0000_0000);
    $display("literal delete %0d: %0d %0d %0d", values.size(),
             values[0], values[1], values[2]);
    $display("selector %0d %0d", values[wide_index], values[1]);
  end
endmodule
