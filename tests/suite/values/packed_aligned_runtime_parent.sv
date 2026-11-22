/*
:name: packed_aligned_runtime_parent
:description: Aligned indexed part writes stay inside the selected packed element and capture both indices once
:tags: packed-array indexed-part-select nba
:type: simulation
:lrm: 1800-2023 7.4.5 11.5.1 4.9.4
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @packed_aligned_runtime_parent.expected
*/
module top;
  logic [1:0][31:0] bank;
  logic [1:0][0:31] ascending;
  logic signed [3:0] parent;
  logic signed [64:0] lane;
  int parents, lanes;

  function automatic logic signed [3:0] pick_parent();
    parents++;
    return parent;
  endfunction
  function automatic logic signed [64:0] pick_lane();
    lanes++;
    return lane;
  endfunction

  initial begin
    bank = 64'h01234567_89abcdef;
    ascending = '0;
    parent = 1;
    lane = 2;
    bank[pick_parent()][pick_lane()*8 +: 8] = 8'haa;
    $display("%h %0d %0d", bank, parents, lanes);

    parent = 0;
    lane = 4;
    bank[parent][lane*8 +: 8] = '1;
    parent = 1;
    lane = -1;
    bank[parent][lane*8 +: 8] = '1;
    lane = 'x;
    bank[parent][lane*8 +: 8] = '0;
    parent = 'z;
    lane = 0;
    bank[parent][lane*8 +: 8] = '0;
    $display("%h", bank);

    parent = 1;
    lane = 1;
    ascending[parent][lane*8 +: 8] = 8'hc5;
    bank[pick_parent()][(pick_lane()<<3)+7 -: 8] <= #2 8'h3c;
    parent = 0;
    lane = 3;
    bank[parent][(lane*8) & ~7 +: 8] <= #2 8'h9e;
    #1;
    $display("%h %h", bank, ascending);
    #2;
    $display("%h %0d %0d", bank, parents, lanes);
  end
endmodule
