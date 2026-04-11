#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_CMD="${ENGINE_CMD:-${ROOT_DIR}/build/engine}"
WORKER_COUNT="${WORKER_COUNT:-3}"
WORKER_BASE_PORT="${WORKER_BASE_PORT:-5001}"
WORKER_BIND="${WORKER_BIND:-127.0.0.1}"
WORKER_LOG_DIR="${WORKER_LOG_DIR:-${ROOT_DIR}/scripts/fastchess_logs/distributed-workers/$(date +%Y-%m-%d-%H-%M-%S)}"
WORKER_CONFIG_PATH="${WORKER_CONFIG_PATH:-${WORKER_LOG_DIR}/workers.conf}"
WORKER_THREADS="${WORKER_THREADS:-4}"
WORKER_READY_TIMEOUT_SECS="${WORKER_READY_TIMEOUT_SECS:-10}"
WORKER_READY_POLL_SECS="${WORKER_READY_POLL_SECS:-0.1}"
KEEP_WORKERS="${KEEP_WORKERS:-0}"
DEMO_MOVETIME_MS="${DEMO_MOVETIME_MS:-500}"
DEMO_WAIT_SECONDS="${DEMO_WAIT_SECONDS:-1}"

[[ -x "${ENGINE_CMD}" ]] || { echo "missing engine binary: ${ENGINE_CMD}" >&2; exit 1; }
mkdir -p "${WORKER_LOG_DIR}"

worker_endpoints=()
worker_ports=()
worker_pids=()
worker_logs=()

worker_threads_for_index() {
  local index="$1"
  local raw="${WORKER_THREADS}"
  if [[ "${raw}" == *,* ]]; then
    IFS=',' read -r -a parts <<< "${raw}"
    if (( index < ${#parts[@]} )); then
      printf '%s' "${parts[index]}"
      return
    fi
    printf '%s' "${parts[$((${#parts[@]} - 1))]}"
    return
  fi
  printf '%s' "${raw}"
}

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
  local i requested_port port log_path
  : > "${WORKER_CONFIG_PATH}"
  for ((i = 0; i < WORKER_COUNT; ++i)); do
    requested_port=$((WORKER_BASE_PORT + i))
    port="$(find_available_worker_port "${requested_port}")"
    if [[ "${port}" != "${requested_port}" ]]; then
      echo "info: worker port ${requested_port} unavailable, using ${port} instead" >&2
    fi
    log_path="${WORKER_LOG_DIR}/worker-${port}.log"
    "${ENGINE_CMD}" --worker-bind "${WORKER_BIND}" --worker-port "${port}" >"${log_path}" 2>&1 &
    worker_pids+=("$!")
    worker_logs+=("${log_path}")
    worker_ports+=("${port}")
    worker_endpoints+=("${WORKER_BIND}:${port}")
    printf '%s:%s %s\n' "${WORKER_BIND}" "${port}" "$(worker_threads_for_index "${i}")" >> "${WORKER_CONFIG_PATH}"
  done
  wait_for_workers_ready
}

port_is_ready() {
  local host="$1"
  local port="$2"
  if command -v python3 >/dev/null 2>&1; then
    python3 - "$host" "$port" <<'PY'
import socket
import sys

host = sys.argv[1]
port = int(sys.argv[2])
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(0.2)
try:
    sock.connect((host, port))
except OSError:
    sys.exit(1)
finally:
    sock.close()
sys.exit(0)
PY
    return $?
  fi

  if (echo >"/dev/tcp/${host}/${port}") >/dev/null 2>&1; then
    return 0
  fi

  return 1
}

port_is_bindable() {
  local host="$1"
  local port="$2"
  python3 - "$host" "$port" <<'PY'
import socket
import sys

host = sys.argv[1]
port = int(sys.argv[2])
family = socket.AF_INET6 if ":" in host and host != "127.0.0.1" else socket.AF_INET
sock = socket.socket(family, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    sock.bind((host, port))
except OSError:
    sys.exit(1)
finally:
    sock.close()
sys.exit(0)
PY
}

find_available_worker_port() {
  local requested_port="$1"
  local candidate="${requested_port}"
  while true; do
    if port_is_bindable "${WORKER_BIND}" "${candidate}"; then
      printf '%s' "${candidate}"
      return
    fi
    candidate=$((candidate + 1))
  done
}

wait_for_workers_ready() {
  local i host port deadline now
  deadline=$(python3 - "${WORKER_READY_TIMEOUT_SECS}" <<'PY'
import sys
import time
print(time.time() + float(sys.argv[1]))
PY
)

  for ((i = 0; i < WORKER_COUNT; ++i)); do
    host="${WORKER_BIND}"
    port="${worker_ports[i]}"

    while true; do
      if ! kill -0 "${worker_pids[i]}" 2>/dev/null; then
        echo "worker failed before becoming ready: ${host}:${port}" >&2
        echo "worker log: ${worker_logs[i]}" >&2
        exit 1
      fi

      if port_is_ready "${host}" "${port}"; then
        break
      fi

      now=$(python3 - <<'PY'
import time
print(time.time())
PY
)
      if python3 - "$now" "$deadline" <<'PY'
import sys
sys.exit(0 if float(sys.argv[1]) <= float(sys.argv[2]) else 1)
PY
      then
        sleep "${WORKER_READY_POLL_SECS}"
        continue
      fi

      echo "worker did not become ready in time: ${host}:${port}" >&2
      echo "worker log: ${worker_logs[i]}" >&2
      exit 1
    done
  done
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
  {
    printf 'uci\n'
    printf 'setoption name Distributed_Workers_Config value %s\n' "${WORKER_CONFIG_PATH}"
    printf 'position startpos\n'
    printf 'go movetime %s\n' "${DEMO_MOVETIME_MS}"
    sleep "${DEMO_WAIT_SECONDS}"
    printf 'quit\n'
  } | "${ENGINE_CMD}"
}

run_fastchess() {
  local concurrency
  concurrency="${CONCURRENCY:-1}"

  if [[ "${concurrency}" != "1" ]]; then
    echo "warning: distributed fastchess works best with CONCURRENCY=1; current value is ${concurrency}" >&2
    echo "warning: multiple concurrent Contender processes will compete for the same worker pool" >&2
  fi

  ENGINE_A="${ENGINE_A:-name=Contender cmd=${ENGINE_CMD} option.Threads=4 option.Hash=1024 option.Distributed_Workers_Config=${WORKER_CONFIG_PATH}}" \
  ENGINE_B="${ENGINE_B:-name=Baseline cmd=${ENGINE_CMD} option.Threads=4 option.Hash=1024}" \
  CONCURRENCY="${concurrency}" \
  TIME_CONTROL="0:80+0.8" \
  ROUNDS=40 \
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
  WORKER_CONFIG_PATH=${WORKER_CONFIG_PATH}
  WORKER_THREADS=${WORKER_THREADS}
  WORKER_READY_TIMEOUT_SECS=${WORKER_READY_TIMEOUT_SECS}
  WORKER_READY_POLL_SECS=${WORKER_READY_POLL_SECS}
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
