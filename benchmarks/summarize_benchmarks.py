#!/usr/bin/env python3
"""Summarize bench.sh results: combine repeated runs and compare versions.

Usage (from the repo root):
  python3 benchmarks/summarize_benchmarks.py                                   # every CSV in benchmarks/data
  python3 benchmarks/summarize_benchmarks.py benchmarks/data/epoll_grow.csv    # one file
  python3 benchmarks/summarize_benchmarks.py a.csv b.csv                       # several files
  python3 benchmarks/summarize_benchmarks.py --labels redis-always cachelet-always
  python3 benchmarks/summarize_benchmarks.py --baseline redis-always           # add % vs a label

Paths can be CSV files or directories (every *.csv inside is included).
Repeated runs are combined with the median, so a single glitched run can't skew the result.

With --baseline, percentages are relative to that label:
  rps:      positive = higher throughput (better)
  p50/p99:  positive = higher latency (worse)

Note: rows with the same label AND commit are combined, even across files. Keep results
from different benchmark setups apart (pass files explicitly, or use distinct labels).
"""
import argparse
import sys
from pathlib import Path

import pandas as pd

# Metrics to show, if present. Older redis-benchmark versions only output rps.
METRICS = ["rps", "p50_latency_ms", "p99_latency_ms"]
GROUP_KEYS = ["benchmark", "test", "label", "commit"]


def load_results(paths):
    """Read every CSV named in paths (files, or directories of CSVs) into one DataFrame."""
    files = []
    for p in map(Path, paths):
        if p.is_dir():
            files.extend(sorted(p.glob("*.csv")))
        elif p.exists():
            files.append(p)
        else:
            sys.exit(f"not found: {p}")
    if not files:
        sys.exit("no CSV files found")

    # read label, commit as strings, not numbers
    frames = [pd.read_csv(f, dtype={"label": str, "commit": str}) for f in files]
    return pd.concat(frames, ignore_index=True)


def main():
    parser = argparse.ArgumentParser(description="Summarize bench.sh results")
    parser.add_argument("paths", nargs="*", default=["benchmarks/data"],
                        help="CSV files and/or directories of CSVs (default: benchmarks/data)")
    parser.add_argument("--labels", nargs="+", help="only include these labels")
    parser.add_argument("--baseline", help="label to compare the others against")
    args = parser.parse_args()

    df = load_results(args.paths)
    if args.labels:
        df = df[df["label"].isin(args.labels)]

    # Drop impossible measurements (e.g. negative throughput from a
    # redis-benchmark timing glitch) and make note of which rows were dropped
    # so they can be investigated. Other glitched runs (e.g. absurdly high rps)
    # are handled by using the median below.
    bad = df[df["rps"] <= 0]
    if not bad.empty:
        print(f"warning: dropping {len(bad)} row(s) with non-positive rps:",
              file=sys.stderr)
        print(bad[["label", "commit", "benchmark", "run", "test", "rps"]]
              .to_string(index=False), file=sys.stderr)
        df = df[df["rps"] > 0]

    if df.empty:
        sys.exit("no matching results")

    metrics = [m for m in METRICS if m in df.columns]

    # One row per (benchmark, test, label, commit): the median of each metric
    # across repeated runs, plus how many runs were combined.
    grouped = df.groupby(GROUP_KEYS)
    summary = grouped[metrics].median().join(grouped.size().rename("runs")).reset_index()

    # Print one block per benchmark/test, keeping the order they were run in.
    for (bench, test), block in summary.groupby(["benchmark", "test"], sort=False):
        print(f"\n{bench} / {test}")

        base = None
        if args.baseline:
            match = block[block["label"] == args.baseline]
            if not match.empty:
                base = match.iloc[0]

        for _, row in block.iterrows():
            name = f"{row['label']} ({row['commit']})"
            cells = [f"{name:<34}", f"runs={row['runs']}"]
            for m in metrics:
                short = m.replace("_latency_ms", "")
                cell = f"{short}={row[m]:,.2f}"
                if base is not None and row["label"] != args.baseline:
                    pct = (row[m] / base[m] - 1) * 100
                    cell += f" ({pct:+.1f}%)"
                cells.append(f"{cell:<28}")
            print("  " + "  ".join(cells))


if __name__ == "__main__":
    main()