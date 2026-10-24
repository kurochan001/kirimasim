/*
:name: array_methods_nan_fixed_initial
:description: Array-method references, nonintegral keys and snapshot rules
:tags: arrays
:type: simulation
:lrm: 1800-2023 7.12.1 7.12.2 7.12.3 13.5.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_array_methods_matrix.py@1
:top_module: top
:expected_stdout: @array_methods_nan_fixed_initial.expected
*/
module top;
typedef real E;
E a[4:1];
E r[$];
initial begin
a[4] = $bitstoreal(64'h7ff8000000000001);
a[3] = $bitstoreal(64'h4008000000000000);
a[2] = $bitstoreal(64'hc000000000000000);
a[1] = $bitstoreal(64'h3ff0000000000000);
r = a.min(); $display("n0_min %0d %016h", r.size(), $realtobits(r[0]));
r = a.max(); $display("n0_max %0d %016h", r.size(), $realtobits(r[0]));
a[4] = $bitstoreal(64'h7ff8000000000001);
a[3] = $bitstoreal(64'h4008000000000000);
a[2] = $bitstoreal(64'hc000000000000000);
a[1] = $bitstoreal(64'h3ff0000000000000);
a.sort(); $write("n0_sort");
$write(" %016h", $realtobits(a[4]));
$write(" %016h", $realtobits(a[3]));
$write(" %016h", $realtobits(a[2]));
$write(" %016h", $realtobits(a[1]));
$write("\n");
a[4] = $bitstoreal(64'h7ff8000000000001);
a[3] = $bitstoreal(64'h4008000000000000);
a[2] = $bitstoreal(64'hc000000000000000);
a[1] = $bitstoreal(64'h3ff0000000000000);
a.rsort(); $write("n0_rsort");
$write(" %016h", $realtobits(a[4]));
$write(" %016h", $realtobits(a[3]));
$write(" %016h", $realtobits(a[2]));
$write(" %016h", $realtobits(a[1]));
$write("\n");
a[4] = $bitstoreal(64'h4008000000000000);
a[3] = $bitstoreal(64'h7ff8000000000001);
a[2] = $bitstoreal(64'hc000000000000000);
a[1] = $bitstoreal(64'h3ff0000000000000);
r = a.min(); $display("n1_min %0d %016h", r.size(), $realtobits(r[0]));
r = a.max(); $display("n1_max %0d %016h", r.size(), $realtobits(r[0]));
a[4] = $bitstoreal(64'h4008000000000000);
a[3] = $bitstoreal(64'h7ff8000000000001);
a[2] = $bitstoreal(64'hc000000000000000);
a[1] = $bitstoreal(64'h3ff0000000000000);
a.sort(); $write("n1_sort");
$write(" %016h", $realtobits(a[4]));
$write(" %016h", $realtobits(a[3]));
$write(" %016h", $realtobits(a[2]));
$write(" %016h", $realtobits(a[1]));
$write("\n");
a[4] = $bitstoreal(64'h4008000000000000);
a[3] = $bitstoreal(64'h7ff8000000000001);
a[2] = $bitstoreal(64'hc000000000000000);
a[1] = $bitstoreal(64'h3ff0000000000000);
a.rsort(); $write("n1_rsort");
$write(" %016h", $realtobits(a[4]));
$write(" %016h", $realtobits(a[3]));
$write(" %016h", $realtobits(a[2]));
$write(" %016h", $realtobits(a[1]));
$write("\n");
a[4] = $bitstoreal(64'h4008000000000000);
a[3] = $bitstoreal(64'hc000000000000000);
a[2] = $bitstoreal(64'h3ff0000000000000);
a[1] = $bitstoreal(64'h7ff8000000000001);
r = a.min(); $display("n2_min %0d %016h", r.size(), $realtobits(r[0]));
r = a.max(); $display("n2_max %0d %016h", r.size(), $realtobits(r[0]));
a[4] = $bitstoreal(64'h4008000000000000);
a[3] = $bitstoreal(64'hc000000000000000);
a[2] = $bitstoreal(64'h3ff0000000000000);
a[1] = $bitstoreal(64'h7ff8000000000001);
a.sort(); $write("n2_sort");
$write(" %016h", $realtobits(a[4]));
$write(" %016h", $realtobits(a[3]));
$write(" %016h", $realtobits(a[2]));
$write(" %016h", $realtobits(a[1]));
$write("\n");
a[4] = $bitstoreal(64'h4008000000000000);
a[3] = $bitstoreal(64'hc000000000000000);
a[2] = $bitstoreal(64'h3ff0000000000000);
a[1] = $bitstoreal(64'h7ff8000000000001);
a.rsort(); $write("n2_rsort");
$write(" %016h", $realtobits(a[4]));
$write(" %016h", $realtobits(a[3]));
$write(" %016h", $realtobits(a[2]));
$write(" %016h", $realtobits(a[1]));
$write("\n");
end
endmodule
