# MSPager Setup Frontend — Handoff for Implementation

> **Audience:** a developer or AI agent building the setup frontend from scratch.
> **Status (2026-09-26):** the firmware side is implemented and unit-tested (see §9). It hasn't been tested on a device yet. **No frontend exists yet.**
> This file is self-contained. The authoritative sources are the firmware code and [pager_config_protocol.md](pager_config_protocol.md). If they disagree, the firmware wins.

---

## 1. Context in 60 seconds

- **MSPager** is firmware for the **Heltec WiFi LoRa 32 V4** (ESP32-S3 + LoRa + 128×64 OLED + GPS). It turns the board into a one-button group pager on a **MeshCore** LoRa mesh. It's a fork of MeshCore's `companion_radio` firmware.
- A group of 2–10 people each carries a pager. They send **canned messages** ("Angekommen?", "Brauche Hilfe", …) on a shared **encrypted group channel** and answer with canned **replies** ("Ja", "Nein", "OK", 📞). Every message automatically carries the sender's GPS position.
- The pagers are **already flashed**. The frontend's job is to **set up** each pager over **Bluetooth LE** by uploading a **YAML file**. The pager parses, validates and stores it itself.
- The pager still speaks the normal **MeshCore companion protocol** over BLE. The stock MeshCore phone app keeps working. The setup upload is one extra command (`0x70`) in that protocol.

## 2. What the frontend must do

1. Let an admin define a **group**: channel name, channel key (generate a random one), and a list of **extra questions, each with its own replies**.
2. Let the admin add **members** (one nickname per pager).
3. For each member, **generate a YAML file** (group part + that member's nickname).
4. **Connect to a pager over BLE**, upload that member's YAML, and show the result: a success summary, or an error with line number and reason.
5. Optional but useful: read the current setup summary from a pager (STATUS), and reset a pager to its build defaults (CLEAR).

Suggested platform: a **web app using Web Bluetooth** (Chrome/Edge on desktop or Android). This fits the existing MeshCore tooling (the official MeshCore web app uses Web Bluetooth too). iOS Safari has no Web Bluetooth. If iOS matters, a native or Capacitor app with a BLE plugin needs the same protocol. Tech stack and design are up to you.

**Out of scope:** flashing firmware, radio settings (frequency etc., fixed at build time), sending messages, maps.

---

## 3. Domain rules the UI must respect

| Rule | Why |
|---|---|
| **All pagers of a group must get identical `channel` and `questions`.** Only `nickname` differs. | A receiver looks up an incoming question text in **its own** setup to decide which replies to offer. The channel key must match, or pagers can't read each other. |
| Uploading **replaces** the pager's whole previous setup (no merge). | Always send the full group definition plus the nickname. |
| Every key is optional. A missing key means "use the value compiled into the firmware" (channel `Pager`, the build's key, built-in questions only). | Lets you do partial setups, but the normal flow sends everything. |
| The setup is **enforced at every boot**. Changes made in the MeshCore app to the nickname or pager channel revert on reboot. | To change something, upload a new YAML. |
| Built-in questions always exist: `Angekommen?`, `Brauche Hilfe`, `Standort?`, `Mein Standort`. Default replies: `Ja`, `Nein`, `OK`, `📞`, `Standort?`, `Mein Standort`. | Extra questions appear **after** the built-ins in the pager's send menu. A question in the YAML whose `text` equals a built-in does **not** add a menu entry. It only **replaces that question's replies**. Show this in the UI (e.g. "override replies of built-in question"). |
| `Mein Standort` is special: as a message or reply, receivers show distance + direction. `📞` is drawn as a phone icon on the OLED. | Offer them as quick-insert chips in the reply editor. |
| OLED character set: ASCII, `ä ö ü Ä Ö Ü ß é °` and 📞 render correctly. **Any other non-ASCII character shows as a filled block** on the pager (the phone app shows it fine). | Warn (don't block) when a text has other non-ASCII characters. |
| The pager display is 21 characters wide (small font), 11 characters in large font. Longer texts scroll or are truncated in menus. | Suggest keeping texts at ≤ 20 characters. Show a soft warning. |
| **Security:** the channel key lets anyone read the group's messages. | Don't send it anywhere except to the pager. Don't log it. Store it only if the user explicitly saves the group (e.g. encrypted or local file export). |

---

## 4. YAML schema (version 1) and validation

### 4.1 Shape

```yaml
version: 1
nickname: Anna                       # per pager
channel:                             # identical for the whole group
  name: Familie
  key: 8b3387e9c5cdea6ac9e5edbaa115cd72
questions:                           # identical for the whole group, may be omitted or []
  - text: Wann kommst du?
    replies: ["5 min", "30 min", "Später", "📞"]
  - text: Essen ist fertig
    replies: [Komme, Später, Ohne mich]
  - text: Angekommen?                # built-in text: only replaces its replies
    replies: [Ja, Noch nicht, Mein Standort]
```

The same as a JSON Schema (validate the model with it before generating YAML):

```json
{
  "type": "object",
  "additionalProperties": false,
  "properties": {
    "version":  { "const": 1 },
    "nickname": { "type": "string", "minLength": 1, "pattern": "^[^:\\[\\]]+$", "x-maxBytesUtf8": 31 },
    "channel": {
      "type": "object", "additionalProperties": false,
      "properties": {
        "name": { "type": "string", "minLength": 1, "not": { "const": "Public" }, "x-maxBytesUtf8": 31 },
        "key":  { "type": "string", "pattern": "^[0-9a-fA-F]{32}$" }
      }
    },
    "questions": {
      "type": "array", "maxItems": 12,
      "items": {
        "type": "object", "additionalProperties": false, "required": ["text", "replies"],
        "properties": {
          "text":    { "type": "string", "minLength": 1, "x-maxBytesUtf8": 40 },
          "replies": { "type": "array", "minItems": 1, "maxItems": 6,
                       "items": { "type": "string", "minLength": 1, "x-maxBytesUtf8": 40 } }
        }
      }
    }
  }
}
```

### 4.2 Validation rules (mirror them in the UI; the pager enforces them too)

| Field | Rule | Pager error message (prefix match) |
|---|---|---|
| any key | unknown keys are **errors**; each key at most once | `unknown key '…'`, `duplicate key '…'` |
| `version` | must be `1` | `unsupported version` |
| `nickname` | 1–31 **UTF-8 bytes**, must not contain `:` `[` `]` | `nickname longer than 31 bytes`, `nickname must not contain…` |
| `channel.name` | 1–31 bytes, not `Public` | `channel name 'Public' is reserved` |
| `channel.key` | exactly 32 hex chars (16 bytes) | `channel key must be 32 hex characters` |
| `questions` | ≤ 12 items; `text` unique within the list | `more than 12 questions`, `duplicate question '…'` |
| `questions[].text` | 1–40 bytes, required | `question text longer than 40 bytes`, `question without 'text'` |
| `questions[].replies` | 1–6 items, each 1–40 bytes | `more than 6 replies`, `question '…' has no replies`, `reply longer than 40 bytes` |
| all texts | valid UTF-8, **no control characters** (no newlines, tabs) | `… contains a control character` |
| whole file | ≤ **4096 bytes**, lines ≤ 255 bytes | BEGIN is rejected with ERR `06` if > 4096 |

**Limits are in bytes, not characters:** `ä` = 2 bytes, `📞` = 4 bytes. Use `new TextEncoder().encode(s).length`.

**Key generation:** `crypto.getRandomValues(new Uint8Array(16))` → lowercase hex (32 chars).

### 4.3 Generating the YAML

The pager parses a **YAML subset**. The output of a standard library in **block style** is fine:
- **js-yaml:** `yaml.dump(obj, { lineWidth: -1, noRefs: true })`. `lineWidth: -1` avoids folded multi-line strings. `noRefs` avoids anchors.
- **eemeli/yaml:** `YAML.stringify(obj, { lineWidth: 0, aliasDuplicateObjects: false })`.
- Or write it by hand: keys **unquoted** (`nickname:`), every string value as a double-quoted JSON string (`JSON.stringify(s)`). The pager accepts that too (it supports `\" \\ \/ \uXXXX` incl. surrogate pairs).

Supported by the pager: spaces-only indentation, `#` comments, plain / `'single'` / `"double"` scalars, `\xXX \uXXXX \UXXXXXXXX` escapes, single-line flow lists `[a, "b"]`, a single-line flow mapping **only** for `channel: {name: …, key: …}`, CRLF, a UTF-8 BOM.
**Not supported:** anchors/aliases/tags, block scalars `|` `>`, multi-line (folded) scalars, other flow mappings (e.g. `- {text: …, replies: […]}`), tabs, multiple documents.

Add a comment header for humans, e.g. `# MSPager setup for Anna — group Familie — generated <date>`.

---

## 5. BLE transport

| | UUID (Nordic UART service) |
|---|---|
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| RX: write, frontend → pager | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| TX: notify, pager → frontend | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

- **One GATT write = one request frame. One notification = one response frame.** Max frame size is **176 bytes**. There's no extra framing, length prefix or checksum.
- Device name: `MeshCore-<nickname>` (the nickname before the new setup, since the BLE name only changes after a reboot). Filter by service UUID and/or `namePrefix: "MeshCore-"`.
- **Pairing/encryption:** the pager uses a 6-digit static PIN. The first access to the characteristics triggers the **OS pairing dialog**, and the user types the PIN shown on the pager:
  - on the boot screen (first 5 s after power-on/RST), or
  - **hold the PRG button for 10 s**, which starts pairing mode for 30 s, shows the PIN in large digits and drops any other connected phone. Put this instruction in the UI. It's also how to fix "pager not found" when a phone is already connected to it (ESP32 BLE stops advertising while connected).
- **Strictly request/response:** send one frame, wait for its response, then send the next. Use a timeout of about 10 s.
- **Unsolicited frames can arrive at any time** (first byte `0x80` or higher, e.g. `0x83` "message waiting" when a LoRa message comes in). **Ignore any frame whose first byte isn't `0x00`, `0x01` or `0x70`** while you wait for a response.
- MTU: Chrome negotiates a large MTU automatically. Keep data chunks at **128 bytes** (132-byte frame) to leave headroom.
- You **don't** need to send the MeshCore `CMD_APP_START` handshake before the setup commands.

---

## 6. Setup commands (all start with `0x70`, integers little-endian)

| Op | Request bytes | Success response | Failure responses |
|---|---|---|---|
| BEGIN | `70 01 <len:u16>` (YAML byte length, 1–4096) | `00` | `01 06` (0 or > 4096) |
| DATA | `70 02 <offset:u16> <chunk…>` (chunk 1–172 bytes, use 128) | `00` | `01 04` (no BEGIN, offset ≠ bytes received so far, or past `len`) |
| COMMIT | `70 03` | RESULT (status 0) | `01 04` (upload incomplete), RESULT status 1/2 |
| STATUS | `70 04` | RESULT (status 0, summary) | — |
| CLEAR | `70 05` | RESULT (status 0, summary) | — |

Standard response frames:
- `00` = OK
- `01 <code>` = ERR. Codes: `01` unsupported command (the device isn't running MSPager firmware), `04` bad state, `06` illegal argument.

**RESULT frame:** `70 <status:u8> <line:u16 LE> <message: UTF-8 until end of frame, no NUL>`

| status | meaning | line | message |
|---|---|---|---|
| 0 | stored **and active immediately** | 0 | summary: `nickname=<n> channel=<c> key=<setup\|build> questions=<k>` |
| 1 | invalid YAML/setup. **The pager is unchanged** | 1-based line in the uploaded YAML (0 = whole file) | English reason, see §4.2 |
| 2 | flash write failed. Pager unchanged | 0 | `flash write failed` |

- `key=setup` means the key came from the uploaded YAML. `key=build` means the compiled-in fallback key.
- After COMMIT (success or failure), the upload is used up. Retry from BEGIN. A new BEGIN always discards an unfinished upload, so it's safe to restart after a disconnect.
- On success, the pager wakes and shows the popup `Setup updated`, so the admin can confirm the right device.

### Worked byte example (314-byte YAML)

```
→ 70 01 3a 01                  BEGIN len=314        ← 00
→ 70 02 00 00 <128 bytes>      DATA  offset=0       ← 00
→ 70 02 80 00 <128 bytes>      DATA  offset=128     ← 00
→ 70 02 00 01 <58 bytes>       DATA  offset=256     ← 00
→ 70 03                        COMMIT               ← 70 00 00 00 "nickname=Anna channel=Familie key=setup questions=3"
```

Error example: `← 70 01 04 00 "unknown key 'nicknme'"` → status 1, line 4.

---

## 7. Reference implementation (TypeScript, Web Bluetooth)

A working Python version of the same flow is in [`bin/pager_setup.py`](../bin/pager_setup.py) (bleak). The TypeScript sketch below implements the same protocol:

```ts
const SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // write
const TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // notify
const CMD = 0x70, OP = { BEGIN: 1, DATA: 2, COMMIT: 3, STATUS: 4, CLEAR: 5 };
const CHUNK = 128, YAML_MAX = 4096;

export type SetupResult = { status: number; line: number; message: string };

export class PagerLink {
  private rx!: BluetoothRemoteGATTCharacteristic;
  private waiters: ((f: Uint8Array) => void)[] = [];

  static async connect(): Promise<PagerLink> {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE] }, { namePrefix: "MeshCore-" }],
      optionalServices: [SERVICE],
    });
    const server = await device.gatt!.connect();
    const svc = await server.getPrimaryService(SERVICE);
    const link = new PagerLink();
    link.rx = await svc.getCharacteristic(RX);
    const tx = await svc.getCharacteristic(TX);
    tx.addEventListener("characteristicvaluechanged", (e: any) => {
      const v = e.target.value as DataView;
      const f = new Uint8Array(v.buffer, v.byteOffset, v.byteLength).slice();
      if (f.length && (f[0] === 0x00 || f[0] === 0x01 || f[0] === 0x70)) link.waiters.shift()?.(f);
      // else: unsolicited push (0x80+), ignore
    });
    await tx.startNotifications(); // may trigger OS pairing (PIN from the pager)
    return link;
  }

  private request(frame: Uint8Array, timeoutMs = 10000): Promise<Uint8Array> {
    return new Promise(async (resolve, reject) => {
      const t = setTimeout(() => reject(new Error("timeout")), timeoutMs);
      this.waiters.push((f) => { clearTimeout(t); resolve(f); });
      try { await this.rx.writeValueWithResponse(frame); } catch (e) { clearTimeout(t); reject(e); }
    });
  }

  private async expectOk(frame: Uint8Array) {
    const r = await this.request(frame);
    if (r[0] === 0x01) throw new Error(`pager error code ${r[1]}`); // 1=not MSPager, 4=bad state, 6=bad arg
    if (r[0] !== 0x00) throw new Error(`unexpected response 0x${r[0].toString(16)}`);
  }

  private async result(op: number): Promise<SetupResult> {
    const r = await this.request(new Uint8Array([CMD, op]));
    if (r[0] === 0x01) throw new Error(`pager error code ${r[1]}`);
    return {
      status: r[1],
      line: r[2] | (r[3] << 8),
      message: new TextDecoder().decode(r.slice(4)),
    };
  }

  async upload(yamlText: string): Promise<SetupResult> {
    const data = new TextEncoder().encode(yamlText);
    if (data.length === 0 || data.length > YAML_MAX) throw new Error("YAML must be 1..4096 bytes");
    await this.expectOk(new Uint8Array([CMD, OP.BEGIN, data.length & 0xff, data.length >> 8]));
    for (let off = 0; off < data.length; off += CHUNK) {
      const chunk = data.slice(off, off + CHUNK);
      const f = new Uint8Array(4 + chunk.length);
      f.set([CMD, OP.DATA, off & 0xff, off >> 8]);
      f.set(chunk, 4);
      await this.expectOk(f);
    }
    return this.result(OP.COMMIT);
  }

  status() { return this.result(OP.STATUS); }
  clear()  { return this.result(OP.CLEAR); }
}
```

When status is 1, map `line` back to the generated YAML and show the offending line and field. Since the frontend generated the file, an error here means the frontend's validation is out of sync with the firmware. Log it prominently.

---

## 8. Suggested UX flow

1. **Group screen:** channel name (default `Pager`), key (auto-generated, with "regenerate" and "copy/QR" for backup), question editor (text, 1–6 replies with chips for `Ja`/`Nein`/`OK`/`📞`/`Mein Standort`). Show live byte counters and warnings (> 20 chars, unsupported characters).
2. **Members screen:** list of nicknames. Show which ones are already set up (from the last success summary).
3. **Set up a pager:** "Hold PRG 10 s on the pager, then click Connect" → pick the device → PIN dialog (OS) → upload → success shows the summary and "The pager shows 'Setup updated'". On error, show the reason and line.
4. **Tools:** Status (read the summary), Reset to defaults (CLEAR, with confirmation), Download YAML (for backup and for `bin/pager_setup.py`).
5. Remind the admin that **all members must be (re)uploaded after any change to channel or questions**. Consider tracking a hash of the group definition per member, and flag outdated pagers.

## 9. Firmware facts (where to look if something is unclear)

| Topic | File |
|---|---|
| Protocol spec (short) | [`docs/pager_config_protocol.md`](pager_config_protocol.md) |
| Requirement + acceptance criteria | [`frd/FRD-021-ble-setup-yaml.md`](../frd/FRD-021-ble-setup-yaml.md) |
| Example YAML | [`docs/pager_setup_example.yaml`](pager_setup_example.yaml) |
| YAML parser, limits (`PAGER_CFG_*`), error messages | `examples/companion_radio/ui-pager/PagerConfig.{h,cpp}` |
| Parser unit tests (good source of valid/invalid examples) | `test/test_pager_config/test_pager_config.cpp` (run `pio test -e native -f test_pager_config`) |
| BLE command handling (`CMD_PAGER_CONFIG`, `handlePagerConfigCmd`) | `examples/companion_radio/MyMesh.cpp` |
| How questions/replies appear on the pager | `UITask::buildComposeOptions()` / `buildReplyOptions()` in `examples/companion_radio/ui-pager/UITask.cpp` |
| Reference client | `bin/pager_setup.py` |
| Manual device test checklist | [`docs/pager_testing.md`](pager_testing.md), section M4b |

## 10. Acceptance checklist for the frontend

- [ ] Generates YAML that the pager accepts for: nickname only; full group; 12 questions × 6 replies × 40-byte texts; umlauts and 📞; texts containing `:`, `#`, quotes, `'`, `[`.
- [ ] Rejects in the UI, before upload, everything §4.2 lists (byte-based limits).
- [ ] Uploads over BLE with chunking and strict request/response. Ignores push frames. Handles timeouts, disconnects mid-upload (restart from BEGIN) and ERR `01` ("not an MSPager firmware").
- [ ] Shows the success summary and pager errors (line + reason).
- [ ] Two pagers set up with the same group can exchange a custom question, and the receiver offers exactly the configured replies.
- [ ] The key never leaves the device except to the pager. It isn't logged.

## 11. Open points / possible firmware extensions

- The pager can't **return the stored setup as YAML** yet (STATUS gives only a summary). If the frontend needs "read back and edit", ask for a firmware op (e.g. `0x06` GET, chunked).
- **Radio settings** aren't part of the setup (they're fixed in `pager.ini` at build time).
- Firmware limits (12 questions, 6 replies, 40 bytes) are compile-time constants in `PagerConfig.h`. Changing them needs a firmware rebuild and has to respect the 160-byte message budget.
