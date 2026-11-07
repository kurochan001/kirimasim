# Grounds of the P1 reference mismatches

[日本語](p1-reference-mismatch-grounds.ja.md). Status: **approved on 2026-11-07**. English is authoritative.

Condition 4 of the [closed exit list](../spec/P1_SCOPE.md#closed-exit-list-approved-2026-11-07) asks for a stated ground for each probe whose Icarus output differs from the expected value. Measured at commit `60c26d7c` on 2026-11-07, 25 probes differ against Icarus Verilog 13.0, and kirimasim gives the expected value on all of them. In the tables, ` / ` separates lines of output.

The grounds are summaries in our own words. Four rest on a rule of the standard. In the other 21 the standard leaves a choice, or the project's contract settles the form, so **a difference there does not show that Icarus 13.0 is wrong**.

## Rules of the standard (4)

| Probe | Expected | Icarus 13.0 | Ground |
| --- | --- | --- | --- |
| `nested_packed_select_bounds` | `5580` | `5780` | 7.4.5, 11.5.1, [VALUE_SEMANTICS 3.11](../spec/VALUE_SEMANTICS.md#311-bit--and-part-selects): a part partly outside the element `a[0]` writes only the bits inside it, so `a[1]` stays `8'h55` |
| `class_virtual_dispatch` | `9` | `3` | 8.20: a method stays virtual in subclasses once it is virtual, so through a base handle the derived object's method runs |
| `dynamic_array_queries` | `3 0 2 -1` | `3 0 2 2` | 20.7: for a dynamic array dimension `$increment` returns -1 |
| `blocking_repeated_event_one_bit_unknown` | `7 9` | `0 0` | 9.4.5: a repeat count that is unknown or high impedance acts as if there were no repeat, so the assignment does not wait |

## Collapsed net ports (11)

SIR_SPEC 3.5 records that merging input and output connections is optional, and that values can differ with an implementation that does not merge.

| Probe | Expected | Icarus 13.0 | Ground |
| --- | --- | --- | --- |
| `partial_port_wand` | `0 0 / 1 1 / z z / 0 0` | `0 0 / 1 z / z z / 0 1` | 23.3.3.7, 28.12.4, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_triand` | `0 0 / 1 1 / z z / 0 0` | `0 0 / 1 z / z z / 0 1` | 23.3.3.7, 28.12.4, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_wor` | `1 1 / 1 1 / z z / 1 1` | `1 0 / 1 z / z z / 1 1` | 23.3.3.7, 28.12.4, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_trior` | `1 1 / 1 1 / z z / 1 1` | `1 0 / 1 z / z z / 1 1` | 23.3.3.7, 28.12.4, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_tri0_inner` | `0 0 / 1 1 / 0 0` | `0 0 / 1 0 / 0 0` | 23.3.3.7, 28.15, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_tri0_outer` | `0 0 / 1 1 / 0 0` | `0 z / 1 z / 0 z` | 23.3.3.7, 28.15, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_tri1_inner` | `1 1 / 0 0 / 1 1` | `1 1 / 0 1 / 1 1` | 23.3.3.7, 28.15, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_tri1_outer` | `1 1 / 0 0 / 1 1` | `1 z / 0 z / 1 z` | 23.3.3.7, 28.15, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_supply0_outer` | `0 0 / 0 0 / 0 0` | `0 z / 0 z / 0 z` | 23.3.3.7, 28.15, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_supply1_outer` | `1 1 / 1 1 / 1 1` | `1 z / 1 z / 1 z` | 23.3.3.7, 28.15, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |
| `partial_port_array_rank2` | `x x z / 0 0 z / z z z` | `x 0 z / 0 0 z / z z z` | 23.3.3.7, 7.4, [SIR_SPEC 3.5](../spec/SIR_SPEC.md#35-collapsing-ports), [MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability): the standard permits merging the nets, and the project merges them |

## Ports with a conversion (6)

| Probe | Expected | Icarus 13.0 | Ground |
| --- | --- | --- | --- |
| `converting_port_input_4_2` | `11 0011` | `xx 00xx` | 4.9.6, 23.3.3.7, [conversion note](converting-port-execution.md): a port with a conversion stays a directed implicit assignment with a strong value |
| `converting_port_input_2_4` | `1111 11` | `11xx xx` | 4.9.6, 23.3.3.7, [conversion note](converting-port-execution.md): a port with a conversion stays a directed implicit assignment with a strong value |
| `converting_port_input_2_4_signed` | `1111 11` | `11xx xx` | 4.9.6, 23.3.3.7, [conversion note](converting-port-execution.md): a port with a conversion stays a directed implicit assignment with a strong value |
| `converting_port_input_3_3_signed_actual_unsigned` | `111 111` | `xxx xxx` | 4.9.6, 23.3.3.7, [conversion note](converting-port-execution.md): a port with a conversion stays a directed implicit assignment with a strong value |
| `converting_port_output_3_3_signed_actual_unsigned` | `000 000` | `xxx xxx` | 4.9.6, 23.3.3.7, [conversion note](converting-port-execution.md): a port with a conversion stays a directed implicit assignment with a strong value |
| `converting_port_input_3_3_cast` | `111 111` | `111 xxx` | 4.9.6, 23.3.3.7, [conversion note](converting-port-execution.md): a port with a conversion stays a directed implicit assignment with a strong value |

## Retained partial inout ports (4)

| Probe | Expected | Icarus 13.0 | Ground |
| --- | --- | --- | --- |
| `partial_inout_port_wand` | `0z 0 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7, 28.12, [partial inout note](partial-inout-port-execution.md): a retained net-to-net inout is an identity connection without attenuation |
| `partial_inout_port_triand` | `0z 0 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7, 28.12, [partial inout note](partial-inout-port-execution.md): a retained net-to-net inout is an identity connection without attenuation |
| `partial_inout_port_wor` | `1z 1 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7, 28.12, [partial inout note](partial-inout-port-execution.md): a retained net-to-net inout is an identity connection without attenuation |
| `partial_inout_port_trior` | `1z 1 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7, 28.12, [partial inout note](partial-inout-port-execution.md): a retained net-to-net inout is an identity connection without attenuation |

## Not confirmed

- Whether condition 4 holds. This table is its record, and the M8 report judges it.
- Whether the Icarus output would differ on another platform. These are the results on macOS arm64.
