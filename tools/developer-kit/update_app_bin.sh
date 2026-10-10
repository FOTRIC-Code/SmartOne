#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

usage() {
  cat <<'EOF'
Usage:
  update_app_bin.sh [-u user] [-P port] <host> <app_bin_path>

Examples:
  update_app_bin.sh 192.168.123.100 /usr/bin/my_app
  update_app_bin.sh -u root -P 22 cube.local /usr/bin/net_app
EOF
}

SSH_USER="root"
SSH_PORT="22"

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

[[ $# -eq 2 ]] || {
  usage >&2
  exit 2
}

HOST="$1"
TARGET_APP_BIN="$2"
TARGET_LOGIN="$(remote_login "${HOST}" "${SSH_USER}")"

need_cmd ssh

log "updating app_bin on ${TARGET_LOGIN}:${SSH_PORT} -> ${TARGET_APP_BIN}"

ssh -p "${SSH_PORT}" "${TARGET_LOGIN}" "python3 - \"${TARGET_APP_BIN}\"" <<'PY'
import json
import os
import sys
import tempfile

target = sys.argv[1]
cfg_dir = "/etc/cfg/main_app"
cfg_path = os.path.join(cfg_dir, "config.json")

os.makedirs(cfg_dir, exist_ok=True)

data = {}
if os.path.exists(cfg_path):
    with open(cfg_path, "r", encoding="utf-8") as fp:
        try:
            data = json.load(fp)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"config decode failed: {exc}")

data["app_bin"] = target

fd, tmp_path = tempfile.mkstemp(prefix="config.", suffix=".json", dir=cfg_dir)
os.close(fd)
with open(tmp_path, "w", encoding="utf-8") as fp:
    json.dump(data, fp, indent=2, ensure_ascii=True)
    fp.write("\n")
os.replace(tmp_path, cfg_path)
PY

log "app_bin updated"
