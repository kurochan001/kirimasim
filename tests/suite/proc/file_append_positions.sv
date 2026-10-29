/*
:name: file_append_positions
:description: Append output updates the position to EOF after seeks and pushed-back input; ordinary update files retain sparse seeking
:tags: proc
:type: simulation
:lrm: 1800-2023 21.3.1 21.3.4.1 21.3.4.3 21.3.5
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:top_module: top
:expected_stdout: @file_append_positions.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  task seed_file;
    integer fd;
    fd = $fopen("append_positions.dat", "w");
    if (fd == 0) $fatal(1, "cannot create fixture");
    $fwrite(fd, "abc");
    $fclose(fd);
  endtask

  task seek_to(input integer fd, offset, origin);
    integer status;
    status = $fseek(fd, offset, origin);
    if (status != 0) $fatal(1, "seek failed: %0d %0d", offset, origin);
  endtask

  task append_mode(input string mode);
    integer fd, first, second, relative_pos, fourth, beyond, last, count;
    reg [71:0] bytes_read;
    seed_file();
    fd = $fopen("append_positions.dat", mode);
    if (fd == 0) $fatal(1, "cannot open %s", mode);
    seek_to(fd, 0, 0);
    $fwrite(fd, "XY");
    first = $ftell(fd);  // Check buffered output before any flush or seek.
    $fwrite(fd, "Z");
    second = $ftell(fd);
    $fwrite(fd, "!");
    seek_to(fd, -2, 1);  // Relative seek directly after another buffered write.
    relative_pos = $ftell(fd);
    $fwrite(fd, "Q");
    fourth = $ftell(fd);
    seek_to(fd, 20, 0);
    beyond = $ftell(fd);
    $fwrite(fd, "R");  // Append has no gap even after seeking beyond EOF.
    last = $ftell(fd);
    $fclose(fd);
    fd = $fopen("append_positions.dat", "rb");
    if (fd == 0) $fatal(1, "cannot reopen fixture");
    count = $fread(bytes_read, fd);
    $fclose(fd);
    $display("append %s: %0d %0d %0d %0d %0d %0d; bytes=%0d %h",
             mode, first, second, relative_pos, fourth, beyond, last, count, bytes_read);
  endtask

  task append_update(input string mode);
    integer fd, first_read, pushed, pos_before, pos_after, end_read, at_end;
    integer second_read, second_push, second_write, relative_pos, tail0, tail1, count;
    reg [47:0] bytes_read;
    seed_file();
    fd = $fopen("append_positions.dat", mode);
    if (fd == 0) $fatal(1, "cannot open %s", mode);
    seek_to(fd, 0, 0);  // The initial read position of a+ is host-dependent.
    first_read = $fgetc(fd);
    pushed = $ungetc(88, fd);
    pos_before = $ftell(fd);
    $fwrite(fd, "D");
    pos_after = $ftell(fd);
    end_read = $fgetc(fd);  // The pushed byte was discarded by the write.
    at_end = $feof(fd);
    seek_to(fd, 0, 0);
    second_read = $fgetc(fd);
    second_push = $ungetc(89, fd);
    $fwrite(fd, "E");
    second_write = $ftell(fd);
    $fwrite(fd, "F");
    seek_to(fd, -2, 1);
    relative_pos = $ftell(fd);
    tail0 = $fgetc(fd);
    tail1 = $fgetc(fd);
    seek_to(fd, 0, 0);
    count = $fread(bytes_read, fd);
    $fclose(fd);
    $display("update %s: read=%0d push=%0d before=%0d write=%0d eof=%0d/%0d next=%0d push=%0d second=%0d cur=%0d tail=%0d,%0d bytes=%0d %h",
             mode, first_read, pushed, pos_before, pos_after, end_read, at_end,
             second_read, second_push, second_write, relative_pos, tail0, tail1, count, bytes_read);
  endtask

  task ordinary_update(input string mode);
    integer fd, first, overwritten, relative_pos, last, tail, count;
    reg [55:0] bytes_read;
    fd = $fopen("append_positions.dat", mode);
    if (fd == 0) $fatal(1, "cannot open %s", mode);
    $fwrite(fd, "abc");
    first = $ftell(fd);
    seek_to(fd, 0, 0);
    $fwrite(fd, "XY");
    overwritten = $ftell(fd);
    $fwrite(fd, "Z");
    seek_to(fd, 3, 1);
    relative_pos = $ftell(fd);
    $fwrite(fd, "Q");
    last = $ftell(fd);
    seek_to(fd, -1, 1);
    tail = $fgetc(fd);
    seek_to(fd, 0, 0);
    count = $fread(bytes_read, fd);
    $fclose(fd);
    $display("ordinary %s: %0d %0d %0d %0d tail=%0d bytes=%0d %h",
             mode, first, overwritten, relative_pos, last, tail, count, bytes_read);
  endtask

  initial begin
    append_mode("a");
    append_mode("ab");
    append_mode("a+");
    append_mode("a+b");
    append_mode("ab+");
    append_update("a+");
    append_update("a+b");
    append_update("ab+");
    ordinary_update("w+");
    ordinary_update("wb+");
  end
endmodule
