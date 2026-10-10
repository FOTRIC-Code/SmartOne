#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
webhook_echo.py - Parkia.Cube 报警 Webhook 临时测试服务
仅依赖 Python 标准库，Windows / Linux / macOS 均可运行。

用法:
    python webhook_echo.py [端口]        # 默认 9000

设备侧配置:
    curl -X PUT http://<设备IP>/system/webhook -H "Content-Type: application/json" ^
         -d "{\"url\":\"http://<本机IP>:9000/alarm\"}"
    curl -X PUT http://<设备IP>/alarm/triggers/2 -H "Content-Type: application/json" ^
         -d "{\"type\":2,\"trigger_webhook_enabled\":true,\"trigger_ftp_enabled\":false,\"trigger_record_enabled\":false}"

浏览器打开 http://127.0.0.1:9000/ 可确认服务在线、并看到已接收的报警条数。
"""
import json
import sys
import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ALARM_TYPES = {1: "thermal_temp", 2: "intrusion", 3: "high_temp", 4: "body_temp"}
LOG_FILE = "webhook_echo.log"
counter = 0


def stamp():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def emit(line):
    print(line, flush=True)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(line + "\n")


class Handler(BaseHTTPRequestHandler):
    def _send_json(self, code, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        self._send_json(200, {
            "service": "parkia webhook echo",
            "time": stamp(),
            "alarms_received": counter,
            "usage": "POST alarm JSON to any path, e.g. /alarm",
        })

    def do_POST(self):
        global counter
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length > 0 else b""
        text = raw.decode("utf-8", errors="replace")

        emit("=" * 70)
        emit(f"[{stamp()}] POST {self.path}  from {self.client_address[0]}")
        ct = self.headers.get("Content-Type", "")
        emit(f"Content-Type: {ct}   Bytes: {length}")

        try:
            payload = json.loads(text)
            emit(json.dumps(payload, ensure_ascii=False, indent=2))

            atype = payload.get("type")
            info = payload.get("alarm_info", {}) or {}
            name = ALARM_TYPES.get(atype, f"UNKNOWN({atype})")
            emit("-" * 70)
            emit(f"解析: 报警类型 = {atype} ({name})")
            for key in ("timestamp_us", "motion_level", "temperature",
                        "threshold", "baseline", "slope", "message",
                        "video_path"):
                if key in info:
                    emit(f"  {key:14s} = {info[key]}")
            missing = [k for k in ("timestamp_us", "message", "video_path")
                       if k not in info]
            if missing:
                emit(f"!! 缺少字段: {missing}")
            counter += 1
        except json.JSONDecodeError:
            emit("!! 非 JSON 请求体,原始内容:")
            emit(text)

        self._send_json(200, {"code": 0, "message": "OK"})

    def log_message(self, fmt, *args):  # 关闭默认逐行访问日志,保持输出干净
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9000
    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    emit(f"[{stamp()}] webhook echo listening on 0.0.0.0:{port} "
         f"(log: {LOG_FILE}, Ctrl+C 退出)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        emit(f"[{stamp()}] stopped, total alarms: {counter}")


if __name__ == "__main__":
    main()
