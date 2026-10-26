// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Class-runtime contract 2; RUNTIME_ABI 9.8: static storage belongs to Context.
package shared_state;
  class Values;
    static int counter = 7;
    static int items[$];
    static string label = "fresh";
    static event changed;
  endclass
endpackage

module top;
  initial begin
    $display("counter=%0d size=%0d label=%s triggered=%0d",
             shared_state::Values::counter, shared_state::Values::items.size(),
             shared_state::Values::label, shared_state::Values::changed.triggered);
    ++shared_state::Values::counter;
    shared_state::Values::items.push_back(9);
    shared_state::Values::label = "changed";
    -> shared_state::Values::changed;
  end
endmodule
