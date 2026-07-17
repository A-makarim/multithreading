#!/usr/bin/env bash
set -euo pipefail

build_dir="${BUILD_DIR:-build}"
port="${PORT:-19000}"
requests="${REQUESTS_PER_CLIENT:-50}"
workload="${WORKLOAD:-prime}"
results="${RESULTS_FILE:-benchmarks/results.csv}"
clients=(1 5 10 25 50 100)
workers=(1 2 4 8)

cat >"$results" <<'CSV'
workers,clients,requests_per_client,workload,total_requests,successes,failures,duration_seconds,throughput_requests_per_second,mean_latency_ms,median_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms
CSV

server_pid=""
cleanup() {
  if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
    kill -INT "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

for worker_count in "${workers[@]}"; do
  for client_count in "${clients[@]}"; do
    "$build_dir/threadforge_server" "$port" "$worker_count" 1024 \
      >"${TMPDIR:-/tmp}/threadforge-server.log" 2>&1 &
    server_pid=$!
    for _ in {1..100}; do
      if (echo >/dev/tcp/127.0.0.1/"$port") 2>/dev/null; then break; fi
      sleep 0.02
    done
    output=$("$build_dir/load_test" --host 127.0.0.1 --port "$port" \
      --clients "$client_count" --requests-per-client "$requests" \
      --workload "$workload" --prime-value 32416190071)
    echo "$output"
    csv=$(sed -n 's/^csv=//p' <<<"$output")
    if [[ -z "$csv" ]]; then echo "load_test did not emit CSV data" >&2; exit 1; fi
    echo "$worker_count,$client_count,$requests,$workload,$csv" >>"$results"
    kill -INT "$server_pid"
    wait "$server_pid"
    server_pid=""
  done
done

echo "Wrote $results"
