/*
:name: partial_port_supply_chain
:description: Chained partial ports retain physical supply-strength roots without switch attenuation or feedback
:tags: nets strength ports
:type: simulation
:lrm: 1800-2023 23.3.3.7 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @partial_port_supply_chain.expected
*/
module supply_source(output wire [1:0] o);
  assign (supply1, supply0) o = 2'b10;
endmodule
module bridge(output wire [3:0] bus);
  supply_source leaf(bus[2:1]);
endmodule
module top;
  wire [7:0] bus;
  bridge middle(bus[6:3]);
  assign (strong1, strong0) bus[5] = 0;
  assign (weak1, weak0) bus[4] = 1;
  initial begin
    #1;
    if (bus !== 8'bzz10zzzz || middle.bus !== 4'bz10z || middle.leaf.o !== 2'b10)
      $fatal(1, "port identity changed supply strength");
    $display("PASS");
  end
endmodule
