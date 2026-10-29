// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

class sample_box;
  int value;
endclass

module unsupported_object;
  sample_box item;
  initial begin
    item = new();
    item.value = 1;
  end
endmodule
