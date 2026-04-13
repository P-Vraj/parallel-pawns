#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_SRC_DIR="${ENGINE_SRC_DIR:-${ROOT_DIR}}"
BUILD_DIR="${BUILD_DIR:-${ENGINE_SRC_DIR}/build}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
INSTALL_DEPS="${INSTALL_DEPS:-1}"
CMAKE_GENERATOR="${CMAKE_GENERATOR:-}"
CXX_COMPILER="${CXX_COMPILER:-}"

usage() {
  cat <<EOF
Usage:
  $(basename "$0") [--engine-src-dir DIR] [--build-dir DIR] [--jobs N] [--skip-deps]

Environment:
  ENGINE_SRC_DIR=${ENGINE_SRC_DIR}
  BUILD_DIR=${BUILD_DIR}
  JOBS=${JOBS}
  INSTALL_DEPS=${INSTALL_DEPS}
  CMAKE_GENERATOR=${CMAKE_GENERATOR}
  CXX_COMPILER=${CXX_COMPILER}

What it does:
  1. Optionally installs build dependencies with apt-get
  2. Configures the engine with CMake
  3. Builds ./build/engine for worker mode
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --engine-src-dir)
      ENGINE_SRC_DIR="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --skip-deps)
      INSTALL_DEPS=0
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

if [[ "${INSTALL_DEPS}" != "0" ]]; then
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y build-essential clang cmake git ninja-build python3
  else
    echo "dependency installation is only automated for apt-based systems; rerun with --skip-deps after installing build tools" >&2
    exit 1
  fi
fi

mkdir -p "${BUILD_DIR}"

cmake_args=(
  -S "${ENGINE_SRC_DIR}"
  -B "${BUILD_DIR}"
)

if [[ -n "${CMAKE_GENERATOR}" ]]; then
  cmake_args+=(-G "${CMAKE_GENERATOR}")
fi

if [[ -n "${CXX_COMPILER}" ]]; then
  cmake_args+=(-D CMAKE_CXX_COMPILER="${CXX_COMPILER}")
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

echo "worker build complete: ${BUILD_DIR}/engine"
