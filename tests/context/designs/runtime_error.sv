// A run-time error of the design: an event triggered in the Postponed
// region, through a function called from $strobe's arguments (13.4.4).
`timescale 1ns/1ns
module runtime_error;
  event ev;
  function void touch();
    ->> ev;
  endfunction
  function int side();
    touch();
    return 7;
  endfunction
  initial begin
    $display("before");
    #1 $strobe("%0d", side());
  end
endmodule
