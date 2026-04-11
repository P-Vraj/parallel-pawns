#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_CMD="${ENGINE_CMD:-${ROOT_DIR}/build/engine}"
WORKER_COUNT="${WORKER_COUNT:-2}"
WORKER_BASE_PORT="${WORKER_BASE_PORT:-5001}"
WORKER_BIND="${WORKER_BIND:-127.0.0.1}"
WORKER_LOG_DIR="${WORKER_LOG_DIR:-${ROOT_DIR}/scripts/fastchess_logs/distributed-workers/$(date +%Y-%m-%d-%H-%M-%S)}"
KEEP_WORKERS="${KEEP_WORKERS:-0}"
DEMO_MOVETIME_MS="${DEMO_MOVETIME_MS:-500}"
DEMO_WAIT_SECONDS="${DEMO_WAIT_SECONDS:-1}"

[[ -x "${ENGINE_CMD}" ]] || { echo "missing engine binary: ${ENGINE_CMD}" >&2; exit 1; }
mkdir -p "${WORKER_LOG_DIR}"

worker_endpoints=()
worker_pids=()

cleanup() {
  if [[ "${KEEP_WORKERS}" != "0" ]]; then
    return
  fi

  for pid in "${worker_pids[@]:-}"; do
    if kill -0 "${pid}" 2>/dev/null; then
      kill "${pid}" 2>/dev/null || true
      wait "${pid}" 2>/dev/null || true
    fi
  done
}

trap cleanup EXIT INT TERM

start_workers() {
  local i port log_path
  for ((i = 0; i < WORKER_COUNT; ++i)); do
    port=$((WORKER_BASE_PORT + i))
    log_path="${WORKER_LOG_DIR}/worker-${port}.log"
    "${ENGINE_CMD}" --worker-bind "${WORKER_BIND}" --worker-port "${port}" >"${log_path}" 2>&1 &
    worker_pids+=("$!")
    worker_endpoints+=("${WORKER_BIND}:${port}")
  done
  sleep 0.2
}

join_worker_endpoints() {
  local joined=""
  local endpoint
  for endpoint in "${worker_endpoints[@]}"; do
    if [[ -n "${joined}" ]]; then
      joined+=","
    fi
    joined+="${endpoint}"
  done
  printf '%s' "${joined}"
}

run_uci_demo() {
  local endpoints
  endpoints="$(join_worker_endpoints)"
  {
    printf 'uci\n'
    printf 'setoption name Distributed Workers value %s\n' "${endpoints}"
    printf 'position startpos\n'
    printf 'go movetime %s\n' "${DEMO_MOVETIME_MS}"
    sleep "${DEMO_WAIT_SECONDS}"
    printf 'quit\n'
  } | "${ENGINE_CMD}"
}

run_fastchess() {
  local endpoints
  local concurrency
  endpoints="$(join_worker_endpoints)"
  concurrency="${CONCURRENCY:-1}"

  if [[ "${concurrency}" != "1" ]]; then
    echo "warning: distributed fastchess works best with CONCURRENCY=1; current value is ${concurrency}" >&2
    echo "warning: multiple concurrent Contender processes will compete for the same worker pool" >&2
  fi

  ENGINE_A="${ENGINE_A:-name=Contender cmd=${ENGINE_CMD} option.Threads=1 option.Hash=1024 option.Distributed_Workers=${endpoints}}" \
  ENGINE_B="${ENGINE_B:-name=Baseline cmd=${ENGINE_CMD} option.Threads=1 option.Hash=1024}" \
  CONCURRENCY="${concurrency}" \
  ROUNDS="1" \
  TIME_CONTROL="0:50+0.1" \
  WRITE_LOGS=1 bash "${ROOT_DIR}/scripts/run-fastchess.sh"
}

usage() {
  cat <<EOF
Usage:
  $(basename "$0") uci-demo
  $(basename "$0") fastchess

Environment:
  ENGINE_CMD=${ENGINE_CMD}
  WORKER_COUNT=${WORKER_COUNT}
  WORKER_BASE_PORT=${WORKER_BASE_PORT}
  WORKER_BIND=${WORKER_BIND}
  WORKER_LOG_DIR=${WORKER_LOG_DIR}
  KEEP_WORKERS=${KEEP_WORKERS}
  DEMO_MOVETIME_MS=${DEMO_MOVETIME_MS}
  DEMO_WAIT_SECONDS=${DEMO_WAIT_SECONDS}
  CONCURRENCY=${CONCURRENCY:-1}
EOF
}

command="${1:-uci-demo}"
case "${command}" in
  uci-demo)
    start_workers
    run_uci_demo
    ;;
  fastchess)
    start_workers
    run_fastchess
    ;;
  *)
    usage
    exit 1
    ;;
esac
