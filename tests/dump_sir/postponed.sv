// $strobe and $monitor (SIR_SPEC.md 7.1, 7.2): each is a postponed process
// started by strobe or monitor; the monitor's body shows its values the
// first time and when one is not the one it showed last, $time aside.
module top;
  int a;
  initial begin
    $strobe("%0d", a);
    $monitor("%0t %0d", $time, a);
    $monitoroff;
    $monitoron;
  end
endmodule
