#!/usr/bin/env python3
"""Upload an MSPager setup YAML to a pager over BLE (FRD-021).

Reference implementation of docs/pager_config_protocol.md for the setup frontend,
and a test tool for developers.

Needs:  pip install bleak

  bin/pager_setup.py scan
  bin/pager_setup.py upload <address> setup.yaml
  bin/pager_setup.py status <address>
  bin/pager_setup.py clear  <address>

The OS asks for the BLE PIN shown on the pager (hold PRG for 10 s to show it).
On Linux, pairing may need to be done once with `bluetoothctl` (pair <address>).
"""
import argparse
import asyncio
import struct
import sys

from bleak import BleakClient, BleakScanner

UART_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
UART_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"   # write: app -> pager, one frame per write
UART_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   # notify: pager -> app, one frame per notification

CMD_PAGER_CONFIG = 0x70
OP_BEGIN, OP_DATA, OP_COMMIT, OP_STATUS, OP_CLEAR = 1, 2, 3, 4, 5
RESP_OK, RESP_ERR, RESP_PAGER_CONFIG = 0x00, 0x01, 0x70
YAML_MAX = 4096
CHUNK = 128
ERR_NAMES = {1: "unsupported command (not a pager firmware?)", 2: "not found", 3: "table full",
             4: "bad state (upload out of order)", 5: "file I/O", 6: "illegal argument (empty or too large)"}
STATUS_NAMES = {0: "OK", 1: "invalid setup", 2: "flash write failed"}


class Pager:
    def __init__(self, client):
        self.client = client
        self.frames = asyncio.Queue()

    async def start(self):
        await self.client.start_notify(UART_TX, lambda _, data: self.frames.put_nowait(bytes(data)))

    async def request(self, frame, timeout=10.0):
        await self.client.write_gatt_char(UART_RX, frame, response=True)
        while True:
            resp = await asyncio.wait_for(self.frames.get(), timeout)
            if resp and resp[0] in (RESP_OK, RESP_ERR, RESP_PAGER_CONFIG):
                return resp
            # anything else is an unrelated push (0x80+), e.g. a received message: ignore

    async def expect_ok(self, frame):
        resp = await self.request(frame)
        if resp[0] == RESP_ERR:
            code = resp[1] if len(resp) > 1 else 0
            raise SystemExit(f"error: {ERR_NAMES.get(code, code)}")
        if resp[0] != RESP_OK:
            raise SystemExit(f"error: unexpected response 0x{resp[0]:02x}")

    async def config_cmd(self, op, payload=b""):
        resp = await self.request(bytes([CMD_PAGER_CONFIG, op]) + payload)
        if resp[0] == RESP_ERR:
            code = resp[1] if len(resp) > 1 else 0
            raise SystemExit(f"error: {ERR_NAMES.get(code, code)}")
        status, line = resp[1], struct.unpack_from("<H", resp, 2)[0]
        return status, line, resp[4:].decode("utf-8", "replace")


async def scan():
    devices = await BleakScanner.discover(timeout=5.0, service_uuids=[UART_SERVICE])
    for d in devices:
        if d.name and d.name.startswith("MeshCore-"):
            print(f"{d.address}  {d.name}")


async def run(address, action, path=None):
    async with BleakClient(address) as client:
        try:
            await client.pair()
        except Exception:
            pass   # already paired, or pairing is left to the OS agent
        pager = Pager(client)
        await pager.start()

        if action == "upload":
            data = open(path, "rb").read()
            if not 0 < len(data) <= YAML_MAX:
                raise SystemExit(f"error: {path} must be 1..{YAML_MAX} bytes")
            await pager.expect_ok(bytes([CMD_PAGER_CONFIG, OP_BEGIN]) + struct.pack("<H", len(data)))
            for off in range(0, len(data), CHUNK):
                await pager.expect_ok(bytes([CMD_PAGER_CONFIG, OP_DATA]) + struct.pack("<H", off) + data[off:off + CHUNK])
            status, line, msg = await pager.config_cmd(OP_COMMIT)
        elif action == "status":
            status, line, msg = await pager.config_cmd(OP_STATUS)
        else:
            status, line, msg = await pager.config_cmd(OP_CLEAR)

        if status == 0:
            print(msg)
        elif line:
            raise SystemExit(f"{path}:{line}: {msg}")
        else:
            raise SystemExit(f"{STATUS_NAMES.get(status, status)}: {msg}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="action", required=True)
    sub.add_parser("scan")
    up = sub.add_parser("upload")
    up.add_argument("address")
    up.add_argument("yaml")
    for name in ("status", "clear"):
        sub.add_parser(name).add_argument("address")
    args = ap.parse_args()

    if args.action == "scan":
        asyncio.run(scan())
    else:
        asyncio.run(run(args.address, args.action, getattr(args, "yaml", None)))


if __name__ == "__main__":
    sys.exit(main())
