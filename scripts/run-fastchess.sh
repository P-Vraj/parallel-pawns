#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FASTCHESS_BIN="${FASTCHESS_BIN:-fastchess}"
PARSER_SCRIPT="${ROOT_DIR}/scripts/parse-fastchess-telemetry.py"
ENGINE_CMD="${ENGINE_CMD:-${ROOT_DIR}/build/engine}"
BOOK_PATH="${BOOK_PATH:-${ROOT_DIR}/data/openings/UHO_Lichess_4852_v1.epd}"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/scripts/fastchess_logs}"
RUN_DIR="${RUN_DIR:-${OUTPUT_DIR}/$(date +%Y-%m-%d-%H-%M-%S)}"

ENGINE_A="${ENGINE_A:-name=Contender cmd=${ENGINE_CMD} option.Threads=4 option.Hash=1024}"
ENGINE_B="${ENGINE_B:-name=Baseline cmd=${ENGINE_CMD} option.Threads=1 option.Hash=1024}"

# ENGINE_A="${ENGINE_A:-name=Contender cmd=${ENGINE_CMD} option.Threads=1 option.Hash=1024}"
# ENGINE_B="${ENGINE_B:-name=Baseline cmd=stockfish option.Threads=1 option.UCI_LimitStrength=true option.UCI_Elo=1650}"

TIME_CONTROL="${TIME_CONTROL:-0:10+0.1}"
ROUNDS="${ROUNDS:-25}"
CONCURRENCY="${CONCURRENCY:-4}"
BOOK_PLIES="${BOOK_PLIES:-10}"
WRITE_LOGS="${WRITE_LOGS:-0}"

command -v "${FASTCHESS_BIN}" >/dev/null || { echo "missing fastchess: ${FASTCHESS_BIN}" >&2; exit 1; }
[[ -x "${ENGINE_CMD}" ]] || { echo "missing engine binary: ${ENGINE_CMD}" >&2; exit 1; }
[[ -f "${BOOK_PATH}" ]] || { echo "missing opening book: ${BOOK_PATH}" >&2; exit 1; }

mkdir -p "${RUN_DIR}"
cd "${RUN_DIR}"

args=(
  -engine ${ENGINE_A}
  -engine ${ENGINE_B}
  -concurrency "${CONCURRENCY}"
  -openings "file=${BOOK_PATH}" format=epd order=random plies=${BOOK_PLIES}
  -each proto=uci tc="${TIME_CONTROL}"
  -rounds "${ROUNDS}"
  -repeat
  -show-latency
)

if [[ "${WRITE_LOGS}" != "0" ]]; then
  args+=(
    -pgnout "file=games.pgn" timeleft=true latency=true
    -log "file=trace.log" level=trace engine=true realtime=true
  )
fi

fastchess_status=0
"${FASTCHESS_BIN}" "${args[@]}" || fastchess_status=$?

if [[ "${WRITE_LOGS}" != "0" ]]; then
  python3 "${PARSER_SCRIPT}" "trace.log"
fi

exit "${fastchess_status}"
