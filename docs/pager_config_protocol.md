# MSPager — Setup over BLE (YAML upload protocol)

This is for whoever builds the **setup frontend**. It turns the setup of a pager (nickname, group channel, extra questions with their replies) into a YAML file and uploads it to an already flashed pager over Bluetooth. The pager parses the YAML itself, rejects invalid files with a line number and a reason, and stores the result as a compact binary blob in flash.

> Requirement: [FRD-021](../frd/FRD-021-ble-setup-yaml.md) · Example: [pager_setup_example.yaml](pager_setup_example.yaml) · Reference client: [`bin/pager_setup.py`](../bin/pager_setup.py)

---

## 1. YAML format (version 1)

```yaml
version: 1
nickname: Anna                              # per pager
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

- **Everything is optional.** A value in the YAML overrides the build config, a missing one keeps it. An upload **replaces** the whole previous setup (no merging). A file without any values (e.g. only comments) goes back to the build config, just like `CLEAR`.
- **Built-in catalogue** (always present): questions `Angekommen?`, `Brauche Hilfe`, `Standort?`, `Mein Standort`. Default replies: `Ja`, `Nein`, `OK`, `📞`, `Standort?`, `Mein Standort`. A setup question with one of the built-in texts adds nothing to the send menu but **replaces that question's replies**.
- **How replies work:** when the user replies to a message, the pager compares the message text (without the `@[Nick] ` mention) with the question texts in **its own** setup. If one matches, it offers that question's replies; otherwise it offers the default replies. So **all pagers of a group need the same `channel` and `questions`**. Only `nickname` differs. The frontend should generate one file per pager from a shared group definition.
- `Mein Standort` works as a reply too: receivers show distance and direction. `📞` and 24 more emoji are drawn as pictures ([list](pager_emoji.md), kids example: [pager_setup_kids.yaml](pager_setup_kids.yaml)). `ä ö ü Ä Ö Ü ß é °` show correctly on the OLED. Other non-ASCII characters show as a block (in the app they show normally).
- **Length budget:** each text is at most 40 bytes, so a reply with a mention and a position suffix stays within the 160-byte MeshCore text limit (FRD-012). Note that `ä` is 2 bytes and `📞` is 4.

### Supported YAML subset
Whatever the frontend's YAML library emits in its default block style is fine (tested with PyYAML output, including sorted keys and `\xE4` / `\U0001F4DE` escapes). In detail:
- UTF-8 (a BOM is ignored), LF or CRLF, at most **4096 bytes**, lines at most 255 bytes.
- Indentation with **spaces only** (tabs are rejected). Sequences may sit at the parent key's indentation (`questions:\n- text: …`).
- `#` comments, `---` / `...` markers.
- Scalars: plain, `'single'` (`''` = `'`), `"double"` with the escapes `\" \\ \/ \xXX \uXXXX \UXXXXXXXX` (UTF-16 surrogate pairs are combined).
- Single-line flow lists (`[a, "b, c"]`) and a single-line flow mapping for `channel` (`{name: X, key: Y}`).
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
| `0` | OK. The setup is stored **and active right away** | 0 / summary, e.g. `nickname=Anna channel=Familie key=setup questions=2` (`key=build` means the key from `pager.secret.ini`) |
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
 |  ----------------------------->  70 00 00 00 "nickname=Anna channel=Familie key=setup questions=2"
```

### What happens on the pager
- The pager shows the popup **`Setup updated`** and wakes the display.
- **Nickname:** used immediately for sending and in the header. The Bluetooth name (`MeshCore-<nickname>`) changes after the next reboot.
- **Channel:** exactly one channel with the configured name and key exists afterwards. If the name changed, the channel with the old name is removed. Other channels (e.g. `Public`) aren't touched.
- The setup is **enforced at every boot**: changing the nickname or the pager channel in the MeshCore app only lasts until the next reboot. To change the setup, upload a new file.
- Radio settings (frequency etc.) still come from `pager.ini` and aren't part of the setup.

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

## 5. Security
The YAML contains the **group key**. Anyone with the file can read the group's messages. The frontend shouldn't store it longer than needed, and shouldn't send it anywhere other than to the pager. The BLE link itself is encrypted (PIN pairing).
