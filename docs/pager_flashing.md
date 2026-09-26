# MSPager — Flashing & Setup Guide

This guide covers everything from a new Heltec V4 in its box to a working pager in the group. Budget about 10 minutes per device, plus a few minutes outdoors for the first GPS fix.

> Product context: [PRD.md](../PRD.md) · Requirements: [FRD.md](../FRD.md)

---

## 1. What you need

| Item | Notes |
|---|---|
| Heltec WiFi LoRa 32 **V4** with the **0.96" OLED** | The TFT variant is **not** supported |
| Heltec V4 **GPS module** + cable | Plug it into the GPS connector *before* powering on |
| LoRa antenna | **Always attach it before powering on.** Transmitting without an antenna can damage the radio |
| 1S LiPo battery (JST 1.25) | Optional for flashing, needed for mobile use |
| USB-C **data** cable | Charge-only cables won't work |
| Computer with **Chrome or Edge** | For the web flasher (WebSerial). Firefox/Safari don't support it |
| Smartphone with the **MeshCore app** | Android/iOS, for configuration over Bluetooth |
| Firmware file | `heltec_v4_pager-<version>-<sha>-merged.bin` (from a release, or built yourself, see §3) |

### Buttons on the Heltec V4
- **PRG**: the only button the pager firmware uses (short / long / double press).
- **RST**: hardware reset. It **always reboots the device** and can't be used for anything else (it's wired to the ESP32's EN pin).

---

## 2. Put the board into bootloader mode (if needed)

Usually the flasher resets the board automatically. If the port doesn't show up, or flashing fails at "Connecting…":

1. Hold **PRG**.
2. Press and release **RST**.
3. Release **PRG**.

The board is now in download mode (the display stays dark). Start flashing again.

---

## 3. Get the firmware

### Option A: release download
Download `heltec_v4_pager-<version>-<sha>-merged.bin` from the project's releases page.

### Option B: build it yourself
Requirements: Python 3, [PlatformIO Core](https://platformio.org/install/cli) (or VS Code + PlatformIO extension), git.

```sh
git clone <this-repo-url> MSPager
cd MSPager

# Quick build + direct upload to a connected board:
pio run -e heltec_v4_pager -t upload

# Or build release files (merged + app-only) into ./out:
cp pager.secret.ini.example pager.secret.ini   # then put the group key in it (see §5.3)
export FIRMWARE_VERSION=v0.1.0
sh build.sh build-firmware heltec_v4_pager
ls out/
#   heltec_v4_pager-v0.1.0-<sha>.bin          (app only, for updates)
#   heltec_v4_pager-v0.1.0-<sha>-merged.bin   (full image, for first install)
```

**Which file?**
- **`-merged.bin`**: bootloader + partitions + app. Use it for the **first install**, flashed at offset `0x0`. It also clears the Bluetooth pairing database, but keeps settings.
- **non-merged `.bin`**: app only, flashed at `0x10000`. Use it for **updates** (keeps Bluetooth pairings).

---

## 4. Flash the firmware

Pick **one** of these options.

### Option 1: MeshCore web flasher (recommended)
1. Connect the pager via USB-C. Open <https://flasher.meshcore.io> in Chrome/Edge.
2. Choose the **custom firmware** option (upload your own `.bin`) and select `heltec_v4_pager-…-merged.bin`.
3. **First install only:** click **Erase Flash** and select the USB device. Wait until it finishes. This removes old MeshCore settings and channels.
4. Click **Flash!** and select the USB device again. Wait for 100%.
5. Press **RST**. The OLED shows the Bluetooth PIN.

### Option 2: Espressif web tool
If the MeshCore flasher doesn't offer a custom upload:
1. Open <https://espressif.github.io/esptool-js/> in Chrome/Edge and click **Connect**.
2. First install: **Erase Flash**.
3. Flash address `0x0`, file `…-merged.bin`, then **Program**.
4. Press **RST**.

### Option 3: esptool (command line)
```sh
pip install esptool
# find the port: macOS /dev/cu.usbmodem* or /dev/cu.usbserial*, Linux /dev/ttyACM0 or /dev/ttyUSB0, Windows COMx

# first install (full erase + merged image)
esptool.py --chip esp32s3 -p <PORT> erase_flash
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 heltec_v4_pager-<version>-<sha>-merged.bin

# later updates (keeps settings and pairings)
esptool.py --chip esp32s3 -p <PORT> write_flash 0x10000 heltec_v4_pager-<version>-<sha>.bin
```

> **Port doesn't show up?** Try a different cable/USB port, use bootloader mode (§2), and on older systems install the CP210x or CH34x USB-serial driver.

---

## 5. First-time setup (per pager, via the MeshCore app)

All pagers in a group need the **same radio preset** and the **same `Pager` channel with the same key**. Only the nickname differs.

### 5.1 Connect
1. Open the MeshCore app and connect to the device over Bluetooth (it shows up as `MeshCore-…`).
2. Enter the **PIN shown on the pager's OLED**. It's shown on the boot screen for the first 5 s. Later, **hold PRG for 10 s** (pairing mode) to show it in large digits.

### 5.2 Nickname
- In the app settings, set the **node name** to the pager's nickname, e.g. `Anna`.
- Keep it **≤ 10 characters**. Longer names get cut off on the small display.

### 5.3 Radio preset & 5.4 Pager channel: automatic
Nothing to set in the app. Every pager applies the **group config** at every boot. It's compiled in from [`pager.ini`](../pager.ini) (channel name, radio) and **`pager.secret.ini`** (the group key; not in git, create it from `pager.secret.ini.example`):
- channel **`Pager`** with the group key, visible in the app's channel list
- radio preset **EU/UK (Narrow)**: 869.618 MHz, BW 62.5, SF 8, CR 5

All pagers flashed from the same checkout meet in the same channel right after flashing. Changes made in the app to this channel or the radio settings are reset on the next reboot.

**Group key:** everyone who builds pagers for the group needs the **same `pager.secret.ini`**. Pass it on privately; it's gitignored and must never be committed (the repo is public).
**Separate group or new key:** generate a new key with `openssl rand -hex 16`, put it into `pager.secret.ini`, rebuild, and **reflash every pager**.

> Anyone who has `pager.secret.ini` or a built `.bin` can read the group's messages. Don't publish `.bin` files.
> To use existing MeshCore repeaters, they must run the same radio preset.

### 5.5 GPS first fix
The header shows a **location pin** left of the battery when the pager has a GPS fix, and a **slashed pin** when it doesn't.

- Take the pager **outdoors** with a clear view of the sky for **2–5 minutes** the first time.
- Messages sent before the first fix carry `[no GPS]`. After that they carry the current or last known position.

### 5.6 Done
Disconnect the app (Bluetooth stays on, and the app can reconnect any time). Press **RST** once to check the settings survive a reboot.

---

## 6. Using the pager (quick reference)

| You want to… | Do this |
|---|---|
| Wake the display | Press **PRG** once. The first press only wakes it |
| See messages | They're on the main screen with the battery level. Newest at the bottom |
| Send "Angekommen?" / "Brauche Hilfe" | **Hold** PRG → **short press** to choose → **hold** to send |
| Read a message's details (distance, direction, coordinates) | **Short press** to select it → **hold** |
| Reply "Ja" / "Nein" / "OK" / 📞 | In the message details: **hold** → **short press** to choose → **hold** to send |
| Ask where the others are | **Hold** → choose `Standort?` → **hold**. The other pagers answer automatically with `Mein Standort` and their distance/direction |
| Share your position | **Hold** → choose `Mein Standort` → **hold** |
| Go back / cancel | **Double press** |
| Turn the display off | **Double press** on the main screen, or wait 15 s |
| Pair with the app (pager not found in the app) | From the main screen, **hold PRG for 10 s**. The large PIN appears for 30 s, so connect the app now |

New message: the display lights up and the **LED blinks** until you press the button once on the message overview. That first press only confirms; it doesn't select anything.

---

## 7. Test with two pagers

1. Pager A: send **Angekommen?**
2. Pager B: the display wakes, the LED blinks, and it shows `Anna: Angekommen?`.
3. Pager B: select the message, hold, and reply **Ja**.
4. Pager A: shows `Ben: @Anna Ja`. The details show Ben's distance/direction (if both have a GPS fix).

Optional: a phone with the MeshCore app joined to the `Pager` channel sees the same messages as text, e.g. `Anna: Angekommen? [52.5201,13.4050]`.

---

## 8. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Display shows `No 'Pager' channel` | The channel was renamed or deleted in the app. Press RST: it's restored from `pager.ini` |
| Messages don't arrive | Pagers built from different `pager.ini` versions (different key/preset). Reflash all from the same commit. Also check that the nicknames differ (own nickname = shown as `me`, no alert) |
| Only some messages arrive | Out of range. Add or position a MeshCore repeater, and check the antenna |
| Always `[no GPS]` | GPS module not plugged in before power-on, or no fix yet. Go outdoors and wait a few minutes |
| Position is old (`~45min`) | No current fix (indoors). It shows the last known position and its age |
| Pager only visible in the app shortly after boot | Something else holds its Bluetooth link (the app reconnecting in the background, another phone), and the pager stops advertising while connected. **Hold PRG 10 s** (pairing mode) to drop that link and advertise again. Close the MeshCore app on other phones |
| Can't connect with the app | Remove the old Bluetooth pairing in the phone's settings and reconnect. After a merged flash, a re-pair is always needed |
| Flashing hangs at "Connecting…" | Use bootloader mode (§2) and a different USB cable |
| Pager behaves oddly after an update | Do a full reinstall: **Erase Flash** + merged image (§4), then repeat §5 |

For generic MeshCore flashing and reset topics, see also [docs/faq.md](faq.md).
