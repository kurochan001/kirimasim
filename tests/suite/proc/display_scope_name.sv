/*
:name: display_scope_name
:description: %m prints the hierarchical name of the module instance, subroutine, named block or named fork that calls the display task, without unnamed blocks, in generate blocks named or not and in package subroutines; $swrite formats into a string like $write
:tags: proc
:type: simulation
:lrm: 1800-2023 21.2.1.5 21.3.3 27.6
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @display_scope_name.expected
*/
package pk;
  function automatic int where_am_i(int k);
    $display("package function: %m");
    return k;
  endfunction
endpackage

module child #(parameter int N = 1);
  task automatic t();
    $display("task: %m");
  endtask
  function automatic int f();
    $display("function: %m");
    return N;
  endfunction
  generate
    if (N == 2) begin : named_gen
      initial #1 $display("named generate: %m");
    end
    if (N == 2) begin
      initial #2 $display("unnamed generate: %m");
    end
  endgenerate
endmodule

module top;
  child #(.N(2)) u ();
  int k;
  string s;
  initial begin
    #3 $display("module: %m");
    begin : outer
      begin
        begin : inner
          $display("nested: %m, 100%% sure");
        end
      end
    end
    u.t();
    k = u.f();
    k = pk::where_am_i(1);
    fork : forked
      $display("named fork branch: %m");
    join
    $swrite(s, "%m k=%0d", k, " hex=", "%h", 8'hc3);
    $display("swrite: %s", s);
  end
endmodule
