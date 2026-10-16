/*
:name: string_literals_long
:description: String literals of any length become strings where a string is wanted: assigned, compared, concatenated, passed to and returned from subroutines, in initializers, array patterns and conditional operators, with zero characters dropped
:tags: values
:type: simulation
:lrm: 1800-2023 5.9 6.16 11.4.12.2
:oracle: spec iverilog@13.0 verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @string_literals_long.expected
*/
module top;
  string s = "a literal longer than eight characters";
  string arr [2] = '{"first long element", "second"};
  string r;
  function automatic string greet(string who = "everybody out there");
    return {"Hello, ", who, "!"};
  endfunction
  task automatic show(string label, string value);
    $display("%s: [%s] len=%0d", label, value, value.len());
  endtask
  initial begin
    show("init", s);
    show("array", arr[0]);
    r = "assigned in a procedure";
    show("assigned", r);
    show("argument", "passed straight as an argument");
    show("default", greet());
    show("returned", greet("kirimasim users"));
    if (r == "assigned in a procedure") show("compare", "equal strings");
    if (r != "assigned in a procedure!") show("compare not", "not equal");
    r = {"concatenated ", "literals only"};
    show("literal concat", r);
    r = 1 ? "a constant condition picks this" : "not this one";
    show("constant ?:", r);
    $display("%s|%0s", "a long literal printed with %s", "and with %0s too");
    show("concat", {s, " -- ", "and more text after it"});
    show("zero dropped", "zero\000inside the literal");
    r = "0123456789abcdef0123456789";
    show("index", $sformatf("%s %0d", r.substr(10, 15), r[25]));
  end
endmodule
