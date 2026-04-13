#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE_HOST="${REMOTE_HOST:-}"
REMOTE_USER="${REMOTE_USER:-}"
REMOTE_DIR="${REMOTE_DIR:-parallel-pawns-mirror}"
REMOTE_PORT="${REMOTE_PORT:-5001}"
REMOTE_BIND="${REMOTE_BIND:-0.0.0.0}"
SSH_PORT="${SSH_PORT:-22}"
INSTALL_DEPS="${INSTALL_DEPS:-1}"
START_WORKER="${START_WORKER:-1}"
SYNC_BUILD="${SYNC_BUILD:-0}"

usage() {
  cat <<EOF
Usage:
  $(basename "$0") --host HOST [--user USER] [--remote-dir DIR] [--remote-port PORT] [--remote-bind HOST]
                  [--ssh-port PORT] [--skip-deps] [--no-start-worker] [--sync-build]

Environment:
  REMOTE_HOST=${REMOTE_HOST}
  REMOTE_USER=${REMOTE_USER}
  REMOTE_DIR=${REMOTE_DIR}
  REMOTE_PORT=${REMOTE_PORT}
  REMOTE_BIND=${REMOTE_BIND}
  SSH_PORT=${SSH_PORT}
  INSTALL_DEPS=${INSTALL_DEPS}
  START_WORKER=${START_WORKER}
  SYNC_BUILD=${SYNC_BUILD}

What it does:
  1. rsyncs this repo to the remote host
  2. runs scripts/install-worker.sh remotely
  3. optionally starts the worker in the background with nohup

Example:
  $(basename "$0") --host 203.0.113.10 --user ubuntu --remote-port 5001
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)
      REMOTE_HOST="$2"
      shift 2
      ;;
    --user)
      REMOTE_USER="$2"
      shift 2
      ;;
    --remote-dir)
      REMOTE_DIR="$2"
      shift 2
      ;;
    --remote-port)
      REMOTE_PORT="$2"
      shift 2
      ;;
    --remote-bind)
      REMOTE_BIND="$2"
      shift 2
      ;;
    --ssh-port)
      SSH_PORT="$2"
      shift 2
      ;;
    --skip-deps)
      INSTALL_DEPS=0
      shift
      ;;
    --no-start-worker)
      START_WORKER=0
      shift
      ;;
    --sync-build)
      SYNC_BUILD=1
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -z "${REMOTE_HOST}" ]]; then
  echo "--host is required" >&2
  usage >&2
  exit 1
fi

REMOTE_TARGET="${REMOTE_HOST}"
if [[ -n "${REMOTE_USER}" ]]; then
  REMOTE_TARGET="${REMOTE_USER}@${REMOTE_HOST}"
fi

ssh_opts=(-p "${SSH_PORT}")
rsync_opts=(-az --delete --exclude .git --exclude build --exclude scripts/fastchess_logs)
if [[ "${SYNC_BUILD}" != "0" ]]; then
  rsync_opts=(-az --delete --exclude .git --exclude scripts/fastchess_logs)
fi

ssh "${ssh_opts[@]}" "${REMOTE_TARGET}" "mkdir -p '${REMOTE_DIR}'"
rsync "${rsync_opts[@]}" -e "ssh -p ${SSH_PORT}" "${ROOT_DIR}/" "${REMOTE_TARGET}:${REMOTE_DIR}/"

install_args=()
if [[ "${INSTALL_DEPS}" == "0" ]]; then
  install_args+=(--skip-deps)
fi

ssh "${ssh_opts[@]}" "${REMOTE_TARGET}" "cd '${REMOTE_DIR}' && ./scripts/install-worker.sh ${install_args[*]}"

if [[ "${START_WORKER}" != "0" ]]; then
  ssh "${ssh_opts[@]}" "${REMOTE_TARGET}" \
    "cd '${REMOTE_DIR}' && nohup ./build/engine --worker-bind '${REMOTE_BIND}' --worker-port '${REMOTE_PORT}' > worker-${REMOTE_PORT}.log 2>&1 < /dev/null &"
  echo "started worker on ${REMOTE_TARGET}:${REMOTE_PORT}"
else
  echo "deployment complete on ${REMOTE_TARGET}; worker not started"
fi
