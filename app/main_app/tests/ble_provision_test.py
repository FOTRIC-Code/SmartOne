#!/usr/bin/env python3
"""
Practical BLE provisioning CLI for Parkia.Cube.

Dependency:
  python3 -m pip install bleak

This tool exercises the public BLE provisioning protocol only. It does not use
ADB, SSH, logs, or device-private hooks for core assertions.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple


SERVICE_UUID = "00001234-0000-1000-8000-00805f9b34fb"
COMMAND_UUID = "00001235-0000-1000-8000-00805f9b34fb"
EVENT_UUID = "00001236-0000-1000-8000-00805f9b34fb"

DEFAULT_NAME_PREFIX = "SmartOne"
DEFAULT_SCAN_TIMEOUT = 6.0
DEFAULT_CONNECT_TIMEOUT = 10.0
DEFAULT_COMMAND_TIMEOUT = 15.0
DEFAULT_STA_TIMEOUT = 90.0


class BleProvisionError(RuntimeError):
    """Operational failure for BLE provisioning actions."""


class ExpectationError(BleProvisionError):
    """Scenario expectation failed."""


def load_bleak() -> Tuple[Any, Any]:
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as exc:
        raise BleProvisionError(
            "Missing dependency 'bleak'. Install it with: python3 -m pip install bleak"
        ) from exc
    return BleakClient, BleakScanner


def canonical_uuid(value: Optional[str]) -> str:
    return (value or "").strip().lower()


def json_dump(value: Any, pretty: bool = False) -> str:
    if pretty:
        return json.dumps(value, ensure_ascii=False, indent=2, sort_keys=False)
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=False)


def deep_get(obj: Any, path: str) -> Any:
    current = obj
    for token in path.split("."):
        if isinstance(current, dict):
            if token not in current:
                return None
            current = current[token]
        elif isinstance(current, list) and token.isdigit():
            index = int(token)
            if index < 0 or index >= len(current):
                return None
            current = current[index]
        else:
            return None
    return current


def ensure_expectations(result: Dict[str, Any], expect: Optional[Dict[str, Any]]) -> None:
    if not expect:
        return
    for path, expected in expect.items():
        actual = deep_get(result, path)
        if actual != expected:
            raise ExpectationError(
                "Expectation failed for %s: expected %r, got %r" % (path, expected, actual)
            )


def to_int(value: Any, default: int = -999) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


@dataclass
class DeviceSelector:
    address: Optional[str]
    name: Optional[str]
    name_prefix: str
    service_uuid: str


def build_device_record(device: Any, advertisement: Any = None) -> Dict[str, Any]:
    metadata = getattr(device, "metadata", {}) or {}
    service_uuids: List[str] = []

    adv_service_uuids = getattr(advertisement, "service_uuids", None)
    if adv_service_uuids:
        service_uuids.extend([canonical_uuid(item) for item in adv_service_uuids if item])

    meta_uuids = metadata.get("uuids") if isinstance(metadata, dict) else None
    if meta_uuids:
        service_uuids.extend([canonical_uuid(item) for item in meta_uuids if item])

    deduped = []
    seen = set()
    for item in service_uuids:
        if item and item not in seen:
            deduped.append(item)
            seen.add(item)

    return {
        "name": getattr(device, "name", None) or "",
        "address": getattr(device, "address", None) or "",
        "rssi": getattr(device, "rssi", None),
        "service_uuids": deduped,
        "raw_metadata": metadata,
        "_device": device,
    }


def selector_matches(record: Dict[str, Any], selector: DeviceSelector) -> bool:
    address = (record.get("address") or "").lower()
    name = record.get("name") or ""
    uuids = [canonical_uuid(item) for item in record.get("service_uuids") or []]

    if selector.address and address != selector.address.lower():
        return False
    if selector.name and name != selector.name:
        return False
    if not selector.name and selector.name_prefix and not name.startswith(selector.name_prefix):
        return False
    if selector.service_uuid and uuids and selector.service_uuid not in uuids:
        return False
    return True


async def discover_records(timeout: float) -> List[Dict[str, Any]]:
    _, BleakScanner = load_bleak()
    try:
        discovered = await BleakScanner.discover(timeout=timeout, return_adv=True)
    except TypeError:
        discovered = await BleakScanner.discover(timeout=timeout)

    records: List[Dict[str, Any]] = []
    if isinstance(discovered, dict):
        for value in discovered.values():
            if isinstance(value, tuple) and len(value) == 2:
                device, advertisement = value
            else:
                device, advertisement = value, None
            records.append(build_device_record(device, advertisement))
    else:
        for device in discovered:
            records.append(build_device_record(device, None))

    records.sort(key=lambda item: to_int(item.get("rssi"), -999), reverse=True)
    return records


def best_match(records: Sequence[Dict[str, Any]], selector: DeviceSelector) -> Dict[str, Any]:
    matches = [record for record in records if selector_matches(record, selector)]
    if not matches:
        sample = [
            {
                "name": record.get("name"),
                "address": record.get("address"),
                "rssi": record.get("rssi"),
            }
            for record in records[:8]
        ]
        raise BleProvisionError(
            "No device matched selector. Seen devices: %s" % json_dump(sample, pretty=True)
        )

    def score(record: Dict[str, Any]) -> Tuple[int, int]:
        points = 0
        if selector.address and record.get("address", "").lower() == selector.address.lower():
            points += 1000
        if selector.name and record.get("name") == selector.name:
            points += 500
        if selector.name_prefix and (record.get("name") or "").startswith(selector.name_prefix):
            points += 100
        if selector.service_uuid and selector.service_uuid in record.get("service_uuids", []):
            points += 50
        return points, to_int(record.get("rssi"), -999)

    matches.sort(key=score, reverse=True)
    return matches[0]


class Output:
    def __init__(self, mode: str, verbose: bool = False) -> None:
        self.mode = mode
        self.verbose = verbose

    def _print(self, value: str) -> None:
        print(value)

    def info(self, message: str) -> None:
        if self.mode == "human":
            self._print(message)
        elif self.mode == "ndjson":
            self._print(json_dump({"kind": "info", "message": message}))

    def event(self, payload: Dict[str, Any]) -> None:
        if self.mode == "ndjson":
            self._print(json_dump({"kind": "event", "payload": payload}))
        elif self.mode == "human" and self.verbose:
            op = payload.get("op", "unknown")
            req_id = payload.get("req_id", "-")
            code = payload.get("code")
            msg = payload.get("msg")
            stage = deep_get(payload, "data.stage")
            suffix = " stage=%s" % stage if stage else ""
            self._print("[%s #%s] code=%s msg=%s%s" % (op, req_id, code, msg, suffix))

    def result(self, payload: Any) -> None:
        if self.mode == "human":
            self._print(json_dump(payload, pretty=True))
        elif self.mode == "json":
            self._print(json_dump(payload, pretty=True))
        else:
            if isinstance(payload, list):
                for item in payload:
                    self._print(json_dump(item))
            else:
                self._print(json_dump(payload))


class ProvisioningClient:
    def __init__(
        self,
        selector: DeviceSelector,
        output: Output,
        scan_timeout: float,
        connect_timeout: float,
    ) -> None:
        self.selector = selector
        self.output = output
        self.scan_timeout = scan_timeout
        self.connect_timeout = connect_timeout
        self.client: Any = None
        self.record: Optional[Dict[str, Any]] = None
        self._queue: "asyncio.Queue[Dict[str, Any]]" = asyncio.Queue()
        self._req_id = 0

    async def __aenter__(self) -> "ProvisioningClient":
        await self.connect()
        return self

    async def __aexit__(self, exc_type: Any, exc: Any, tb: Any) -> None:
        await self.disconnect()

    async def connect(self) -> None:
        BleakClient, _ = load_bleak()
        records = await discover_records(self.scan_timeout)
        selected = best_match(records, self.selector)
        self.record = selected
        self.output.info(
            "Connecting to %s (%s)" % (selected.get("name") or "unknown", selected.get("address"))
        )
        self.client = BleakClient(selected["_device"], timeout=self.connect_timeout)
        await self.client.connect()
        await self.client.start_notify(EVENT_UUID, self._notification_handler)

    async def disconnect(self) -> None:
        if self.client is None:
            return
        try:
            await self.client.stop_notify(EVENT_UUID)
        except Exception:
            pass
        try:
            await self.client.disconnect()
        finally:
            self.client = None

    def _notification_handler(self, _: Any, raw: bytearray) -> None:
        try:
            decoded = bytes(raw).decode("utf-8", errors="replace")
        except Exception:
            decoded = repr(raw)
        try:
            payload = json.loads(decoded)
        except json.JSONDecodeError:
            payload = {"raw": decoded, "parse_error": True}
        self._queue.put_nowait(payload)

    def _next_req_id(self) -> int:
        self._req_id += 1
        return self._req_id

    async def _drain_queue(self) -> None:
        while not self._queue.empty():
            try:
                self._queue.get_nowait()
            except asyncio.QueueEmpty:
                return

    async def _wait_for_match(
        self,
        op: str,
        req_id: int,
        timeout: float,
        kind: str,
        wait_final: bool = True,
    ) -> Dict[str, Any]:
        deadline = asyncio.get_running_loop().time() + timeout
        events: List[Dict[str, Any]] = []
        merged: Optional[Dict[str, Any]] = None

        while True:
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0:
                raise BleProvisionError("Timed out waiting for %s response" % op)

            event = await asyncio.wait_for(self._queue.get(), timeout=remaining)
            if not isinstance(event, dict):
                continue
            if event.get("op") != op or event.get("req_id") != req_id:
                self.output.event(event)
                continue

            events.append(event)
            self.output.event(event)

            if kind == "wifi_sta_apply":
                if merged is None:
                    merged = deepcopy(event)
                    merged["history"] = events
                else:
                    merged = deepcopy(event)
                    merged["history"] = deepcopy(events)
                if not wait_final:
                    return merged
                stage = deep_get(event, "data.stage")
                if event.get("code") != 0:
                    return merged
                if event.get("msg") == "Provisioned":
                    return merged
                if stage in ("done", "failed"):
                    return merged
                continue

            return event

    async def send_command(
        self,
        op: str,
        payload: Optional[Dict[str, Any]] = None,
        timeout: float = DEFAULT_COMMAND_TIMEOUT,
        kind: str = "single",
        wait_final: bool = True,
    ) -> Dict[str, Any]:
        if self.client is None:
            raise BleProvisionError("BLE client is not connected")

        await self._drain_queue()
        req_id = self._next_req_id()
        body = {"op": op, "req_id": req_id}
        if payload:
            body.update(payload)
        raw = json_dump(body).encode("utf-8")

        try:
            await self.client.write_gatt_char(COMMAND_UUID, raw, response=True)
        except Exception:
            await self.client.write_gatt_char(COMMAND_UUID, raw, response=False)

        return await self._wait_for_match(
            op=op,
            req_id=req_id,
            timeout=timeout,
            kind=kind,
            wait_final=wait_final,
        )


async def run_scan(args: argparse.Namespace, output: Output) -> Dict[str, Any]:
    records = await discover_records(args.scan_timeout)
    devices = []
    selector = DeviceSelector(
        address=args.address,
        name=args.name,
        name_prefix=args.name_prefix,
        service_uuid=canonical_uuid(args.service_uuid),
    )
    for record in records:
        matched = selector_matches(record, selector)
        if not args.all and not matched:
            continue
        devices.append(
            {
                "name": record.get("name"),
                "address": record.get("address"),
                "rssi": record.get("rssi"),
                "service_uuids": record.get("service_uuids"),
                "matched": matched,
            }
        )

    if output.mode == "human" and devices:
        for item in devices:
            marker = "*" if item["matched"] else "-"
            output.info(
                "%s %s  %s  RSSI=%s  services=%s"
                % (
                    marker,
                    item["name"] or "(unnamed)",
                    item["address"],
                    item["rssi"],
                    ",".join(item["service_uuids"] or []),
                )
            )

    return {
        "service_uuid": canonical_uuid(args.service_uuid),
        "name_filter": args.name or args.name_prefix,
        "devices": devices,
    }


async def run_single_ble_command(
    args: argparse.Namespace,
    output: Output,
    op: str,
    payload: Optional[Dict[str, Any]] = None,
    timeout: Optional[float] = None,
    kind: str = "single",
    wait_final: bool = True,
) -> Dict[str, Any]:
    selector = DeviceSelector(
        address=args.address,
        name=args.name,
        name_prefix=args.name_prefix,
        service_uuid=canonical_uuid(args.service_uuid),
    )
    async with ProvisioningClient(
        selector=selector,
        output=output,
        scan_timeout=args.scan_timeout,
        connect_timeout=args.connect_timeout,
    ) as client:
        return await client.send_command(
            op=op,
            payload=payload,
            timeout=timeout or args.timeout,
            kind=kind,
            wait_final=wait_final,
        )


async def run_suite(args: argparse.Namespace, output: Output) -> Dict[str, Any]:
    if args.suite_name != "smoke":
        raise BleProvisionError("Unknown suite: %s" % args.suite_name)

    selector = DeviceSelector(
        address=args.address,
        name=args.name,
        name_prefix=args.name_prefix,
        service_uuid=canonical_uuid(args.service_uuid),
    )
    results: List[Dict[str, Any]] = []
    async with ProvisioningClient(
        selector=selector,
        output=output,
        scan_timeout=args.scan_timeout,
        connect_timeout=args.connect_timeout,
    ) as client:
        steps = [
            ("info", {}, DEFAULT_COMMAND_TIMEOUT, "single", True, {"code": 0}),
            ("status", {}, DEFAULT_COMMAND_TIMEOUT, "single", True, {"code": 0}),
        ]
        for op, payload, timeout, kind, wait_final, expect in steps:
            output.info("Running suite step: %s" % op)
            result = await client.send_command(op, payload, timeout, kind, wait_final)
            ensure_expectations(result, expect)
            results.append({"command": op.replace("_", "-"), "result": result})

    return {
        "suite": args.suite_name,
        "ok": True,
        "steps": results,
    }


async def execute_script_step(
    step: Dict[str, Any],
    args: argparse.Namespace,
    output: Output,
    client: Optional[ProvisioningClient],
) -> Tuple[Optional[ProvisioningClient], Dict[str, Any]]:
    command = str(step.get("command", "")).strip().replace("_", "-")
    step_args = step.get("args") or {}

    if command == "sleep":
        seconds = float(step_args.get("seconds", step.get("seconds", 1)))
        output.info("Sleeping for %.1fs" % seconds)
        await asyncio.sleep(seconds)
        return client, {"command": "sleep", "seconds": seconds}

    if command == "scan":
        result = await run_scan(args, output)
        return client, result

    if client is None:
        selector = DeviceSelector(
            address=args.address,
            name=args.name,
            name_prefix=args.name_prefix,
            service_uuid=canonical_uuid(args.service_uuid),
        )
        client = ProvisioningClient(
            selector=selector,
            output=output,
            scan_timeout=args.scan_timeout,
            connect_timeout=args.connect_timeout,
        )
        await client.connect()

    payload: Dict[str, Any] = {}
    op = command.replace("-", "_")
    timeout = float(step.get("timeout", DEFAULT_COMMAND_TIMEOUT))
    kind = "single"
    wait_final = bool(step.get("wait", True))
    if command == "wifi-sta-apply":
        payload["ssid"] = step_args["ssid"]
        if "pwd" in step_args:
            payload["pwd"] = step_args["pwd"]
        if "hidden" in step_args:
            payload["hidden"] = bool(step_args["hidden"])
        if "bssid" in step_args:
            payload["bssid"] = step_args["bssid"]
        if "security" in step_args:
            payload["security"] = step_args["security"]
        timeout = float(step.get("timeout", DEFAULT_STA_TIMEOUT))
        kind = "wifi_sta_apply"
    elif command == "wifi-ap-apply":
        payload["ssid"] = step_args["ssid"]
        if "pwd" in step_args:
            payload["pwd"] = step_args["pwd"]
        if "channel" in step_args:
            payload["channel"] = int(step_args["channel"])
    elif command in ("info", "status", "reset-provision"):
        if command == "reset-provision":
            op = "reset_provision"
    else:
        raise BleProvisionError("Unsupported script command: %s" % command)

    result = await client.send_command(op, payload, timeout, kind, wait_final)
    ensure_expectations(result, step.get("expect"))
    return client, {"command": command, "result": result}


async def run_script(args: argparse.Namespace, output: Output) -> Dict[str, Any]:
    scenario = json.loads(Path(args.file).read_text(encoding="utf-8"))
    steps = scenario.get("steps")
    if not isinstance(steps, list) or not steps:
        raise BleProvisionError("Scenario file must contain a non-empty 'steps' array")

    results = []
    client: Optional[ProvisioningClient] = None
    try:
        for index, step in enumerate(steps, start=1):
            if not isinstance(step, dict):
                raise BleProvisionError("Step %d is not an object" % index)
            output.info("Running script step %d: %s" % (index, step.get("command", "unknown")))
            client, result = await execute_script_step(step, args, output, client)
            results.append({"index": index, **result})
    finally:
        if client is not None:
            await client.disconnect()

    return {
        "script": str(Path(args.file)),
        "ok": True,
        "steps": results,
    }


def build_parser() -> argparse.ArgumentParser:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--address", help="BLE MAC address to connect to")
    common.add_argument("--name", help="Exact BLE device name to connect to")
    common.add_argument(
        "--name-prefix",
        default=DEFAULT_NAME_PREFIX,
        help="Fallback BLE name prefix when --name is not provided (default: %(default)s)",
    )
    common.add_argument(
        "--service-uuid",
        default=SERVICE_UUID,
        help="Provisioning service UUID (default: %(default)s)",
    )
    common.add_argument(
        "--scan-timeout",
        type=float,
        default=DEFAULT_SCAN_TIMEOUT,
        help="Discovery timeout in seconds (default: %(default)s)",
    )
    common.add_argument(
        "--connect-timeout",
        type=float,
        default=DEFAULT_CONNECT_TIMEOUT,
        help="Connect timeout in seconds (default: %(default)s)",
    )
    common.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_COMMAND_TIMEOUT,
        help="Default command timeout in seconds (default: %(default)s)",
    )
    common.add_argument(
        "--format",
        choices=("human", "json", "ndjson"),
        default="human",
        help="Output format (default: %(default)s)",
    )
    common.add_argument(
        "--verbose",
        action="store_true",
        help="Print matched BLE events as they arrive in human mode",
    )

    parser = argparse.ArgumentParser(
        description="Parkia.Cube BLE provisioning CLI",
        epilog="Common selector and output flags are available on every subcommand.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    scan_parser = subparsers.add_parser(
        "scan", parents=[common], help="Discover nearby provisioning devices"
    )
    scan_parser.add_argument("--all", action="store_true", help="Show all BLE devices, not just Parkia matches")

    subparsers.add_parser("info", parents=[common], help="Read BLE provisioning info")

    subparsers.add_parser("status", parents=[common], help="Read BLE provisioning status")

    sta_parser = subparsers.add_parser(
        "wifi-sta-apply", parents=[common], help="Run BLE wifi_sta_apply"
    )
    sta_parser.add_argument("--ssid", required=True, help="Target Wi-Fi SSID")
    sta_parser.add_argument("--pwd", default="", help="Target Wi-Fi password")
    sta_parser.add_argument("--hidden", action="store_true", help="Set hidden=true in the BLE payload")
    sta_parser.add_argument("--bssid", help="Optional BSSID")
    sta_parser.add_argument("--security", help="Optional security string")
    sta_parser.add_argument(
        "--no-wait",
        action="store_true",
        help="Return after the initial Accepted/first response instead of waiting for the final event",
    )

    ap_parser = subparsers.add_parser(
        "wifi-ap-apply", parents=[common], help="Run BLE wifi_ap_apply"
    )
    ap_parser.add_argument("--ssid", required=True, help="Hotspot SSID to apply")
    ap_parser.add_argument("--pwd", default="", help="Hotspot password (empty = open hotspot)")
    ap_parser.add_argument("--channel", type=int, default=0, help="Hotspot channel (0 = auto)")

    subparsers.add_parser(
        "reset-provision", parents=[common], help="Run BLE reset_provision"
    )

    suite_parser = subparsers.add_parser(
        "suite", parents=[common], help="Run a built-in regression suite"
    )
    suite_parser.add_argument("suite_name", choices=("smoke",), help="Suite to run")

    script_parser = subparsers.add_parser(
        "script", parents=[common], help="Run a JSON scenario file"
    )
    script_parser.add_argument("--file", required=True, help="Path to a JSON scenario file")

    return parser


async def async_main(args: argparse.Namespace) -> Dict[str, Any]:
    output = Output(mode=args.format, verbose=args.verbose)
    if args.command == "scan":
        return await run_scan(args, output)
    if args.command == "info":
        return await run_single_ble_command(args, output, "info")
    if args.command == "status":
        return await run_single_ble_command(args, output, "status")
    if args.command == "wifi-sta-apply":
        payload = {
            "ssid": args.ssid,
            "pwd": args.pwd,
            "hidden": args.hidden,
        }
        if args.bssid:
            payload["bssid"] = args.bssid
        if args.security:
            payload["security"] = args.security
        return await run_single_ble_command(
            args,
            output,
            "wifi_sta_apply",
            payload=payload,
            timeout=max(args.timeout, DEFAULT_STA_TIMEOUT),
            kind="wifi_sta_apply",
            wait_final=not args.no_wait,
        )
    if args.command == "wifi-ap-apply":
        payload = {"ssid": args.ssid, "pwd": args.pwd, "channel": args.channel}
        return await run_single_ble_command(args, output, "wifi_ap_apply", payload=payload)
    if args.command == "reset-provision":
        return await run_single_ble_command(args, output, "reset_provision")
    if args.command == "suite":
        return await run_suite(args, output)
    if args.command == "script":
        return await run_script(args, output)
    raise BleProvisionError("Unhandled command: %s" % args.command)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        result = asyncio.run(async_main(args))
    except ExpectationError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except BleProvisionError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted", file=sys.stderr)
        return 130

    Output(mode=args.format, verbose=args.verbose).result(result)
    return 0


if __name__ == "__main__":
    sys.exit(main())
