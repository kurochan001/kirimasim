/*
:name: struct_arrays
:description: Arrays of unpacked structures: members of elements chosen by constant and run-time indices are read and written, as are array members of elements and bits of packed members; whole elements and whole arrays are assigned from patterns and copies, computing the right side first; members take their defaults in every element; comparisons, event controls, foreach, nonblocking assignments and indices outside the array
:tags: values
:type: simulation
:lrm: 1800-2023 7.2 7.2.2 7.4 7.4.5 10.9 11.2.2
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @struct_arrays.expected
*/
// Icarus Verilog 13.0 accepts no unpacked structures. The other lines match
// Verilator 5.052; it has two states, so "compare x" sees no x, and for
// "outside" it writes and reads element 1, where 7.4.5 ignores a write
// outside the array and reads the default of the element's type.
module top;
  typedef struct { int x; int y = 7; logic [7:0] tag; } point_t;
  typedef struct { point_t ends [2]; byte data [3]; } line_t;
  point_t pts [3];
  point_t pts2 [0:2];
  point_t grid [2][2];
  line_t ln;
  point_t one;
  int i, wakes;

  always @(pts[1].x) wakes++;

  initial begin
    $display("defaults %0d %0d %0d", pts[0].y, pts[2].y, grid[1][1].y);
    for (i = 0; i < 3; i++) begin
      pts[i].x = i * 10;
      pts[i].tag = 8'h10 + i;
    end
    i = 2;
    $display("dynamic %0d %0d %h", pts[i].x, pts[i - 1].x, pts[i].tag);
    pts[i].tag[3:0] = 4'hf;
    $display("bits %h", pts[2].tag);
    pts[i] = '{x: 5, y: 6, tag: 8'haa};
    $display("element pattern %0d %0d %h", pts[2].x, pts[2].y, pts[2].tag);
    pts[0] = pts[2];
    one = pts[1];
    $display("element copies %0d %0d %0d", pts[0].x, pts[0].y, one.x);
    pts2 = pts;
    pts2[1].x = 99;
    $display("array copy %0d %0d %0d", pts2[0].x, pts2[1].x, pts[1].x);
    pts = '{pts[2], pts[1], pts[0]};
    $display("swap %0d %0d %0d", pts[0].x, pts[1].x, pts[2].x);
    pts = '{default: point_t'{x: 1, y: 2, tag: 8'h00}};
    $display("default pattern %0d %0d %0d", pts[0].x, pts[2].y, pts[1].tag);
    ln.ends[1].x = 3;
    ln.data[2] = 8'd4;
    i = 1;
    ln.data[i] = ln.ends[i].x + ln.data[2];
    $display("nested %0d %0d %0d", ln.ends[1].x, ln.data[1], ln.ends[0].y);
    grid[1][0].x = 8;
    grid[0][1] = grid[1][0];
    $display("2-D %0d %0d", grid[0][1].x, grid[0][1].y);
    pts2 = pts;
    $display("compare %b %b %b", pts == pts2, pts[0] != pts[1], pts[0] === pts2[0]);
    pts2[2].tag = 8'hxx;
    $display("compare x %b %b", pts == pts2, pts !== pts2);
    #1 wakes = 0;  // after the wakes of time 0
    #1 pts[1].x = 50;
    #1 pts[2].x = 50;
    #1 pts[1].y = 50;
    #1 $display("wakes %0d", wakes);
    foreach (pts[k]) pts[k].x = k + 100;
    $display("foreach %0d %0d %0d", pts[0].x, pts[1].x, pts[2].x);
    pts[1].x <= 7;
    $display("nba before %0d", pts[1].x);
    #1 $display("nba after %0d", pts[1].x);
    i = 5;
    pts[i].x = 1;
    $display("outside %0d %0d %b", pts[i].x, pts[i].y, pts[i].tag);
  end
endmodule
