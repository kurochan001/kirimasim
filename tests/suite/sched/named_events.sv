/*
:name: named_events
:description: -> wakes only the processes already waiting on the event, ->> triggers in the NBA region of the same or a later time slot, triggered holds for the rest of the time slot, on events of the module, of a package, of arrays and of a child instance
:tags: sched
:type: simulation
:lrm: 1800-2023 15.5.1 15.5.2 15.5.3 9.4.2
:oracle: spec iverilog@13.0 verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @named_events.expected
*/
// Each case is laid out so that no two processes race on the order of a
// trigger and a wait in the same time slot (15.5.2 leaves that order to
// the scheduler): a waiter starts waiting a time step before the trigger,
// or one process both triggers and waits.
//
// Icarus 13.0 matches the lines it accepts: it reports event arrays and
// ->> as not supported and rejects triggered. Verilator 5.052 matches
// every line except those of ->>: it wakes no waiter on ->> at all, even
// one that has waited since time 0, which 15.5.1 requires (checked on a
// smaller case). The ->> lines rest on 15.5.1 alone.
package pk;
  event pe;
endpackage

module child;
  event ce;
  int hits;
  initial forever begin
    @ce;
    hits++;
  end
endmodule

module top;
  event e, f, g;
  event ea [0:2];
  event eb [3:1];
  event em [2][3];
  int count;
  child u ();

  // An always procedure that waits on an event loops like any other.
  always @(e) count++;

  // Waiters that start before the triggers below.
  initial begin
    @(ea[1]);
    $display("%0t ea[1] woke", $time);
    @(eb[3]);
    $display("%0t eb[3] woke", $time);
    @(em[1][2]);
    $display("%0t em[1][2] woke", $time);
    @(pk::pe);
    $display("%0t pk::pe woke", $time);
  end

  initial begin
    #1;
    // A trigger wakes the always procedure once; a second trigger in the
    // same statement sequence finds it not waiting yet (15.5.1).
    -> e;
    -> e;
    #1 $display("%0t count %0d", $time, count);
    -> e;
    #1 $display("%0t count %0d", $time, count);

    // triggered holds from the trigger to the end of the time slot (15.5.3).
    $display("%0t f.triggered %0d", $time, f.triggered);
    -> f;
    $display("%0t f.triggered %0d", $time, f.triggered);
    #0 $display("%0t f.triggered after #0 %0d", $time, f.triggered);
    #1 $display("%0t f.triggered next slot %0d", $time, f.triggered);

    // A process that triggers with -> and then waits misses its own
    // trigger; with ->> the trigger comes in the NBA region, after the
    // wait has begun (15.5.1, 15.5.2).
    fork
      begin
        -> g;
        @g;
        $display("%0t g woke from a later trigger", $time);
      end
      begin
        #3 -> g;
      end
    join
    ->> f;
    $display("%0t f.triggered after ->> %0d", $time, f.triggered);
    @f;
    $display("%0t f woke from its own ->>, triggered %0d", $time, f.triggered);
    ->> #2 f;
    @f;
    $display("%0t f woke from ->> #2", $time);

    // Arrays with constant indices, an event of a package and one of a
    // child instance.
    #1 -> ea[0];
    -> ea[2];
    #1 -> ea[1];
    #1 -> eb[3];
    #1 -> em[0][2];
    -> em[1][1];
    #1 -> em[1][2];
    #1 -> pk::pe;
    #1 -> u.ce;
    #1 -> u.ce;
    #1 $display("%0t u.hits %0d", $time, u.hits);
    $finish;
  end
endmodule
