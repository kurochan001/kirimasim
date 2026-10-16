/*
:name: streaming_unpacked
:description: Streaming of fixed-size unpacked arrays and unpacked structures, elements in foreach order and members in declaration order, as values and as targets, with >> and with << in blocks and bits; a stream assigned to an unpacked array left-aligned with zeros on the right; an unpacked array as the source of an unpacking; and unpacking with << when the last block is short, the inverse of packing
:tags: values
:type: simulation
:lrm: 1800-2023 11.4.14.1 11.4.14.2 11.4.14.3
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @streaming_unpacked.expected
*/
// The rules are VALUE_SEMANTICS.md 8. Icarus Verilog 13.0 accepts no
// streaming concatenations. Verilator 5.052 does not compile the unpacking
// into the structure; without those two lines it matches the others except
// two: for "into array" it puts the stream at the right end, where 11.4.14
// fills with zeros on the right, and for "short block" it puts 6'b111001
// into s6, which packs back to 6'b100111, not to the source.
module top;
  typedef struct { bit [3:0] a; bit [7:0] b; bit [3:0] c; } rec_t;
  bit [7:0] bytes [4];
  bit [7:0] down [3:0];
  bit [3:0] grid [2][2];
  rec_t r;
  bit [31:0] w;
  bit [15:0] h;
  bit [5:0] s6;
  bit [7:0] p1, p2, p3, p4;
  initial begin
    bytes = '{8'h12, 8'h34, 8'h56, 8'h78};
    w = {>>{bytes}};
    $display("pack %h", w);
    w = {<<8{bytes}};
    $display("pack bytes reversed %h", w);
    down = '{8'ha1, 8'hb2, 8'hc3, 8'hd4};
    w = {>>{down}};
    $display("descending %h", w);
    grid = '{'{4'h1, 4'h2}, '{4'h3, 4'h4}};
    h = {>>{grid}};
    $display("2-D %h", h);
    r = '{4'ha, 8'hbc, 4'hd};
    h = {>>{r}};
    $display("struct %h", h);
    h = {<<4{r}};
    $display("struct nibbles reversed %h", h);
    {>>{bytes}} = 32'hdeadbeef;
    $display("unpack %h %h %h %h", bytes[0], bytes[1], bytes[2], bytes[3]);
    {<<8{bytes}} = 32'h01020304;
    $display("unpack reversed %h %h %h %h", bytes[0], bytes[1], bytes[2], bytes[3]);
    {>>{r}} = 16'h1234;
    $display("unpack struct %h %h %h", r.a, r.b, r.c);
    {>>{h, r}} = 32'hcafe_9876;
    $display("unpack mixed %h %h %h %h", h, r.a, r.b, r.c);
    bytes = {>>{16'habcd}};
    $display("into array %h %h %h %h", bytes[0], bytes[1], bytes[2], bytes[3]);
    down = '{8'h11, 8'h22, 8'h33, 8'h44};
    {>>{p1, p2, p3, p4}} = down;
    $display("array source %h %h %h %h", p1, p2, p3, p4);
    {<<4{s6}} = 6'b011110;
    p1 = {<<4{s6}};
    $display("short block %b packs back to %b", s6, p1[7:2]);
  end
endmodule
