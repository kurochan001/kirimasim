// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

class sample_box;
  int value;
endclass

module unsupported_property_ref;
  sample_box item;
  task automatic update(ref int target);
    target = 29;
  endtask
  initial begin
    item = new();
    update(item.value);
  end
endmodule
