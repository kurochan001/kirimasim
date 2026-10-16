/*
:name: concat_target
:description: Concatenations as assignment targets: variables, bits, parts, array elements and nested concatenations, by blocking, nonblocking, continuous and assignment-operator assignments and through an output port
:tags: proc
:type: simulation
:lrm: 1800-2023 10.3.2 10.4 11.4.12 11.4.1 11.8.3 23.3.3
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @concat_target.expected
*/
// A concatenation target is one packed vector (11.4.12): the right-hand
// side is sized to its width by the assignment rules (11.8.3; a signed
// value is sign-extended) and its bits go to the parts from the most
// significant one.
module child (input logic [7:0] i, output logic [7:0] o);
  assign o = i + 8'd1;
endmodule

module top;
  logic [3:0] a, b;
  logic c;
  logic [7:0] v;
  int arr [4];
  int calls;
  logic [1:0] n1, n2, n3;
  wire [2:0] w1;
  wire [4:0] w2;
  logic [7:0] src;
  logic [3:0] p1, p2;

  function int idx();
    calls++;
    return 1;
  endfunction

  assign {w1, w2} = src;
  child u (.i(src), .o({p1, p2}));

  task static in_task();
    static logic [2:0] x;
    static logic [4:0] y;
    {x, y} = 8'b101_11001;
    $display("task %b %b", x, y);
  endtask

  initial begin
    {a, b} = 8'hA5;
    $display("whole %h %h", a, b);
    {c, a, b} = 9'h1FF;
    $display("three %b %h %h", c, a, b);
    {a, b} = 4'sb1001;
    $display("signed %b %b", a, b);
    {a, b} = 12'hABC;
    $display("wider %h %h", a, b);
    v = 8'h00;
    {v[7], v[3:0]} = 5'b1_0110;
    $display("bits %b", v);
    {a, {b, c}} = 9'b1100_0011_1;
    $display("nested %b %b %b", a, b, c);
    arr[0] = 0; arr[1] = 0; arr[2] = 0; arr[3] = 0;
    calls = 0;
    {arr[idx()], arr[3]} = {32'd7, 32'd9};
    $display("elem %0d %0d %0d %0d calls %0d", arr[0], arr[1], arr[2], arr[3], calls);
    {a, b} = 8'hFF;
    {a, b} += 1;
    $display("op %h %h", a, b);
    {n1, n2, n3} = 6'b01_10_11;
    {n1, n2, n3} <= {n3, n1, n2};
    $display("nba before %b %b %b", n1, n2, n3);
    #1 $display("nba after %b %b %b", n1, n2, n3);
    {a, b} = 8'b1x0z_0110;
    $display("xz %b %b", a, b);
    src = 8'b110_10101;
    #1 $display("cont %b %b", w1, w2);
    $display("port %h %h", p1, p2);
    in_task();
  end
endmodule
