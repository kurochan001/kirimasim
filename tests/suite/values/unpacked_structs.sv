/*
:name: unpacked_structs
:description: Members of unpacked structures of every kind of member type are read and written like variables, in static and automatic variables, in packages, through hierarchical names and in event controls; whole structures are copied and assigned from patterns, and members take their default values only when the declaration has no initial value
:tags: values
:type: simulation
:lrm: 1800-2023 7.2 7.2.1 7.2.2 10.9.2 6.8
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @unpacked_structs.expected
*/
// No pattern below reads the structure it is assigned to. Icarus Verilog
// 13.0 does not accept unpacked structures. Verilator 5.052 matches the
// other lines, but shows the logic and enum members of "fresh" as 0 (it
// has two states), leaves the string member of the package's structure
// empty although its type gives it a default (7.2.2), and on "l2 default"
// and "l2 type key" sets only the first member of the nested structures,
// where 10.9.2 applies the type and default keys to every member of an
// unmatched nested structure.
package cfg_pkg;
  typedef struct {
    int width = 8;
    string tag = "cfg";
  } cfg_t;
  cfg_t cfg;
endpackage

module child;
  typedef struct { logic [7:0] v; int n; } pair_t;
  pair_t st;
endmodule

module top;
  typedef enum logic [1:0] { IDLE, RUN, DONE } state_t;
  typedef struct packed { logic [3:0] hi; logic [3:0] lo; } nib_t;
  typedef struct {
    int x;
    int y;
  } point_t;
  typedef struct {
    point_t a;
    point_t b;
    logic [7:0] color;
  } line_t;
  typedef struct {
    logic [7:0] v;
    int n;
    string name;
    real r;
    state_t st;
    nib_t p;
    byte bytes [4];
  } all_t;
  typedef struct {
    int id = 7;
    logic [3:0] flags = 4'b1010;
    point_t at;
    int plain;
  } dflt_t;

  all_t s, t, fresh;
  line_t l1, l2;
  dflt_t d1;
  dflt_t d2 = '{id: 1, flags: 4'b0001, at: '{3, 4}, plain: 5};
  point_t pt = '{10, 20};
  child u ();
  int wakes;
  logic [7:0] comb_v;

  always @(s.n) wakes++;
  always_comb comb_v = t.v ^ 8'hff;

  task automatic auto_task(int k);
    dflt_t local_d;
    point_t q = '{k, k + 1};
    $display("auto %0d: id=%0d flags=%b at=(%0d,%0d) q=(%0d,%0d)", k, local_d.id,
             local_d.flags, local_d.at.x, local_d.at.y, q.x, q.y);
    local_d.id = 99;
    q.y = q.x * 10;
    $display("auto %0d after: id=%0d q=(%0d,%0d)", k, local_d.id, q.x, q.y);
  endtask

  task static copy_task();
    l2 = l1;
    l2.b.y = 77;
    $display("task: l1.b.y=%0d l2.b.y=%0d l2.a.x=%0d", l1.b.y, l2.b.y, l2.a.x);
  endtask

  initial begin
    // Defaults of the member types (6.8, table 6-7).
    $display("fresh: v=%b n=%0d name=\"%s\" r=%0.1f st=%0d bytes0=%0d", fresh.v, fresh.n,
             fresh.name, fresh.r, fresh.st, fresh.bytes[0]);
    // Member defaults only without an initializer (7.2.2).
    $display("d1: id=%0d flags=%b at=(%0d,%0d) plain=%0d", d1.id, d1.flags, d1.at.x, d1.at.y,
             d1.plain);
    $display("d2: id=%0d flags=%b at=(%0d,%0d) plain=%0d", d2.id, d2.flags, d2.at.x, d2.at.y,
             d2.plain);
    $display("pkg: width=%0d tag=%s", cfg_pkg::cfg.width, cfg_pkg::cfg.tag);
    cfg_pkg::cfg.width = 16;
    $display("pkg after: width=%0d", cfg_pkg::cfg.width);

    // Members of every kind.
    s.v = 8'h5a;
    s.n = -3;
    s.name = "abc";
    s.r = 2.5;
    s.st = RUN;
    s.p = 8'h3c;
    s.p.lo = 4'h9;
    s.v[0] = 1'b1;
    s.v[7:4] = 4'h1;
    s.bytes[2] = 8'd42;
    s.bytes[3] = s.bytes[2] + 1;
    $display("s: v=%h n=%0d name=%s r=%0.1f st=%s p=%h hi=%h bytes=%0d,%0d",
             s.v, s.n, s.name, s.r, s.st.name(), s.p, s.p.hi, s.bytes[2], s.bytes[3]);
    s.name = {s.name, "d"};
    s.r += 1.0;
    s.n++;
    $display("s ops: name=%s len=%0d r=%0.1f n=%0d", s.name, s.name.len(), s.r, s.n);

    // A whole copy is a copy of every member, arrays included.
    t = s;
    t.n = 100;
    t.bytes[2] = 0;
    $display("copy: s.n=%0d t.n=%0d s.bytes2=%0d t.bytes2=%0d t.bytes3=%0d t.name=%s",
             s.n, t.n, s.bytes[2], t.bytes[2], t.bytes[3], t.name);

    // Patterns: by position, by name, with default and type keys.
    l1 = '{'{1, 2}, '{3, 4}, 8'hc0};
    $display("l1: a=(%0d,%0d) b=(%0d,%0d) color=%h", l1.a.x, l1.a.y, l1.b.x, l1.b.y, l1.color);
    l1 = '{color: 8'h0f, b: '{x: 30, y: 40}, a: pt};
    $display("l1 named: a=(%0d,%0d) b=(%0d,%0d) color=%h", l1.a.x, l1.a.y, l1.b.x, l1.b.y,
             l1.color);
    l2 = '{default: 5};
    $display("l2 default: a=(%0d,%0d) b=(%0d,%0d) color=%0d", l2.a.x, l2.a.y, l2.b.x, l2.b.y,
             l2.color);
    l2 = '{int: 6, default: 8'hee};
    $display("l2 type key: a=(%0d,%0d) color=%h", l2.a.x, l2.a.y, l2.color);
    l2.a = l1.b;
    $display("l2 member copy: a=(%0d,%0d)", l2.a.x, l2.a.y);
    pt = '{2{9}};
    $display("pt replicated: (%0d,%0d)", pt.x, pt.y);
    copy_task();

    // Nonblocking: the members change at the end of the time step.
    pt <= '{x: 1, y: 2};
    $display("nba before: (%0d,%0d)", pt.x, pt.y);
    #1 $display("nba after: (%0d,%0d)", pt.x, pt.y);

    // Automatic variables get their defaults at each entry.
    auto_task(1);
    auto_task(2);

    // Event controls on members.
    wakes = 0;
    s.n = 5;
    #1 s.v = 8'h00;
    #1 s.n = 6;
    #1 $display("wakes=%0d comb_v=%h", wakes, comb_v);

    // A member of a child's structure, by its hierarchical name.
    u.st.v = 8'h77;
    u.st.n = u.st.v + 1;
    $display("child: v=%h n=%0d", u.st.v, u.st.n);

    fork
      begin
        l1.a.x = 500;
        #1 $display("fork: l1.a.x=%0d", l1.a.x);
      end
    join
  end
endmodule
