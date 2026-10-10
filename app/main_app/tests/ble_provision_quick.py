#!/usr/bin/env python3
"""
Quick BLE provisioning helper for Parkia.Cube.

Connects to a SmartOne device over BLE and sends a wifi_sta_apply or
wifi_ap_apply command.  Prints the final response as JSON and exits with
code 0 on success, 1 on failure.

Dependencies:
    python3 -m pip install bleak

Usage examples:
    python3 ble_provision_quick.py sta --ssid "MyWiFi" --pwd "password123"
    python3 ble_provision_quick.py ap  --ssid "SmartOne-AP" --pwd "12345678"
    python3 ble_provision_quick.py sta --ssid "MyWiFi" --name "SmartOne-ABCD"
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from typing import Any, Dict, Optional

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    sys.exit("Missing dependency 'bleak'. Install: python3 -m pip install bleak")

SERVICE_UUID = "00001234-0000-1000-8000-00805f9b34fb"
WRITE_UUID = "00001235-0000-1000-8000-00805f9b34fb"
NOTIFY_UUID = "00001236-0000-1000-8000-00805f9b34fb"

DEFAULT_BLE_NAME = "SmartOne"
SCAN_TIMEOUT = 6.0
CONNECT_TIMEOUT = 10.0


def _json(obj: Any) -> str:
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":"))


def _json_pretty(obj: Any) -> str:
    return json.dumps(obj, ensure_ascii=False, indent=2)


# ── BLE helpers ──────────────────────────────────────────────────────────

async def find_device(
    name: str,
    address: Optional[str],
    timeout: float = SCAN_TIMEOUT,
) -> Any:
    """Scan and return the first matching BleakDevice."""
    print(f"Scanning for BLE device (name={name!r}, address={address!r}) …")
    discovered = await BleakScanner.discover(timeout=timeout, return_adv=True)

    for dev, _adv in discovered.values():
        if address and (dev.address or "").lower() == address.lower():
            return dev
        dev_name = dev.name or ""
        if not address and dev_name.startswith(name):
            return dev

    names = [
        f"  {d.name or '(unnamed)'}  {d.address}"
        for d, _ in discovered.values()
    ]
    raise SystemExit(
        "No matching device found.\nVisible devices:\n" + "\n".join(names[:12])
    )


async def provision(
    device: Any,
    command: Dict[str, Any],
    timeout: float,
) -> Dict[str, Any]:
    """Connect, send *command*, wait for the final BLE notification, return it."""
    queue: asyncio.Queue[Dict[str, Any]] = asyncio.Queue()

    def _on_notify(_sender: Any, raw: bytearray) -> None:
        try:
            payload = json.loads(bytes(raw).decode("utf-8", errors="replace"))
        except (json.JSONDecodeError, UnicodeDecodeError):
            return
        queue.put_nowait(payload)

    op = command["op"]
    req_id = command["req_id"]

    async with BleakClient(device, timeout=CONNECT_TIMEOUT) as client:
        print(f"Connected to {device.name} ({device.address})")
        await client.start_notify(NOTIFY_UUID, _on_notify)

        raw = _json(command).encode("utf-8")
        try:
            await client.write_gatt_char(WRITE_UUID, raw, response=True)
        except Exception:
            await client.write_gatt_char(WRITE_UUID, raw, response=False)
        print(f"Sent: {_json(command)}")

        deadline = asyncio.get_running_loop().time() + timeout
        while True:
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0:
                raise SystemExit(f"Timed out ({timeout}s) waiting for {op} response")
            event = await asyncio.wait_for(queue.get(), timeout=remaining)
            if not isinstance(event, dict):
                continue
            if event.get("op") != op or event.get("req_id") != req_id:
                continue

            # For wifi_sta_apply: wait until a terminal condition
            if op == "wifi_sta_apply":
                code = event.get("code")
                if code is not None and code != 0:
                    return event
                if event.get("msg") == "Provisioned":
                    return event
                stage = (event.get("data") or {}).get("stage")
                if stage in ("done", "failed"):
                    return event
                # intermediate progress – keep waiting
                continue

            # For wifi_ap_apply (and any other op): first response with code is final
            if "code" in event:
                return event


# ── CLI ──────────────────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Quick BLE Wi-Fi provisioning for Parkia.Cube",
    )
    parser.add_argument(
        "--name", default=DEFAULT_BLE_NAME,
        help="BLE device name or prefix (default: %(default)s)",
    )
    parser.add_argument("--address", help="BLE MAC address (overrides --name)")
    parser.add_argument(
        "--timeout", type=float, default=None,
        help="Response timeout in seconds (default: 60 for sta, 30 for ap)",
    )

    sub = parser.add_subparsers(dest="mode", required=True)

    sta = sub.add_parser("sta", help="Provision device as Wi-Fi station (STA)")
    sta.add_argument("--ssid", required=True, help="Wi-Fi SSID to join")
    sta.add_argument("--pwd", default="", help="Wi-Fi password (default: open)")

    ap = sub.add_parser("ap", help="Provision device as Wi-Fi access point (AP)")
    ap.add_argument("--ssid", required=True, help="AP SSID to broadcast")
    ap.add_argument("--pwd", default="", help="AP password (default: open)")
    ap.add_argument("--channel", type=int, default=0, help="AP channel (default: 0 = auto)")

    return parser


async def async_main() -> int:
    args = build_parser().parse_args()

    # Defaults per mode
    if args.timeout is None:
        args.timeout = 60.0 if args.mode == "sta" else 30.0

    device = await find_device(args.name, args.address)

    if args.mode == "sta":
        command = {
            "op": "wifi_sta_apply",
            "req_id": 1,
            "ssid": args.ssid,
            "pwd": args.pwd,
        }
    else:
        command = {
            "op": "wifi_ap_apply",
            "req_id": 1,
            "ssid": args.ssid,
            "pwd": args.pwd,
            "channel": args.channel,
        }

    result = await provision(device, command, args.timeout)
    print(_json_pretty(result))

    code = result.get("code")
    if code == 0:
        print("✓ Provisioning succeeded")
        return 0
    else:
        print(f"✗ Provisioning failed (code={code})", file=sys.stderr)
        return 1


def main() -> int:
    try:
        return asyncio.run(async_main())
    except KeyboardInterrupt:
        print("\nInterrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
