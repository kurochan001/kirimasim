// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One time slot with many current NBAs: measure the pending queue and arena,
// rather than the small per-clock working set of nba_shift.
// Oracle: original design, SCHEDULER_SPEC 13 / IEEE 1800-2023 10.4.2;
// spec_check: ai. The last issued assignment must be the last applied value.
// bench-n: 500000
// bench-expect: nba_burst n=500000 last=499999
module nba_burst;
  int n, value;
  initial begin
    if (!$value$plusargs("n=%d", n)) n = 500000;
    for (int i = 0; i < n; i++) value <= i;
    #1;
    $display("nba_burst n=%0d last=%0d", n, value);
    $finish(0);
  end
endmodule
