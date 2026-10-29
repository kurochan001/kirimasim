# Compile-time validation of `$bits` function calls

Issue [#53](https://github.com/kurochan001/kirimasim/issues/53) exposed a
missing error check: a call returning a dynamic array could be evaluated by
the pinned front end when enclosed by `$bits`, producing an apparent size.
IEEE 1800-2023 20.6.2 requires a diagnostic for a dynamically sized function
return inside the enclosed expression. This supplements the existing
[conversion/query contract](../spec/SYSTEM_TASKS.md#8-conversion-and-queries);
it does not change that contract or the dependency pin.

The shared front check runs after successful elaboration and analysis, before
lowering. Both `parseAndElaborate` and `compile` count these as language errors.
The command-line parse, execution and SIR-dump paths therefore return code 2,
without running the design or producing SIR. Each offending query is reported
once at its source position, including when several instances share that
position or several dynamic calls occur inside one query.

The checker uses types rather than computing a replacement size. It inspects
the query's argument expression, including nested function and system-function
calls, while keeping the body of a called function outside that expression.
String and dynamic-array, queue and associative-array returns are dynamic;
fixed arrays, records, unions and class properties are checked recursively.
The class traversal handles inheritance and cycles. Event, chandle and
interface handles are not classified as dynamic arrays or strings.

Original expressions retained by elaboration cover initializers, subroutine
bodies, parameter overrides, defparams and generate controls. For dimensions
and size casts whose bound values have replaced their expressions, only the
query argument is bound to recover its type. The complete query and its
argument value are not evaluated by this check. The declaration's lexical
scope is preserved; overridden parameters use the instantiation's scope.

Original regressions check errors and source positions through all three CLI
paths, unevaluated fixed-size queries, nested calls, opaque handles, classes,
folded contexts and caller/callee shadowing. API tests distinguish language
errors from unsupported lowering and check per-position diagnostic counts.
Existing source bodies, output expectations and performance expectations are
kept. Runtime query support for dynamic variables retains its existing scope.
