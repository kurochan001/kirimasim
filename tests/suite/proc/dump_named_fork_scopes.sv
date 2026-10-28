/*
:name: dump_named_fork_scopes
:description: A vector declaration before a nested named fork does not hide its VCD scope
:tags: proc
:type: simulation
:lrm: 1800-2023 21.7.2.3
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:top_module: top
:expected_stdout: begin=1 fork=1 task=1 vector=1 nested=1 balanced=1
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  task named_task;
    static int task_value;
    task_value = 7;
  endtask

  initial begin : named_begin
    static logic [3:0] nib = 4'h5;
    fork : named_fork
      begin
        static logic in_fork = 0;
        #2 in_fork = 1;
      end
    join
  end

  // Read whole lines, so the $end following [3:0] cannot consume the
  // next command, which declares the nested fork scope.
  task automatic check_dump;
    int fd, depth = 0;
    bit begin_ok = 0, fork_ok = 0, task_ok = 0, vector_ok = 0;
    bit nested = 0, balanced = 1;
    string line;
    fd = $fopen("named_fork.vcd", "r");
    if (fd == 0) $fatal(1, "missing VCD file");
    while ($fgets(line, fd) != 0) begin
      if (line.substr(0, 6) == "$scope ") begin
        depth++;
        if (line == "$scope begin named_begin $end\n") begin_ok = 1;
        if (line == "$scope fork named_fork $end\n") begin
          fork_ok = 1;
          nested = depth == 3;
        end
        if (line == "$scope task named_task $end\n") task_ok = 1;
      end else if (line == "$upscope $end\n") begin
        depth--;
        if (depth < 0) balanced = 0;
      end else if (line.substr(0, 10) == "$var reg 4 ") begin
        if (line.substr(line.len() - 15, line.len() - 1) == "nib [3:0] $end\n")
          vector_ok = 1;
      end
    end
    $fclose(fd);
    $display("begin=%0d fork=%0d task=%0d vector=%0d nested=%0d balanced=%0d",
             begin_ok, fork_ok, task_ok, vector_ok, nested, balanced && depth == 0);
  endtask

  initial begin
    named_task;
    $dumpfile("named_fork.vcd");
    $dumpvars(0, top);
    #3 $dumpflush;
    check_dump;
  end
endmodule
