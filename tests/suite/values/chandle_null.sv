/*
:name: chandle_null
:description: chandle variables start at null in every place (module, automatic, array element, structure member), compare with ==, !=, === and !== against each other and null, are false when null in conditions, and pass to and from functions
:tags: values
:type: simulation
:lrm: 1800-2023 6.14 6.8
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @chandle_null.expected
*/
module top;
  typedef struct { chandle h; int n; } holder_t;
  chandle s;
  chandle arr [0:2];
  holder_t hold;
  function automatic chandle pass(chandle c);
    chandle local_c;
    $display("in function: local %0d arg %0d", local_c == null, c == null);
    return c;
  endfunction
  initial begin
    automatic chandle a;
    chandle b;
    $display("defaults %0d %0d %0d %0d %0d", s == null, a == null, b == null, arr[1] == null,
             hold.h == null);
    $display("eq %0d ne %0d ceq %0d cne %0d", s == a, s != a, s === null, a !== null);
    b = s;
    $display("assigned %0d", b == null);
    if (s) $display("s is true");
    else $display("s is false");
    $display("not %0d ternary %0d", !s, s ? 1 : 2);
    a = pass(s);
    $display("returned %0d", a == null);
    a = null;
    arr[2] = a;
    $display("element %0d", arr[2] === null);
  end
endmodule
