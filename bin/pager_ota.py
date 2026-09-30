#!/usr/bin/env python3
"""Firmware update of a pager over BLE (FRD-024).

Reference implementation of docs/pager_config_protocol.md (section 5) for the web app,
and a test tool for developers.

Needs:  pip install bleak

  bin/pager_ota.py info   <address>
  bin/pager_ota.py update <address> <image>

<image>: ESP32 boards (V3, V4): the app image  <env>-<version>-<sha>.bin  (NOT -merged.bin)
         T114:                  the DFU package <env>-<version>-<sha>.zip
The pager has to run an MSPager firmware with update support (0x71). First install: USB.
"""
import argparse
import asyncio
import hashlib
import io
import struct
import sys
import time
import zipfile

from bleak import BleakClient, BleakScanner

UART_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
UART_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
UART_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# Nordic legacy DFU (Adafruit nRF52 bootloader)
DFU_SERVICE = "00001530-1212-efde-1523-785feabcd123"
DFU_CONTROL = "00001531-1212-efde-1523-785feabcd123"
DFU_PACKET = "00001532-1212-efde-1523-785feabcd123"

CMD_PAGER_OTA = 0x71
OP_INFO, OP_BEGIN, OP_DATA, OP_END, OP_ABORT, OP_DFU = 0, 1, 2, 3, 4, 0x10
RESP_OK, RESP_ERR = 0x00, 0x01
MARKER = b"MSPAGER-BOARD:"
CHUNK = 160      # image bytes per DATA frame (frame = 7 + CHUNK <= 173 = MTU 176 - 3)
WINDOW = 4       # DATA frames per acknowledgement (= the pager's BLE receive queue)
DFU_PKT = 20     # legacy DFU packet size
DFU_PRN = 10     # packet receipt notification interval


def board_of(image):
    i = image.find(MARKER)
    if i < 0:
        return None
    end = image.index(b"\0", i)
    return image[i + len(MARKER):end].decode()


class Pager:
    def __init__(self, client):
        self.client = client
        self.frames = asyncio.Queue()

    async def start(self):
        await self.client.start_notify(UART_TX, lambda _, d: self.frames.put_nowait(bytes(d)))

    async def send(self, frame):
        await self.client.write_gatt_char(UART_RX, frame, response=True)

    async def reply(self, timeout=15.0):
        while True:
            r = await asyncio.wait_for(self.frames.get(), timeout)
            if r and r[0] in (RESP_OK, RESP_ERR, CMD_PAGER_OTA):
                return r   # anything else is an unrelated push (0x80+)

    async def ota(self, op, payload=b"", timeout=15.0):
        await self.send(bytes([CMD_PAGER_OTA, op]) + payload)
        r = await self.reply(timeout)
        if r[0] == RESP_ERR:
            raise SystemExit("error: command not supported (no update support in this firmware?)")
        if r[0] == RESP_OK:
            return 0, ""
        return r[1], r[4:].decode("utf-8", "replace")


def parse_info(msg):
    return dict(kv.split("=", 1) for kv in msg.split() if "=" in kv)


async def connect(address):
    client = BleakClient(address)
    await client.connect()
    try:
        await client.pair()
    except Exception:
        pass
    pager = Pager(client)
    await pager.start()
    return client, pager


async def update_esp32(pager, image):
    md5 = hashlib.md5(image).digest()
    st, msg = await pager.ota(OP_BEGIN, struct.pack("<I", len(image)) + md5)
    if st:
        raise SystemExit(f"begin: {msg}")
    t0 = time.time()
    frames = [(off, image[off:off + CHUNK]) for off in range(0, len(image), CHUNK)]
    for k, (off, chunk) in enumerate(frames):
        ack = (k % WINDOW == WINDOW - 1) or k == len(frames) - 1
        await pager.send(bytes([CMD_PAGER_OTA, OP_DATA]) + struct.pack("<IB", off, ack) + chunk)
        if ack:
            r = await pager.reply()
            if r[0] != RESP_OK:
                raise SystemExit(f"data @{off}: {r[4:].decode('utf-8', 'replace')}")
            done = off + len(chunk)
            rate = done / max(time.time() - t0, 0.001) / 1024
            print(f"\r{done * 100 // len(image):3d}%  {done}/{len(image)}  {rate:.1f} KB/s", end="", flush=True)
    print()
    st, msg = await pager.ota(OP_END, timeout=30.0)
    if st:
        raise SystemExit(f"end: {msg}")
    print(msg)


async def dfu_nrf(client, pager, zip_bytes):
    z = zipfile.ZipFile(io.BytesIO(zip_bytes))
    image, init = z.read("firmware.bin"), z.read("firmware.dat")
    st, msg = await pager.ota(OP_DFU)
    print(msg)
    try:
        await client.disconnect()
    except Exception:
        pass

    print("waiting for the DFU bootloader ...")
    dev = await BleakScanner.find_device_by_filter(
        lambda d, adv: DFU_SERVICE in [u.lower() for u in adv.service_uuids], timeout=30.0)
    if dev is None:
        raise SystemExit("DFU bootloader not found")
    print(f"bootloader: {dev.name} {dev.address}")

    async with BleakClient(dev) as dfu:
        notes = asyncio.Queue()
        await dfu.start_notify(DFU_CONTROL, lambda _, d: notes.put_nowait(bytes(d)))

        async def expect(req):
            while True:
                r = await asyncio.wait_for(notes.get(), 60.0)
                if r[0] == 0x10 and r[1] == req:
                    if r[2] != 1:
                        raise SystemExit(f"DFU op {req} failed, status {r[2]}")
                    return

        async def ctrl(data):
            await dfu.write_gatt_char(DFU_CONTROL, bytes(data), response=True)

        async def packet(data):
            await dfu.write_gatt_char(DFU_PACKET, bytes(data), response=False)

        await ctrl([0x01, 0x04])                                  # START_DFU, application
        await packet(struct.pack("<III", 0, 0, len(image)))       # sizes: softdevice, bootloader, app
        await expect(0x01)
        await ctrl([0x02, 0x00])                                  # INIT_DFU_PARAMS start
        await packet(init)
        await ctrl([0x02, 0x01])                                  # INIT_DFU_PARAMS complete
        await expect(0x02)
        await ctrl([0x08, DFU_PRN, 0])                            # packet receipt notifications
        await ctrl([0x03])                                        # RECEIVE_FIRMWARE_IMAGE
        t0 = time.time()
        for k, off in enumerate(range(0, len(image), DFU_PKT)):
            await packet(image[off:off + DFU_PKT])
            if k % DFU_PRN == DFU_PRN - 1:
                while True:
                    r = await asyncio.wait_for(notes.get(), 30.0)
                    if r[0] == 0x11:
                        break
                    if r[0] == 0x10:
                        raise SystemExit(f"DFU aborted: {r.hex()}")
                done = struct.unpack_from("<I", r, 1)[0]
                rate = done / max(time.time() - t0, 0.001) / 1024
                print(f"\r{done * 100 // len(image):3d}%  {done}/{len(image)}  {rate:.1f} KB/s", end="", flush=True)
        print()
        await expect(0x03)
        await ctrl([0x04])                                        # VALIDATE
        await expect(0x04)
        try:
            await ctrl([0x05])                                    # ACTIVATE_AND_RESET
        except Exception:
            pass   # the bootloader resets right away
    print("OK, restarting")


async def run(args):
    client, pager = await connect(args.address)
    try:
        st, msg = await pager.ota(OP_INFO)
        print(msg)
        if args.action == "info":
            return
        info = parse_info(msg)
        data = open(args.image, "rb").read()
        if info.get("ota") == "esp32":
            if data[:1] != b"\xe9" or struct.unpack_from("<I", data, 32)[0] != 0xABCD5432:
                raise SystemExit("not an ESP32 app image (use the .bin without -merged)")
            image = data
        elif info.get("ota") == "nrf-dfu":
            try:
                image = zipfile.ZipFile(io.BytesIO(data)).read("firmware.bin")
            except Exception:
                raise SystemExit("not a DFU package (use the .zip)")
        else:
            raise SystemExit("this pager can't be updated over BLE")
        if board_of(image) != info.get("board"):
            raise SystemExit(f"image is for {board_of(image)}, pager is {info.get('board')}")
        if info["ota"] == "esp32":
            await update_esp32(pager, image)
        else:
            await dfu_nrf(client, pager, data)
    finally:
        if client.is_connected:
            await client.disconnect()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="action", required=True)
    sub.add_parser("info").add_argument("address")
    up = sub.add_parser("update")
    up.add_argument("address")
    up.add_argument("image")
    asyncio.run(run(ap.parse_args()))


if __name__ == "__main__":
    sys.exit(main())
