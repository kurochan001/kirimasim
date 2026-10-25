// Decoding with casez patterns and packed structures: each iteration decodes
// an instruction word into the fields of a packed struct and updates a small
// register file by what it decodes.
// +n=<instructions> sets how many instructions run.
// bench-n: 5000000
// bench-expect: casez_struct n=5000000 r0=001937a3 r7=ba163ebf r15=04cd903d
module casez_struct;
  typedef struct packed {
    logic [3:0] op;
    logic [3:0] rd;
    logic [3:0] rs;
    logic [3:0] imm;
  } insn_t;

  int n;

  initial begin
    insn_t ins;
    logic [15:0] word;
    logic [31:0] regs[16];
    if (!$value$plusargs("n=%d", n)) n = 1000;
    for (int r = 0; r < 16; r++) regs[r] = 32'(r * 17);
    word = 16'hbeef;
    for (int i = 0; i < n; i++) begin
      word = {word[14:0], word[15] ^ word[13] ^ word[12] ^ word[10]};
      ins = word;
      casez (ins.op)
        4'b00??: regs[ins.rd] = regs[ins.rs] + 32'(ins.imm);
        4'b010?: regs[ins.rd] = regs[ins.rs] ^ regs[ins.rd];
        4'b0110: regs[ins.rd] = regs[ins.rs] << ins.imm;
        4'b0111: regs[ins.rd] = regs[ins.rs] >> ins.imm;
        4'b10??: regs[ins.rd] = regs[ins.rd] - regs[ins.rs];
        4'b110?: regs[ins.rd] = {regs[ins.rs][15:0], regs[ins.rd][31:16]};
        default: regs[ins.rd] = ~regs[ins.rd];
      endcase
    end
    $display("casez_struct n=%0d r0=%h r7=%h r15=%h", n, regs[0], regs[7], regs[15]);
    $finish;
  end
endmodule
