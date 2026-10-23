#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Observe binary scanf behavior for the approved binary-scan rules.

These are original probes, not simulator tests copied from elsewhere.
The report records behavior; it does not declare a standards verdict.
Each design is compiled once, and tagged result lines prevent warnings
from being mistaken for a different case's result.
"""

import argparse
import json
import re
import signal
import sys
import tempfile
from pathlib import Path

import bounded_run


def cases():
    rows = []
    prefixes = (b"\x20\x09\x0a\x0d", b"\x00\x41\x00\x42", b"\x81\xff\x42\x00")
    for width in (4, 32, 33, 64, 65, 129):
        for conv in ("u", "z"):
            for field in ("", "0", "4", "33", "65"):
                for skip in (False, True):
                    for prefix in prefixes:
                        # Long enough for the largest field plus a continuation byte.
                        data = prefix + bytes(range(0x41, 0x41 + 60))
                        rows.append({"width": width, "format": (" " if skip else "") + "%" + field + conv,
                                     "data": data, "suppressed": False})
    for width in (4, 33, 65):
        for conv in ("u", "z"):
            needed = ((width + 31) // 32) * (4 if conv == "u" else 8)
            for length in range(needed + 1):
                rows.append({"width": width, "format": "%" + conv,
                             "data": bytes(range(0x41, 0x41 + length)), "suppressed": False})
    for conv in ("u", "z"):
        for field in ("", "0", "4", "33", "65"):
            rows.append({"width": 32, "format": "%*" + field + conv + "%c",
                         "data": bytes(range(0x41, 0x41 + 60)), "suppressed": True})
    return rows


def design(rows):
    lines = ["module top;", "  int fd,n,pos,e,c;",
             *[f"  logic [{w-1}:0] v{w};" for w in sorted({row['width'] for row in rows})],
             "  initial begin"]
    for i, row in enumerate(rows):
        width = row["width"]
        lines += [f'    fd=$fopen("data_{i}.bin","rb");',
                  f"    v{width}='hdeadbeef;",
                  f'    n=$fscanf(fd,"{row["format"]}",v{width});',
                  "    pos=$ftell(fd); e=$feof(fd); c=$fgetc(fd); $fclose(fd);",
                  f'    $display("BINARY_SCAN {i} %0d %0d %0d %h %0d",n,pos,e,v{width},c);']
    return "\n".join(lines + ["  end", "endmodule"]) + "\n"


def candidate(row):
    """The approved project rules' result, from Python integers, not simulator output."""
    fmt, data, width = row["format"], row["data"], row["width"]
    spec = re.search(r"%\*?(\d*)([uz])", fmt)
    bits = int(spec[1] or 0) or (32 if row["suppressed"] else width)
    step = 4 if spec[2] == "u" else 8
    start = 0
    if fmt.startswith(" "):
        while start < len(data) and data[start] in b" \t\r\n\f\v":
            start += 1
    needed = ((bits + 31) // 32) * step
    position = start + min(needed, len(data) - start)
    complete = len(data) - start >= needed
    a, b = 0xdeadbeef & ((1 << width) - 1), 0
    if complete:
        if row["suppressed"]:
            a = data[position]  # The following %c assigns the target.
            position += 1
        else:
            a = b = 0
            for low in range(0, bits, 32):
                offset = start + (low // 32) * step
                a |= int.from_bytes(data[offset:offset + 4], sys.byteorder) << low
                if step == 8:
                    b |= int.from_bytes(data[offset + 4:offset + 8], sys.byteorder) << low
            a &= (1 << min(bits, width)) - 1
            b &= (1 << min(bits, width)) - 1
    digits = []
    for low in reversed(range(0, width, 4)):
        mask = (1 << min(4, width - low)) - 1
        value, unknown = a >> low & mask, b >> low & mask
        digits.append(format(value, "x") if not unknown else
                      "x" if value == unknown == mask else
                      "z" if not value and unknown == mask else
                      "X" if value & unknown else "Z")
    return {"assigned": 1 if complete else -1, "position": position,
            "eof": 0 if complete else 1, "value_hex": "".join(digits),
            "next_byte": data[position] if position < len(data) else -1}


def scrub(data, root):
    text = data.decode(errors="replace").replace(str(root), "<probe>")
    return re.sub(r"^kirimasim: info: .*\n?", "", text, flags=re.M)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", type=Path, required=True)
    parser.add_argument("--iverilog", default="iverilog")
    parser.add_argument("--vvp", default="vvp")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--exclude-zero-width", action="store_true",
                        help="observe nonzero and omitted widths separately; record the excluded cases")
    args = parser.parse_args()
    rows = cases()
    excluded = []
    if args.exclude_zero_width:
        excluded = [row for row in rows if re.search(r"%\*?0[uz]", row["format"])]
        rows = [row for row in rows if not re.search(r"%\*?0[uz]", row["format"])]
    report = {"scope": "observations against approved project rules; no conformance verdict",
              "cases": [{**row, "data": row["data"].hex(), "candidate": candidate(row)} for row in rows],
              "excluded_zero_width_cases": len(excluded),
              "engines": {}}
    for engine in ("icarus", "kirimasim"):
        with tempfile.TemporaryDirectory(prefix="kirimasim-binary-scan-") as tmp:
            root = Path(tmp)
            source = root / "scan.sv"
            source.write_text(design(rows))
            for i, row in enumerate(rows):
                (root / f"data_{i}.bin").write_bytes(row["data"])
            version = bounded_run.run([args.iverilog, "-V"] if engine == "icarus" else
                                      [str(args.kirimasim.resolve()), "--version"], timeout=10)
            record = {"version": (version.stdout or version.stderr).splitlines()[0]}
            if engine == "icarus":
                compiled = bounded_run.run([args.iverilog, "-g2012", "-s", "top", "-o",
                                            str(root / "scan.vvp"), str(source)],
                                           cwd=root, text=False, timeout=30)
                if compiled.returncode:
                    record.update(status="compile_failed", exit=compiled.returncode,
                                  diagnostics=scrub(compiled.stderr, root))
                    report["engines"][engine] = record
                    continue
                cmd = [args.vvp, str(root / "scan.vvp")]
            else:
                cmd = [str(args.kirimasim.resolve()), "--top=top", str(source)]
            result = bounded_run.run(cmd, cwd=root, text=False, timeout=30)
            trace, warnings = {}, []
            for line in scrub(result.stdout, root).splitlines():
                match = re.fullmatch(r"BINARY_SCAN (\d+) (-?\d+) (-?\d+) (-?\d+) ([0-9a-fA-FxXzZ]+) (-?\d+)", line)
                if match:
                    i, count, position, eof, value, next_byte = match.groups()
                    trace[i] = {"assigned": int(count), "position": int(position), "eof": int(eof),
                                "value_hex": value, "next_byte": int(next_byte)}
                else:
                    warnings.append(line)
            record.update(status="observed" if result.returncode == 0 else "failed", exit=result.returncode,
                          traces=trace, warnings=warnings, diagnostics=scrub(result.stderr, root))
            if result.returncode < 0:
                record["signal"] = signal.Signals(-result.returncode).name
            record["candidate_differences"] = [int(i) for i, observed in trace.items()
                                               if observed != candidate(rows[int(i)])]
            record["candidate_comparison_complete"] = result.returncode == 0 and len(trace) == len(rows)
            report["engines"][engine] = record
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    for engine, record in report["engines"].items():
        differences = (str(len(record["candidate_differences"])) if record.get("traces") else
                       "not compared (no result traces)")
        print(f"{engine}: {record['status']}, {len(record.get('traces', {}))}/{len(rows)} observations, "
              f"candidate differences: {differences}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
