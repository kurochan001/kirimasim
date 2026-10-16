/*
:name: string_methods
:description: String concatenation, replication, comparison, indexing and the string methods, with compare giving -1, 0 or 1
:tags: values string
:type: simulation
:lrm: 1800-2023 6.16 11.4.12.2
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @string_methods.expected
*/
// compare and icompare only promise a negative, zero or positive result
// (6.16.6); kirimasim returns -1, 0 or 1 (VALUE_SEMANTICS.md 7.6), so the
// second number on the compare line is -1, where the output of Verilator
// 5.052 has the difference of the first differing characters (-16). The
// other lines match that output.
module top;
  string s, t, u;
  byte b;
  initial begin
    s = "hello";
    t = {s, " ", "world"};
    $display("%s|%0d|[%8s]", t, t.len(), s);
    u = {3{"ab"}};
    $display(u);
    b = s[1];
    $display("%c %0d %0d", b, s.getc(4), s[9]);
    s[0] = "J";
    $display("%s %s", s, s.toupper());
    u = "abc";
    $display("%0d %0d %0d", s.compare("Jello"), s.compare("Zz"), u.icompare("ABC"));
    $display("%s|%s|", t.substr(6, 10), t.substr(5, 2));
    s = "1_23x";
    $display("%0d", s.atoi());
    s.hextoa(255);
    $display("%s %b %b", s, s == "ff", s < "fg");
  end
endmodule
