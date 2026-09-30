# MSPager — Setup over BLE (YAML upload protocol)

This is for whoever builds the **setup frontend**. It turns the setup of a pager (nickname, group channel, extra questions with their replies) into a YAML file and uploads it to an already flashed pager over Bluetooth. The pager parses the YAML itself, rejects invalid files with a line number and a reason, and stores the result as a compact binary blob in flash.

> Requirement: [FRD-021](../frd/FRD-021-ble-setup-yaml.md) · Example: [pager_setup_example.yaml](pager_setup_example.yaml) · Reference client: [`bin/pager_setup.py`](../bin/pager_setup.py)

---

## 1. YAML format (version 1)

```yaml
version: 1
nickname: Anna                              # per pager
radio:                                      # same for the whole group
  region: EU                                # country/band; EU/UK/US/CA have a preset
channel:                                    # same for the whole group
  name: Familie
  key: 8b3387e9c5cdea6ac9e5edbaa115cd72
questions:                                  # same for the whole group
  - text: Wann kommst du?
    replies: ["5 min", "30 min", "Später", "📞"]
  - text: Essen ist fertig
    replies:
      - Komme
      - Später
```

| Key | Type | Rules | If missing |
|---|---|---|---|
| `version` | number | must be `1` | treated as 1 |
| `nickname` | text | 1–31 bytes UTF-8. No `:`, `[` or `]` (they would break sender and mention parsing) | node name stays as it is |
| `channel.name` | text | 1–31 bytes. `Public` is reserved | `channel_name` from `pager.ini` (`Pager`) |
| `channel.key` | text | exactly **32 hex characters** (128-bit key, as in `openssl rand -hex 16`) | key from `pager.secret.ini` |
| `questions` | list, max **12** | each has `text` (1–40 bytes, unique) and `replies` (list of **1–6** texts, 1–40 bytes each) | only the built-in catalogue |
| `radio.region` | text | **required when `radio` is present.** `EU`, `UK`, `US`, `CA`, `AU`, `NZ`, `IN`, `KR` (not case-sensitive). Sets the **allowed band** and, for EU/UK/US/CA, a **preset** | radio from `pager.ini` (EU/UK Narrow) |
| `radio.frequency` | MHz, ≤ 3 decimals | the whole channel (frequency ± bandwidth/2) must lie inside the region's band | preset of the region (required for AU/NZ/IN/KR) |
| `radio.bandwidth` | kHz | one of `7.8 10.4 15.6 20.8 31.25 41.7 62.5 125 250 500` | preset |
| `radio.spreading_factor` | number | 5–12 | preset |
| `radio.coding_rate` | number | 5–8 (= 4/5 … 4/8) | preset |

- **Everything is optional.** A value in the YAML overrides the build config, a missing one keeps it. An upload **replaces** the whole previous setup (no merging). A file without any values (e.g. only comments) goes back to the build config, just like `CLEAR`.
- **Built-in catalogue** (always present): questions `Angekommen?`, `Brauche Hilfe`, `Standort?`, `Mein Standort`. Default replies: `Ja`, `Nein`, `OK`, `📞`, `Standort?`, `Mein Standort`. A setup question with one of the built-in texts adds nothing to the send menu but **replaces that question's replies**.
- **How replies work:** when the user replies to a message, the pager compares the message text (without the `@[Nick] ` mention) with the question texts in **its own** setup. If one matches, it offers that question's replies; otherwise it offers the default replies. So **all pagers of a group need the same `channel` and `questions`**. Only `nickname` differs. The frontend should generate one file per pager from a shared group definition.
- `Mein Standort` works as a reply too: receivers show distance and direction. `📞` and 24 more emoji are drawn as pictures ([list](pager_emoji.md), kids example: [pager_setup_kids.yaml](pager_setup_kids.yaml)). `ä ö ü Ä Ö Ü ß é °` show correctly on the OLED. Other non-ASCII characters show as a block (in the app they show normally).
- **Length budget:** each text is at most 40 bytes, so a reply with a mention and a position suffix stays within the 160-byte MeshCore text limit (FRD-012). Note that `ä` is 2 bytes and `📞` is 4.

### Regions and radio presets

| `region` | Allowed band | Preset (frequency / bandwidth / SF / CR) |
|---|---|---|
| `EU`, `UK` | 863–870 MHz | **869.618 MHz / 62.5 kHz / 8 / 5** (MeshCore "EU/UK (Narrow)", same as `pager.ini`) |
| `US`, `CA` | 902–928 MHz | **910.525 MHz / 62.5 kHz / 7 / 5** (MeshCore "USA/Canada (Recommended)") |
| `AU`, `NZ` | 915–928 MHz | none: set all four values |
| `IN` | 865–867 MHz | none: set all four values |
| `KR` | 920–923 MHz | none: set all four values |

- **All pagers of a group need the same `radio`**, just like `channel`. A pager with a different frequency can't hear the others. When changing the radio of an existing group, update **every** pager.
- The pager checks only the band, not duty-cycle or power rules. The group is responsible for using a frequency that is legal where they are. The TX power stays as it is (not part of the setup).
- The new settings apply **right away** after COMMIT (the BLE link isn't affected) and at every boot. `CLEAR` goes back to `pager.ini`.

### Supported YAML subset
Whatever the frontend's YAML library emits in its default block style is fine (tested with PyYAML output, including sorted keys and `\xE4` / `\U0001F4DE` escapes). In detail:
- UTF-8 (a BOM is ignored), LF or CRLF, at most **4096 bytes**, lines at most 255 bytes.
- Indentation with **spaces only** (tabs are rejected). Sequences may sit at the parent key's indentation (`questions:\n- text: …`).
- `#` comments, `---` / `...` markers.
- Scalars: plain, `'single'` (`''` = `'`), `"double"` with the escapes `\" \\ \/ \xXX \uXXXX \UXXXXXXXX` (UTF-16 surrogate pairs are combined).
- Single-line flow lists (`[a, "b, c"]`) and single-line flow mappings for `channel` and `radio` (`{name: X, key: Y}`, `{region: EU}`).
- **Not supported:** anchors/aliases/tags, block scalars (`|`, `>`), multi-line scalars, other flow mappings, unknown keys (they are **errors**, so typos don't go unnoticed).

---

## 2. Bluetooth transport

The pager runs the normal **MeshCore companion protocol** over the Nordic UART service. The stock MeshCore app keeps working next to it.

| | UUID |
|---|---|
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| RX (write, app → pager) | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| TX (notify, pager → app) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

- **One write = one frame, one notification = one frame.** A frame is at most **176 bytes**. The link needs a negotiated MTU above that (Chrome/Web Bluetooth and the mobile OSes do this automatically).
- The device advertises as `MeshCore-<nickname>`.
- **Pairing:** the connection is encrypted with a 6-digit PIN. The PIN is shown on the pager's boot screen, and in pairing mode (**hold PRG for 10 s**, which also drops any other connected phone). The OS pairing dialog asks for it the first time you write.
- **Send one command, then wait for its response** before sending the next one. The pager may push unrelated frames at any time (codes `0x80` and above, e.g. "message waiting"). Ignore them.

---

## 3. Commands

All setup commands start with **`0x70`** (`CMD_PAGER_CONFIG`), followed by an operation byte. Integers are **little-endian**.

| Op | Name | Request | Response |
|---|---|---|---|
| `0x01` | BEGIN | `70 01 <total:u16>` (YAML length, 1–4096) | OK, or ERR `06` if the length is 0 or too large |
| `0x02` | DATA | `70 02 <offset:u16> <bytes…>` (up to 172 bytes, **128 recommended**) | OK, or ERR `04` if the offset isn't exactly the number of bytes received so far, or it would go past `total` |
| `0x03` | COMMIT | `70 03` | ERR `04` if not all bytes arrived, otherwise a **RESULT** frame |
| `0x04` | STATUS | `70 04` | RESULT (summary of the active setup) |
| `0x05` | CLEAR | `70 05` | RESULT. Deletes the stored setup and goes back to the build config |

Standard responses from the companion protocol:
- **OK** = `00`
- **ERR** = `01 <code>`: `01` unknown command (not a pager firmware), `04` bad state (order/offset), `06` illegal argument.

**RESULT** = `70 <status:u8> <line:u16> <message: UTF-8, rest of the frame>`

| Status | Meaning | `line` / message |
|---|---|---|
| `0` | OK. The setup is stored **and active right away** | 0 / summary, e.g. `nickname=Anna channel=Familie key=setup questions=2 radio=EU:869.618/62.5/8/5` (`key=build` means the key from `pager.secret.ini`; `radio=build:…` means the radio from `pager.ini`; the values after the colon are the active MHz/kHz/SF/CR) |
| `1` | invalid YAML/setup. **Nothing was changed** | line number (1-based, `0` = whole file) / reason in English, e.g. `unknown key 'nicknme'`, `reply longer than 40 bytes` |
| `2` | flash write failed. Nothing was changed | 0 / `flash write failed` |

After COMMIT, whether it succeeded or not, the upload buffer is used up. A new attempt starts again with BEGIN. A new BEGIN always drops an unfinished upload.

### Sequence

```
app                                   pager
 |  70 01 3a 01            (BEGIN, 314 bytes)
 |  ----------------------------->  00 (OK)
 |  70 02 00 00 <128 bytes>          (DATA @0)
 |  ----------------------------->  00
 |  70 02 80 00 <128 bytes>          (DATA @128)
 |  ----------------------------->  00
 |  70 02 00 01 <58 bytes>           (DATA @256)
 |  ----------------------------->  00
 |  70 03                            (COMMIT)
 |  ----------------------------->  70 00 00 00 "nickname=Anna channel=Familie key=setup questions=2 radio=EU:869.618/62.5/8/5"
```

### What happens on the pager
- The pager shows the popup **`Setup updated`** and wakes the display.
- **Nickname:** used immediately for sending and in the header. The Bluetooth name (`MeshCore-<nickname>`) changes after the next reboot.
- **Channel:** exactly one channel with the configured name and key exists afterwards. If the name changed, the channel with the old name is removed. Other channels (e.g. `Public`) aren't touched.
- The setup is **enforced at every boot**: changing the nickname or the pager channel in the MeshCore app only lasts until the next reboot. To change the setup, upload a new file.
- **Radio:** with a `radio` block, the pager switches to the new frequency immediately after COMMIT. Without one, it uses `pager.ini`. See "Regions and radio presets" above.

---

## 4. Reference client

```sh
pip install bleak
bin/pager_setup.py scan                                   # list pagers nearby
bin/pager_setup.py upload <address> docs/pager_setup_example.yaml
bin/pager_setup.py status <address>
bin/pager_setup.py clear  <address>
```

For a Web Bluetooth frontend, the same flow is: `requestDevice({filters: [{services: [SERVICE]}]})` → `getPrimaryService` → `startNotifications()` on TX → for each frame, `writeValueWithResponse()` on RX, then wait for the next notification whose first byte is `00`, `01` or `70`.

## 5. Firmware update over BLE (`0x71`, FRD-024)

Firmware with update support can be updated over Bluetooth. **The first install needs USB.** Command **`0x71`** (`CMD_PAGER_OTA`), same connection and pairing as above. The result frame has the same layout as the setup RESULT: `71 <status:u8> <line:u16 = 0> <message>`, status `0` = OK, `1` = failed.

| Op | Name | Request | Response |
|---|---|---|---|
| `0x00` | INFO | `71 00` | RESULT `board=<env> fw=<version> ota=<esp32\|nrf-dfu>` |
| `0x01` | BEGIN | `71 01 <size:u32> <md5:16 bytes>` (ESP32) | RESULT `ready`, or the reason (e.g. image too large) |
| `0x02` | DATA | `71 02 <offset:u32> <ack:u8> <image bytes>` (ESP32, **160 bytes recommended**) | **only if `ack` ≠ 0**: OK (`00`), or RESULT status 1 if anything failed since BEGIN |
| `0x03` | END | `71 03` (ESP32) | RESULT `OK, restarting` (the pager restarts ~1 s later into the new image), or the reason |
| `0x04` | ABORT | `71 04` | OK |
| `0x10` | DFU | `71 10` (nRF52) | RESULT, then the pager restarts ~1 s later into the **BLE DFU bootloader** |

**ESP32 (V3, V4), `ota=esp32`:** the file is the **app image** (`<env>-<version>-<sha>.bin`, **not** `-merged.bin`: it starts with `0xE9`, and the app descriptor magic `0xABCD5432` is at offset 32).
- BEGIN with the size and the MD5 of the whole file, then DATA frames in order.
- Set `ack` on **every 4th frame** and on the last one, and wait for the answer before sending more. The pager's BLE receive queue holds 4 frames, and it answers at most every 60 ms, so acknowledging every frame would be ~4× slower.
- END checks the size, the MD5 and the **board marker** (below). Only then is the new image marked bootable. On any failure the running firmware stays as it is.
- The image goes into the second app slot. Settings, setup and Bluetooth pairings are kept.

**nRF52 (T114), `ota=nrf-dfu`:** the file is the **DFU package** (`<env>-<version>-<sha>.zip` with `manifest.json`, `firmware.bin`, `firmware.dat`).
- Send DFU. The pager restarts into its bootloader, which advertises the Nordic legacy DFU service `00001530-1212-efde-1523-785feabcd123` (as `AdaDFU` or similar). Connect to it (a new device for the OS).
- Nordic **legacy DFU**, as in `bin/pager_ota.py`: control point `…1531` (write + notify), packet `…1532` (write without response).
  1. `01 04` (start, application), then on the packet characteristic 12 bytes: sizes softdevice `0`, bootloader `0`, app (u32 LE each). Wait for `10 01 01`.
  2. `02 00`, the `firmware.dat` bytes as a packet, `02 01`. Wait for `10 02 01`.
  3. `08 0a 00` (receipt notification every 10 packets), then `03`. Send `firmware.bin` in 20-byte packets, and after every 10th wait for `11 <bytes received:u32>`. At the end, wait for `10 03 01`.
  4. `04` (validate), wait for `10 04 01`. Then `05`: the bootloader activates the image and restarts.

**Board marker:** every pager image contains the string `MSPAGER-BOARD:<env>` followed by a NUL, e.g. `MSPAGER-BOARD:heltec_v3_pager`. INFO reports the same `<env>` as `board=`. Clients SHALL refuse a file whose marker doesn't match, so an image for another board can't make a pager unusable. The ESP32 update also checks it on the pager.

## 6. Group password → channel key (FRD-025)

A client can derive the 128-bit channel key from a **group password** instead of handling hex keys. Everyone who enters the same password and channel name gets the same key; the pager only ever sees the resulting hex key in the setup YAML. The derivation is fixed. **Changing any detail would split groups**, so a future variant needs a new salt prefix (`…/v2/…`).

```
password'  = NFC(password), lowercased, split at whitespace and - _ . , ; : + /, empty parts dropped, joined with "-"
salt       = UTF-8("MSPager/v1/channel-key/" + channel name)     (channel name exactly as in the YAML)
key        = PBKDF2-HMAC-SHA256(UTF-8(password'), salt, 600000 iterations, 16 bytes)
channel.key = lowercase hex(key)
```

Test vector: password `Tulip  Harbor-orbit_PASTE`, channel `Familie` → `bf1722a8bf0556be4ee86abe8b953878`.

- Anyone in radio range can record the traffic and try passwords offline, so the password is the whole security. **Use 4 random words** from a large list (the web app uses the EFF large wordlist, 7776 words: 4 words ≈ 52 bits). The slow KDF makes each guess cost 600,000 hash rounds. Don't use a sentence or a name.
- Clients shouldn't store the password.

## 7. Security
The YAML contains the **group key** (and a group password, if used, gives it too). Anyone with the file can read the group's messages. The frontend shouldn't store it longer than needed, and shouldn't send it anywhere other than to the pager. The BLE link itself is encrypted (PIN pairing).
