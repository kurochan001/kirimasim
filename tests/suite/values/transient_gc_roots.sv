/*
:name: transient_gc_roots
:description: Transient collection preserves typed temporaries, detached cells and delayed string writes
:tags: values string queue gc
:type: simulation
:lrm: 1800-2023 6.16 7.10 10.4.2 13.5.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @transient_gc_roots.expected
*/
module top;
  string q[$];
  string pending;
  string watched;
  int sum;
  function automatic string churn(input int n);
    string scratch;
    for (int i=0; i<n; i++) scratch=$sformatf("discard=%0d",i);
    return scratch;
  endfunction
  function automatic string prefix();
    return $sformatf("left=%0d:",123);
  endfunction
  function automatic int size(ref string text); return text.len(); endfunction
  function automatic string detached(ref string held);
    string ignored;
    q.delete();
    ignored=churn(3000);
    return held;
  endfunction
  function automatic void copy_result(input string head, output string result);
    result={head,churn(3000)};
  endfunction
  initial begin
    string combined, ignored;
    combined={prefix(),churn(3000)};
    $display("combined=%s",combined);
    q.push_back($sformatf("kept=%0d",73));
    combined=detached(q[0]);
    $display("detached=%s size=%0d",combined,q.size());
    pending=$sformatf("pending=%0d",91);
    pending <= #5 pending;
    pending="overwritten";
    ignored=churn(3000);
    #6;
    $display("delayed=%s",pending);
    watched=$sformatf("watch=%0d",17);
    #1;
    ignored=churn(3000);
    watched=$sformatf("watch=%0d",18);
    #1;
    $display("watched=%s",watched);
    copy_result(prefix(),combined);
    $display("copy=%s",combined);
    fork
      begin
        automatic string retained=$sformatf("captured=%0d",42);
        #2;
        $display("fork=%s",retained);
      end
      begin
        ignored=churn(3000);
      end
    join
    for (int i=0; i<3000; i++) begin
      automatic string local_value=$sformatf("v=%0d",i);
      fork
        begin local_value={local_value,""}; end
      join
      sum+=size(local_value);
    end
    $display("scoped=%0d",sum);
  end
  initial begin
    #6;
    @(watched) $display("event=%s",watched);
  end
endmodule
