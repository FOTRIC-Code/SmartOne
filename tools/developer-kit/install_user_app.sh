#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

usage() {
  cat <<'EOF'
Usage:
  install_user_app.sh [-u user] [-P port] [--no-reboot] <host> <elf> [remote_name]

Examples:
  install_user_app.sh 192.168.123.100 ./build/user_app
  install_user_app.sh 192.168.123.100 ./build/user_app my_app
  install_user_app.sh cube.local ./build/user_app /usr/bin/net_app
EOF
}

SSH_USER="root"
SSH_PORT="22"
NO_REBOOT=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -u|--user)
      SSH_USER="${2:-}"
      shift 2
      ;;
    -P|--port)
      SSH_PORT="${2:-}"
      shift 2
      ;;
    --no-reboot)
      NO_REBOOT=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      die "unknown argument: $1"
      ;;
    *)
      break
      ;;
  esac
done

[[ $# -ge 2 && $# -le 3 ]] || {
  usage >&2
  exit 2
}

HOST="$1"
LOCAL_ELF="$(resolve_abs_path "$2")"
REMOTE_NAME="${3:-$(basename "${LOCAL_ELF}")}"
REMOTE_PATH="$(remote_target_path "${REMOTE_NAME}")"
TARGET_LOGIN="$(remote_login "${HOST}" "${SSH_USER}")"

[[ -f "${LOCAL_ELF}" ]] || die "ELF not found: ${LOCAL_ELF}"

need_cmd scp
need_cmd ssh

log "uploading ${LOCAL_ELF} -> ${TARGET_LOGIN}:${REMOTE_PATH}"
scp -P "${SSH_PORT}" "${LOCAL_ELF}" "${TARGET_LOGIN}:${REMOTE_PATH}"
ssh -p "${SSH_PORT}" "${TARGET_LOGIN}" "chmod 0755 '${REMOTE_PATH}'"

"${SCRIPT_DIR}/update_app_bin.sh" \
  -u "${SSH_USER}" -P "${SSH_PORT}" "${HOST}" "${REMOTE_PATH}"

if [[ "${NO_REBOOT}" -eq 0 ]]; then
  log "rebooting ${TARGET_LOGIN}"
  ssh -p "${SSH_PORT}" "${TARGET_LOGIN}" "reboot" || true
else
  log "skipping reboot (--no-reboot)"
fi

log "install completed"
