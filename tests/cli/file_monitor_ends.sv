// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// A file monitor ends when the last of its files closes (SYSTEM_TASKS.md
// 6.2). Each of the 100 calls makes a process and a frame; both go when the
// file closes, so the processes kept do not grow with the number of calls.
// tools/run_cli_tests.py checks proc.kept_peak of --stats=json on this
// design.
module top;
  int fd, x = 0;
  initial begin
    for (int i = 0; i < 100; i++) begin
      x = i;
      fd = $fopen("file_monitor_ends.txt", "w");
      $fmonitor(fd, "x=%0d", x);
      #1 $fclose(fd);
    end
    $display("done");
  end
endmodule
