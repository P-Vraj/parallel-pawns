#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_SCRIPT="${ROOT_DIR}/scripts/run-fastchess.sh"
ENGINE_CMD="${ENGINE_CMD:-${ROOT_DIR}/build/engine}"
BASE_OUTPUT_DIR="${BASE_OUTPUT_DIR:-${ROOT_DIR}/scripts/fastchess_logs/batch/$(date +%Y-%m-%d-%H-%M-%S)}"

mkdir -p "${BASE_OUTPUT_DIR}"

run_match() {
  local a_threads="$1"
  local b_threads="$2"
  local concurrency="$3"
  local rounds="$4"

  local label="a${a_threads}-b${b_threads}-c${concurrency}-r${rounds}"
  local run_output_dir="${BASE_OUTPUT_DIR}/${label}"
  local stdout_log="${BASE_OUTPUT_DIR}/${label}.log"

  mkdir -p "${run_output_dir}"

  echo "Running ${label}"
  echo "  output dir: ${run_output_dir}"
  echo "  stdout log: ${stdout_log}"

  ENGINE_A="name=Contender cmd=${ENGINE_CMD} option.Threads=${a_threads} option.Hash=1024" \
  ENGINE_B="name=Baseline cmd=${ENGINE_CMD} option.Threads=${b_threads} option.Hash=1024" \
  CONCURRENCY="${concurrency}" \
  ROUNDS="${rounds}" \
  OUTPUT_DIR="${run_output_dir}" \
  bash "${RUN_SCRIPT}" >"${stdout_log}" 2>&1
}

run_match 4 1 4 100
run_match 8 1 2 100
run_match 16 1 1 50

echo
echo "Batch complete. Logs are in ${BASE_OUTPUT_DIR}"
