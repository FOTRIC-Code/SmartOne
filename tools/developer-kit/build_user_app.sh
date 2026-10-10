#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

usage() {
  cat <<'EOF'
Usage:
  build_user_app.sh [--sdk-root <path>] [--source <dir>] [--build-dir <dir>] [--clean]

Defaults:
  --source    samples/user_app
  --build-dir <source>/build

Environment:
  BUILDROOT_SDK_ROOT / SDK_ROOT  Buildroot SDK root after relocate-sdk.sh
EOF
}

SDK_ROOT_ARG=""
SOURCE_DIR="${REPO_ROOT}/samples/user_app"
BUILD_DIR=""
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdk-root)
      SDK_ROOT_ARG="${2:-}"
      shift 2
      ;;
    --source)
      SOURCE_DIR="${2:-}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown argument: $1"
      ;;
  esac
done

need_cmd cmake
need_cmd python3

SOURCE_DIR="$(resolve_abs_path "${SOURCE_DIR}")"
[[ -d "${SOURCE_DIR}" ]] || die "source directory not found: ${SOURCE_DIR}"

if [[ -z "${BUILD_DIR}" ]]; then
  BUILD_DIR="${SOURCE_DIR}/build"
fi
BUILD_DIR="$(resolve_abs_path "${BUILD_DIR}")"

SDK_ROOT_RESOLVED="$(resolve_sdk_root "${SDK_ROOT_ARG}")"
TOOLCHAIN_FILE="$(resolve_toolchain_file "${SDK_ROOT_RESOLVED}")"

if [[ "${CLEAN}" -eq 1 ]]; then
  rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

log "source: ${SOURCE_DIR}"
log "build:  ${BUILD_DIR}"
log "sdk:    ${SDK_ROOT_RESOLVED}"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
cmake --build "${BUILD_DIR}" -j"$(default_job_count)"

log "build completed"
