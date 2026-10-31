#!/usr/bin/env bash
# bench.sh: benchmark one cachelet-server binary with redis-benchmark.
#
# Usage:   ./bench.sh <path-to-server-binary> [label] [server args...]
# Example: ./bench.sh build/cachelet-server epoll
#          ./bench.sh ../cachelet-poll/build/cachelet-server poll
#          ./bench.sh build-release/cachelet-server cachelet-always --appendonly yes --appendfsync always
#
# Every benchmark run gets its own freshly started server in its own empty temp directory,
# such that no stateful elements (cache contents, memory, AOF size) will carry over between runs.
#
# Optional environment variables:
#   RUNS=3                   how many times to repeat each benchmark
#   OUT=benchmarks/data/results.csv    where results are appended by default
set -euo pipefail # any failure terminates the script, unset vars = error (for safety)

BIN=${1:?usage: ./bench.sh <server-binary> [label]} # set BIN to be the path of the server binary (required)
LABEL=${2:-$(basename "$(dirname "$(dirname "$(realpath "$BIN")")")")} # runs from build/cachelet-server, step back twice for the label
SERVER_ARGS=("${@:3}")   # all arguments from the 3rd onward, as an array
BIN=$(realpath -s "$BIN")   # -s: absolute path, but don't follow symbolic links
PORT=6379
RUNS=${RUNS:-3} # default to 3 runs
OUT=${OUT:-benchmarks/results.csv}

# list of benchmarks
# -r N: use random keys from a keyspace of N keys instead of one fixed key
#   still, because N is much less than the -n (request count), misses will be statistically rare
BENCHMARKS=(
  "c50_small|-c 50 -n 100000 -r 10000 -t ping_mbulk,set,get" # relatively small request count, client count
  "c50_100kb|-c 50 -n 10000 -r 1000 -d 100000 -t set,get" # large data size: keyspace caps memory at ~100 MB (NOTE: ~1 GB of AOF per run with persistence on)
  "c1000_small|-c 1000 -n 200000 -r 10000 -t set,get" # larger number of clients, 1000, small values
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

SERVER_PID=""
WORKDIR=""

# stop the current server (if any) and delete its temp directory
stop_server() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true # wait for it to exit so the port is free for the next run
    SERVER_PID=""
  fi
  if [[ -n "$WORKDIR" ]]; then
    rm -rf "$WORKDIR"
    WORKDIR=""
  fi
}

# start a fresh server in a new empty temp directory, so persistence files never carry over
start_server() {
  WORKDIR=$(mktemp -d)
  # exec makes the subshell become the server, so $! is the server's own PID
  (cd "$WORKDIR" && exec "$BIN" "${SERVER_ARGS[@]}") >/dev/null 2>&1 &
  SERVER_PID=$!

  # wait ~5s for server to start
  for _ in $(seq 50); do
    redis-cli -p "$PORT" ping >/dev/null 2>&1 && return 0
    sleep 0.1
  done
  echo "error: server did not start" >&2
  exit 1
}

# trap analogous to finally: ALWAYS stop the server and clean up on exit, no matter the reason
trap stop_server EXIT

# ensure results folder
mkdir -p "$(dirname "$OUT")"
# print status line
echo "benchmarking $BIN (label=$LABEL, commit=$COMMIT, runs=$RUNS)" >&2 #&2 = stderr

for entry in "${BENCHMARKS[@]}"; do
  name=${entry%%|*} # extract name from combined benchmark string
  args=${entry#*|} # extract args from combined benchmark string
  for run in $(seq "$RUNS"); do
    echo "  $name: run $run/$RUNS" >&2
    start_server

    # warm-up (not recorded): PING only, so it doesn't write any keys before the measured run
    redis-benchmark -p "$PORT" -c 50 -n 10000 -t ping_mbulk -q >/dev/null 2>&1 || true

    # $args is not in quotes to split it into space-separated tokens
    # filter out stderr and other warnings
    result=$(redis-benchmark -p "$PORT" $args --csv 2>/dev/null | grep '^"')

    stop_server

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