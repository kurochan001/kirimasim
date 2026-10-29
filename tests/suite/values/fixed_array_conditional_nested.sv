/*
:name: fixed_array_conditional_nested
:description: One differing leaf defaults its complete outer array element, including nested fixed arrays and unpacked records
:tags: values arrays conditional
:type: simulation
:lrm: 1800-2023 11.4.11 7.4.5
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @fixed_array_conditional_nested.expected
*/
module top;
  typedef logic [3:0] row_t[2];
  typedef struct { bit [7:0] tag; logic [3:0] payload; } record_t;
  row_t a[2], b[2], result[2];
  record_t left_records[2], right_records[2], records[2];
  logic select = 1'bx;
  initial begin
    a = '{'{4'h1, 4'h2}, '{4'h3, 4'h4}};
    b = '{'{4'h1, 4'h2}, '{4'h3, 4'h5}};
    result = select ? a : b;
    if (result[0][0] !== 4'h1 || result[0][1] !== 4'h2)
      $fatal(1, "equal nested element");
    if (result[1][0] !== 'x || result[1][1] !== 'x)
      $fatal(1, "default the complete nested element");
    left_records = '{'{8'h31, 4'h2}, '{8'h42, 4'h3}};
    right_records = '{'{8'h31, 4'h2}, '{8'h42, 4'h4}};
    records = select ? left_records : right_records;
    if (records[0].tag !== 8'h31 || records[0].payload !== 4'h2)
      $fatal(1, "equal record element");
    if (records[1].tag !== 0 || records[1].payload !== 'x)
      $fatal(1, "default all record members");
    // The result owns captured values; changing either operand is independent.
    left_records[0].payload = 4'hf;
    right_records[0].tag = 8'hff;
    if (records[0].tag !== 8'h31 || records[0].payload !== 4'h2)
      $fatal(1, "record result capture");
    $display("nested array defaults passed");
  end
endmodule
