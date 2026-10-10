#!/usr/bin/env bash
set -euo pipefail

DEVELOPER_KIT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${DEVELOPER_KIT_DIR}/../.." && pwd)"

log() {
  printf '[developer-kit] %s\n' "$*"
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

default_job_count() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN
  else
    echo 4
  fi
}

resolve_abs_path() {
  python3 - "$1" <<'PY'
import os
import sys

print(os.path.abspath(sys.argv[1]))
PY
}

resolve_sdk_root() {
  local input="${1:-}"
  local candidate

  if [[ -n "${input}" ]]; then
    candidate="${input}"
  elif [[ -n "${BUILDROOT_SDK_ROOT:-}" ]]; then
    candidate="${BUILDROOT_SDK_ROOT}"
  elif [[ -n "${SDK_ROOT:-}" ]]; then
    candidate="${SDK_ROOT}"
  elif [[ -d "${REPO_ROOT}/buildroot/output/rockchip_rk3506_board/host" ]]; then
    candidate="${REPO_ROOT}/buildroot/output/rockchip_rk3506_board/host"
  elif [[ -d "${REPO_ROOT}/output/rockchip_rk3506_board/host" ]]; then
    candidate="${REPO_ROOT}/output/rockchip_rk3506_board/host"
  else
    die "unable to locate Buildroot SDK root; pass --sdk-root"
  fi

  resolve_abs_path "${candidate}"
}

resolve_toolchain_file() {
  local sdk_root="$1"
  local candidate

  for candidate in \
    "${sdk_root}/share/buildroot/toolchainfile.cmake" \
    "${sdk_root}/host/share/buildroot/toolchainfile.cmake"
  do
    if [[ -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  die "unable to locate toolchainfile.cmake under ${sdk_root}"
}

remote_target_path() {
  local remote_name="${1:-}"

  if [[ -z "${remote_name}" ]]; then
    die "remote name cannot be empty"
  fi

  if [[ "${remote_name}" = /* ]]; then
    printf '%s\n' "${remote_name}"
  else
    printf '/usr/bin/%s\n' "${remote_name}"
  fi
}

remote_login() {
  local host="$1"
  local user="$2"

  printf '%s@%s\n' "${user}" "${host}"
}
