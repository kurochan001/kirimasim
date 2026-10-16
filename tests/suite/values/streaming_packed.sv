/*
:name: streaming_packed
:description: Streaming concatenations of packed operands as values and as targets: >> keeps the order, << reverses blocks of a slice size given by a number or a type, sliced from the right with a short last block, nested streams, a wider target filled with zeros on the right, a wider source unpacked from its left end, four-state bits, part-select and member targets and nonblocking unpacking
:tags: values
:type: simulation
:lrm: 1800-2023 11.4.14 11.4.14.1 11.4.14.2 11.4.14.3
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @streaming_packed.expected
*/
// Icarus Verilog 13.0 and its development version do not accept streaming
// concatenations. Verilator 5.052 matches the other lines, but gives 0000
// for "nested", where the inner stream reverses the bits to 1110 and the
// outer one reverses its 2-bit blocks, sliced from the right, to 1011
// (11.4.14.2), and shows the four-state bits as 0 (it has two states).
module top;
  typedef struct packed { logic [7:0] hi; logic [7:0] lo; } pair_t;
  logic [31:0] w, r;
  logic [7:0] a, b, c;
  logic [5:0] s6;
  logic [3:0] n4;
  logic [39:0] wide;
  bit [15:0] two;
  pair_t p;
  initial begin
    w = 32'h1234_5678;
    r = {>>{w}};
    $display("keep %h", r);
    r = {<<{w}};
    $display("bits %h", r);
    r = {<<8{w}};
    $display("bytes %h", r);
    r = {<<byte{w}};
    $display("byte type %h", r);
    r = {<<16{w}};
    $display("halves %h", r);
    r = {>>8{w}};
    $display("ignored slice %h", r);
    s6 = {<<4{6'b10_0111}};
    $display("short block %b", s6);
    n4 = {<<2{{<<{4'b0111}}}};
    $display("nested %b", n4);
    a = 8'hab; b = 8'hcd; c = 8'hef;
    wide = {>>{a, b, c}};
    $display("padded %h", wide);
    wide = {<<8{a, b, c}};
    $display("padded reversed %h", wide);
    n4 = {<<{4'b1x0z}};
    $display("four-state %b", n4);
    // Unpacking.
    {>>{a, b}} = 16'h1357;
    $display("unpack %h %h", a, b);
    {<<8{a, b}} = 16'h1357;
    $display("unpack reversed %h %h", a, b);
    {>>{a}} = 16'h9abc;
    $display("unpack left end %h", a);
    {>>{w[31:24], w[23:0]}} = 32'hfedc_ba98;
    $display("unpack parts %h", w);
    {<<8{p.hi, p.lo}} = 16'h2468;
    $display("unpack members %h %h", p.hi, p.lo);
    {>>{two}} = 16'b1010_xz10_0000_0001;
    $display("unpack two-state %b", two);
    {<<8{r}} <= 32'haabb_ccdd;
    $display("nba before %h", r);
    #1 $display("nba after %h", r);
  end
endmodule
