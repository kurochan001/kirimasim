/*
:name: disable_named_blocks
:description: A disable statement inside a named block ends that block and goes on after it, out of every kind of loop, as a break or a continue, from nested blocks at several levels, in initial, always and final procedures
:tags: proc
:type: simulation
:lrm: 1800-2023 9.6.2 9.3.5 12.7
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @disable_named_blocks.expected
*/
module top;
  int i, n, sum, trace;
  logic go = 1'b0;

  always @(posedge go) begin : on_go
    n = 0;
    forever begin
      n++;
      if (n == 4) disable on_go;
    end
  end

  final begin
    begin : at_end
      sum = 0;
      for (i = 0; i < 10; i++) begin : body
        if (i % 2 == 1) disable body;  // continue
        if (i > 6) disable at_end;     // leaves both loops
        sum += i;
      end
      $display("final: not reached");
    end
    $display("final sum %0d", sum);
  end

  initial begin
    // break out of for, while, repeat, do-while and forever
    sum = 0;
    begin : b_for
      for (i = 0; i < 100; i++) begin
        if (i == 5) disable b_for;
        sum += i;
      end
    end
    $display("for: i=%0d sum=%0d", i, sum);
    n = 0;
    begin : b_while
      while (1) begin
        n += 3;
        if (n > 10) disable b_while;
      end
    end
    $display("while: n=%0d", n);
    n = 0;
    begin : b_repeat
      repeat (10) begin
        n++;
        if (n == 7) disable b_repeat;
      end
    end
    $display("repeat: n=%0d", n);
    n = 0;
    begin : b_do
      do begin
        n++;
        if (n == 2) disable b_do;
      end while (n < 9);
    end
    $display("do: n=%0d", n);
    n = 0;
    begin : b_forever
      forever begin
        #1 n++;
        if (n == 3) disable b_forever;
      end
    end
    $display("forever: n=%0d at %0t", n, $time);

    // continue: a block that is the body of the loop
    sum = 0;
    for (i = 0; i < 6; i++) begin : step
      if (i == 2 || i == 4) disable step;
      sum += 10 ** i;
    end
    $display("continue: sum=%0d", sum);

    // nested blocks: leave the outer, the middle, the inner
    trace = 0;
    begin : outer
      begin : middle
        begin : inner
          trace = trace * 10 + 1;
          disable middle;
          trace = trace * 10 + 9;
        end
        trace = trace * 10 + 8;
      end
      trace = trace * 10 + 2;
      begin : inner2
        trace = trace * 10 + 3;
        if (trace > 0) disable outer;
      end
      trace = trace * 10 + 7;
    end
    $display("nested: trace=%0d", trace);

    // nested loops: leave both from the inner one
    sum = 0;
    begin : both
      for (i = 0; i < 5; i++)
        for (n = 0; n < 5; n++) begin
          if (i == 2 && n == 3) disable both;
          sum++;
        end
    end
    $display("both loops: sum=%0d i=%0d n=%0d", sum, i, n);

    // in a case item
    n = 2;
    begin : sel
      case (n)
        1: $display("case 1");
        2: begin
          $display("case 2");
          disable sel;
        end
      endcase
      $display("after case: not reached");
    end

    go = 1'b1;
    #1 $display("always: n=%0d", n);
    $finish;
  end
endmodule
