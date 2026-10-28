/*
:name: foreach_omitted_indices
:description: A foreach without any loop variables skips its body while partial omissions retain the other dimensions
:tags: proc foreach
:type: simulation
:lrm: 1800-2023 12.7.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @foreach_omitted_indices.expected
*/
module top;
  int vector[3];
  int matrix[2][3];
  int cube[2][3][4];
  bit [7:0] packed_vector;
  int dynamic_vector[];
  int queue[$];
  int skipped, rows, columns, middle, nested, continued;
  initial begin
    foreach (vector[]) skipped++;
    foreach (matrix[, ]) skipped++;
    foreach (cube[,,]) skipped++;
    foreach (packed_vector[]) skipped++;
    dynamic_vector = new[3];
    foreach (dynamic_vector[]) skipped++;
    queue = '{1, 2, 3};
    foreach (queue[]) skipped++;
    $display("skipped=%0d", skipped);
    foreach (matrix[i,]) rows++;
    foreach (matrix[,j]) columns++;
    foreach (cube[,j,]) middle++;
    $display("rows=%0d columns=%0d middle=%0d", rows, columns, middle);
    for (int outer = 0; outer < 2; outer++) begin
      foreach (matrix[,]) break;
      nested++;
      foreach (matrix[,]) continue;
      continued++;
    end
    $display("nested=%0d continued=%0d", nested, continued);
    foreach (matrix[,]) begin
      #10;
      $fatal(1, "a foreach with no loop variables ran");
    end
    $display("time=%0t", $time);
  end
endmodule
