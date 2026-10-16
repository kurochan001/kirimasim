/*
:name: short_circuit
:description: &&, || and -> skip their right operand, with its side effects, when the left one decides, on four-state and two-state values; <-> and an x left operand evaluate both; ?: evaluates the side its condition picks, both when the condition is x or z
:tags: values
:type: simulation
:lrm: 1800-2023 11.3.5 11.4.7 11.4.11
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @short_circuit.expected
*/
// The right operand calls a function that counts its calls (11.3.5). A
// left operand that is x or z decides nothing, so the right one is
// evaluated. Icarus 13.0 evaluates the right operand of -> even when the
// left one is false (the "-> 00" line, and the count of the "2state 0"
// line); 11.3.5 names -> among
// the operators that short-circuit. The other lines match its output.
module top;
  int calls, i;
  logic r;
  logic [1:0] vals [6];

  function automatic logic f(logic v);
    calls++;
    return v;
  endfunction

  function automatic bit h(bit v);
    calls++;
    return v;
  endfunction

  function automatic int g(int v);
    calls++;
    return v;
  endfunction

  initial begin
    vals[0] = 2'b00; vals[1] = 2'b01; vals[2] = 2'b0x; vals[3] = 2'b0z; vals[4] = 2'b10;
    vals[5] = 2'b1x;
    for (int k = 0; k < 6; k++) begin
      calls = 0; r = vals[k] && f(1'b1); $display("&& %b %b %0d", vals[k], r, calls);
      calls = 0; r = vals[k] || f(1'b0); $display("|| %b %b %0d", vals[k], r, calls);
      calls = 0; r = vals[k] -> f(1'b0); $display("-> %b %b %0d", vals[k], r, calls);
      calls = 0; r = vals[k] <-> f(1'b1); $display("<-> %b %b %0d", vals[k], r, calls);
      calls = 0; i = vals[k] ? g(7) : g(9); $display("?: %b %0d %0d", vals[k], i, calls);
    end
    calls = 0;
    if (vals[0] && f(1'b1)) $display("if taken");
    else $display("if not taken %0d", calls);
    calls = 0;
    i = 0;
    while (i < 3 && f(1'b1)) i++;
    $display("while %0d %0d", i, calls);
    calls = 0;
    r = vals[1] && (vals[0] || f(1'b1));
    $display("nested %b %0d", r, calls);
    for (int n = 0; n < 2; n++) begin
      bit b;
      b = n[0];
      calls = 0;
      $display("2state %b %b %b %b %0d", b, b && h(1'b1), b || h(1'b0), b -> h(1'b0), calls);
    end
  end
endmodule
