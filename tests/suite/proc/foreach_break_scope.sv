/*
:name: foreach_break_scope
:description: Break leaves the complete multidimensional foreach while continue advances one element
:tags: proc foreach
:type: simulation
:lrm: 1800-2023 12.7.3 12.8
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @foreach_break_scope.expected
*/
module top;
  int grid[3][2];
  int cube[2][3][4];
  int dynamic_grid[][];
  int visits, nested, continued, omitted, delayed;
  initial begin
    visits = 0;
    foreach (grid[i,j]) begin visits++; break; end
    $display("fixed=%0d", visits);
    visits = 0;
    foreach (cube[i,j,k]) begin visits++; break; end
    $display("cube=%0d", visits);
    visits = 0;
    foreach (cube[,j,k]) begin visits++; break; end
    $display("omitted=%0d", visits);
    foreach (grid[i,j]) begin
      continued++;
      continue;
      omitted++;
    end
    $display("continue=%0d unreachable=%0d", continued, omitted);
    for (int outer=0; outer<2; outer++)
      foreach (grid[i,j]) begin
        for (int inner=0; inner<3; inner++) begin nested++; break; end
        visits++;
        break;
      end
    $display("nested=%0d visits=%0d", nested, visits);
    visits = 0;
    foreach (grid[i,j]) begin
      foreach (cube[x,y,z]) begin visits++; break; end
    end
    $display("nested_foreach=%0d", visits);
    foreach (grid[i,j]) begin : delayed_body
      automatic int saved = i+j;
      #1;
      delayed += saved+1;
      break;
    end
    $display("delayed=%0d", delayed);
    dynamic_grid = new[3];
    foreach (dynamic_grid[i]) dynamic_grid[i] = new[2];
    visits = 0;
    foreach (dynamic_grid[i,j]) begin visits++; break; end
    $display("dynamic=%0d", visits);
  end
endmodule
