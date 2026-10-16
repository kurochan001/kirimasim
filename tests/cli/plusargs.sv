// $test$plusargs (SYSTEM_TASKS.md 9, CLI.md 2.4): 1 when a plusarg starts
// with the string, including the empty string when there is any plusarg.
module top;
  initial $display("%0d %0d %0d %0d", $test$plusargs("mode"), $test$plusargs("mode=fa"),
                   $test$plusargs("nothere"), $test$plusargs(""));
endmodule
