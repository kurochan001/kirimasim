// ref arguments, array arguments and automatic arrays (SIR_SPEC.md 7.4,
// 8): ref frame variables, ref.make and the ref instructions, array
// operands, and element instructions on frame variables.
module top;
  int s;
  int sa [0:3];
  function automatic void add(ref int v, input int k);
    v = v + k;
    v[0] = 1'b1;
  endfunction
  function automatic int pick(const ref int a [0:3], input int i);
    return a[i];
  endfunction
  function automatic int total(int a [0:3]);
    int t [0:1];
    t[0] = a[0] + a[1];
    t[1][3:0] = a[2][3:0];
    return t[0] + t[1];
  endfunction
  initial begin
    add(s, 1);
    add(sa[2], 3);
    s = pick(sa, 1) + total(sa) + total('{1, 2, 3, 4});
  end
endmodule
