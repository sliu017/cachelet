#!/usr/bin/env python3
"""Summarize benchmarks/results.csv: average repeated runs and compare versions.

Usage (from the repo root):
  python3 benchmarks/summarize_benchmarks.py                     # everything
  python3 benchmarks/summarize_benchmarks.py --labels redis cachelet-release
  python3 benchmarks/summarize_benchmarks.py --baseline redis    # add % vs a label

With --baseline, percentages are relative to that label:
  rps:      positive = higher throughput (better)
  p50/p99:  positive = higher latency (worse)
"""
import argparse
import sys

import pandas as pd

# Metrics to show, if present. Older redis-benchmark versions only output rps.
METRICS = ["rps", "p50_latency_ms", "p99_latency_ms"]
GROUP_KEYS = ["benchmark", "test", "label", "commit"]


def main():
    parser = argparse.ArgumentParser(description="Summarize bench.sh results")
    parser.add_argument("csv", nargs="?", default="benchmarks/results.csv",
                        help="results file (default: benchmarks/results.csv)")
    parser.add_argument("--labels", nargs="+", help="only include these labels")
    parser.add_argument("--baseline", help="label to compare the others against")
    args = parser.parse_args()

    # read label, commit as strings, not numbers
    df = pd.read_csv(args.csv, dtype={"label": str, "commit": str})
    if args.labels:
        df = df[df["label"].isin(args.labels)]
    if df.empty:
        sys.exit("no matching results")

    metrics = [m for m in METRICS if m in df.columns]

    # One row per (benchmark, test, label, commit): the mean of each metric
    # across repeated runs, plus how many runs were averaged.
    grouped = df.groupby(GROUP_KEYS)
    summary = grouped[metrics].mean().join(grouped.size().rename("runs")).reset_index()

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