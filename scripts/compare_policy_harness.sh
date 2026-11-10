# compares cachelet against libCacheSim, which also implements a policy harness
# usage: scripts/compare_policy_harness.sh <policy> [sizes...]
set -euo pipefail

LIBCACHESIM_DIR="${LIBCACHESIM_DIR:-../libCacheSim}"   # override with an env var
TRACE_CSV="${TRACE_CSV:-$LIBCACHESIM_DIR/data/twitter_cluster52.csv}"
KEYS="${KEYS:-traces/prod/twitter52_keys.txt}"

policy="$1"; shift
sizes=("${@:-10 1000 10000}")

for size in ${sizes[@]}; do
  echo "=== $policy, size $size ==="
  "$LIBCACHESIM_DIR/_build/bin/cachesim" "$TRACE_CSV" csv "$policy" "$size" \
      --ignore-obj-size=true \
      -t "time-col=1, obj-id-col=2, obj-size-col=3, obj-id-is-num=1" 2>/dev/null | grep "miss ratio"
  ./build/cachelet-sim "$KEYS" "$policy" "$size" | grep "miss ratio"
done