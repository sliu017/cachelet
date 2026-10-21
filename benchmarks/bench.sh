#!/usr/bin/env bash
# bench.sh: benchmark one cachelet-server binary with redis-benchmark.
#
# Usage:   ./bench.sh <path-to-server-binary> [label]
# Example: ./bench.sh build/cachelet-server epoll
#          ./bench.sh ../cachelet-poll/build/cachelet-server poll
#
# Optional environment variables:
#   RUNS=3                   how many times to repeat each benchmark
#   OUT=benchmarks/results.csv    where results are appended
set -euo pipefail # any failure terminates the script, unset vars = error (for safety)

BIN=${1:?usage: ./bench.sh <server-binary> [label]} # set BIN to be the path of the server binary (required)
LABEL=${2:-$(basename "$(dirname "$(dirname "$(realpath "$BIN")")")")} # runs from build/cachelet-server, step back twice for the label
SERVER_ARGS=("${@:3}")   # all arguments from the 3rd onward, as an array
PORT=6379
RUNS=${RUNS:-3} # default to 3 runs
OUT=${OUT:-benchmarks/results.csv}

# list of benchmarks
BENCHMARKS=(
  "c50_small|-c 50 -n 100000 -t ping_mbulk,set,get" # relatively small request count, client count
  "c50_100kb|-c 50 -n 10000 -d 100000 -t set,get" # large data size
  "c1000_small|-c 1000 -n 200000 -t get" # larger number of clients, 1000
)

# raise open file limit from 1024 -> 4096 (for slight headroom regarding the 1000 client test
# NOTE if add higher client-based benchmarks ulimit needs to raise
ulimit -n 4096 || echo "warning: could not raise the fd limit" >&2

# check if the port is free
if redis-cli -p "$PORT" ping >/dev/null 2>&1; then
  echo "error: something is already listening on port $PORT" >&2
  exit 1
fi

# commit hash of the commit being tested, also works for work trees
COMMIT=$(git -C "$(dirname "$BIN")" rev-parse --short HEAD 2>/dev/null || echo unknown)


# start server in background, ALWAYS killed on exit, no matter reason
"$BIN" "${SERVER_ARGS[@]}" >/dev/null 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT # trap analogous to finally

# wait ~5s for server to start
for _ in $(seq 50); do
  redis-cli -p "$PORT" ping >/dev/null 2>&1 && break
  sleep 0.1
done
if ! redis-cli -p "$PORT" ping >/dev/null 2>&1; then
  echo "error: server did not start" >&2
  exit 1
fi

echo "  warm-up run (not recorded)" >&2
redis-benchmark -p "$PORT" -c 50 -n 10000 -t ping_mbulk,set,get -q >/dev/null 2>&1 || true

# ensure results folder
mkdir -p "$(dirname "$OUT")"
# print status line
echo "benchmarking $BIN (label=$LABEL, commit=$COMMIT, runs=$RUNS)" >&2 #&2 = stderr

for entry in "${BENCHMARKS[@]}"; do
  name=${entry%%|*} # extract name from combined benchmark string
  args=${entry#*|} # extract args from combined benchmark string
  for run in $(seq "$RUNS"); do
    echo "  $name: run $run/$RUNS" >&2
    # $args is not in quotes to split it into space-separated tokens
    # filter out stderr and other warnings
    result=$(redis-benchmark -p "$PORT" $args --csv 2>/dev/null | grep '^"')

    # header row
    if [[ ! -s "$OUT" ]]; then
      echo "\"label\",\"commit\",\"benchmark\",\"run\",$(head -n 1 <<<"$result")" > "$OUT"
    fi

    # write data rows in format
    # commit label (e.g. epoll) | hash | benchmark name | run number | result (for this benchmark)
    tail -n +2 <<<"$result" | while IFS= read -r line; do
      echo "\"$LABEL\",\"$COMMIT\",\"$name\",$run,$line"
    done >> "$OUT"
  done
done

echo "results appended to $OUT" >&2