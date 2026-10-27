/*
:name: explicit_empty_input_default
:description: Explicitly empty named connections suppress defaults while omitted inputs use them
:tags: ports
:type: simulation
:lrm: 1800-2023 23.2.2.4 23.3.2.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @explicit_empty_input_default.expected
*/
module receiver(input wire [2:0] data=3'b101, output wire [2:0] seen);
  assign seen=data;
endmodule
`unconnected_drive pull1
module pulled(input wire [2:0] data=3'b101, output wire [2:0] seen);
  assign seen=data;
endmodule
`nounconnected_drive
module top;
  wire [2:0] omitted, empty, connected, pulled_empty, pulled_omitted;
  receiver a(.seen(omitted));
  receiver b(.data(), .seen(empty));
  receiver c(.data(3'b010), .seen(connected));
  pulled d(.data(), .seen(pulled_empty));
  pulled e(.seen(pulled_omitted));
  initial begin
    #1;
    $display("omitted=%b empty=%b connected=%b", omitted, empty, connected);
    $display("pulled_empty=%b pulled_omitted=%b", pulled_empty, pulled_omitted);
  end
endmodule
