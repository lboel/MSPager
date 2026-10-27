# AGENTS.md — MSPager

MSPager is a single-button LoRa pager built on MeshCore (companion firmware + `examples/companion_radio/ui-pager`).
Product/requirements: `PRD.md`, `FRD.md`, `frd/`. Decisions and history: `DEVLOG.md` (newest on top, add an entry per change).

## Rules

- **The channel key is secret.** It lives only in the gitignored `pager.secret.ini`; `origin` is a public fork.
  Before every push: `git log -p origin/<branch>..<branch> | grep -ci <key>` must print `0`.
  Built firmware files contain the key as a fallback: don't publish them.
- Work on the feature branch (`mspager-m1`), not `main`. Push after each commit.
- Local setup YAMLs (they contain group keys) go in `pager-setups/` (gitignored), not `out/`: `build.sh` deletes `out/`.

## Boards and build envs

| Board | Env | MCU | USB port (macOS) | Update files |
|---|---|---|---|---|
| Heltec V4 (OLED) | `heltec_v4_pager` | ESP32-S3, 16 MB | `/dev/cu.usbmodem*` (native USB) | `.bin` (app), `-merged.bin` (full) |
| Heltec V3 | `heltec_v3_pager` | ESP32-S3, 8 MB | `/dev/cu.usbserial-*` (CP2102, `10C4:EA60`) | `.bin` (app), `-merged.bin` (full) |
| Heltec T114 (TFT) | `heltec_t114_pager` | nRF52840 | `/dev/cu.usbmodem*` (`239A:xxxx`, "HT-n5262") | `.zip` (DFU), `.uf2` |

Identify connected boards: `pio device list`. Tell V3 from V4: `python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --port <port> flash_id` (8 MB = V3, 16 MB = V4).
Each board needs its own file; they aren't interchangeable. Every image contains `MSPAGER-BOARD:<env>`.

## Build

Prerequisites:
- `pager.secret.ini` (copy `pager.secret.ini.example`, 32 hex chars). The build fails without it.
- Apple Silicon: the nRF52 (T114) toolchain is x86_64 and needs Rosetta: `softwareupdate --install-rosetta --agree-to-license`.

```sh
pio run -e heltec_v3_pager                              # compile one env
FIRMWARE_VERSION=v0.5.0 sh build.sh build-pager-firmwares   # all *_pager envs -> out/ (deletes out/ first)
FIRMWARE_VERSION=v0.5.0 sh build.sh build-firmware heltec_t114_pager
```

`build.sh` output in `out/`: `<env>-<version>-<sha>.bin` + `-merged.bin` (ESP32), `.uf2` + `.zip` (T114).
`pio run` alone doesn't set `FIRMWARE_VERSION`, so the boot screen shows no version string.

Native tests (macOS clang needs the extra include; upstream `ConfigSerializer.cpp` lacks `<stdlib.h>`):

```sh
PLATFORMIO_BUILD_FLAGS="-include stdlib.h" pio test -e native
```

## Flash over USB

Always pass `--upload-port` when more than one board is connected; auto-detection can pick the wrong one.

### V3 / V4 (ESP32-S3)

Update, keeps settings, setup and Bluetooth pairings:

```sh
pio run -e heltec_v3_pager -t upload --upload-port /dev/cu.usbserial-0001
# or a built file, app image at 0x10000:
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port <port> --baud 921600 \
  write_flash 0x10000 out/heltec_v3_pager-<version>-<sha>.bin
```

First install, full image at `0x0`:

```sh
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port <port> --baud 921600 \
  write_flash 0x0 out/heltec_v3_pager-<version>-<sha>-merged.bin
```

- The merged image overwrites NVS (`0x9000`) and **deletes the Bluetooth pairings**. Paired phones/computers must remove the pager and pair again ("Peer removed pairing information" on macOS). Setup and prefs (SPIFFS) survive.
- `erase_flash` before the merged image only for a clean start; it also deletes the node identity and setup. Ask first.
- V4 won't connect: hold PRG, press RST, release PRG (download mode). The V3 resets into download mode by itself.

### T114 (nRF52840)

```sh
pio run -e heltec_t114_pager -t upload --upload-port /dev/cu.usbmodem101
```

With a built `.zip` (serial DFU):

```sh
NRFUTIL="$HOME/.platformio/penv/bin/python $HOME/.platformio/packages/tool-adafruit-nrfutil/adafruit-nrfutil.py"
$NRFUTIL dfu serial -pkg out/heltec_t114_pager-<version>-<sha>.zip -p /dev/cu.usbmodem101 -b 115200 --singlebank --touch 1200
```

- `--touch 1200` reboots the board into the bootloader, which re-enumerates (USB PID changes, e.g. `4405`/`8029` app → `0071` bootloader).
  If nrfutil then reports "could not open port", the port came back too late: run the same command again **without** `--touch 1200`.
- Alternative: double-press RST, a USB drive appears, copy the `.uf2` onto it.
- Settings and pairings survive.

### Check after flashing

The pager firmware prints nothing on serial. A clean boot on ESP32 shows only the ROM lines ending in `entry 0x403c98d0`, with no reboot loop.
On the T114, the USB PID returns to the app PID. Everything else needs a look at the display.

## Update over Bluetooth (FRD-024)

Only for pagers that already run firmware with update support (`0x71`); first install is USB.

```sh
pip install bleak
bin/pager_ota.py info   <ble-address>
bin/pager_ota.py update <ble-address> out/heltec_v3_pager-<version>-<sha>.bin    # V3/V4: app .bin, not -merged
bin/pager_ota.py update <ble-address> out/heltec_t114_pager-<version>-<sha>.zip  # T114: DFU .zip
```

The web app (`webapp/`, serve with `python3 -m http.server 8000 -d webapp`) does the same from Chrome/Edge, also on Android.
Pairing needs the PIN from the pager's display (hold the button 10 s). Protocol: `docs/pager_config_protocol.md` §5.

## Setup over Bluetooth (FRD-021)

```sh
bin/pager_setup.py scan
bin/pager_setup.py upload <ble-address> pager-setups/<file>.yaml
bin/pager_setup.py status <ble-address>
```

Format and limits: `docs/pager_config_protocol.md` §1. All pagers of a group need the same `channel`, `radio` and `questions`.
