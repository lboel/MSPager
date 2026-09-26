# MSPager — Product Requirements Document

| | |
|---|---|
| Status | Draft v0.1 |
| Date | 2026-09-25 |
| Base | Fork of [MeshCore](https://github.com/meshcore-dev/MeshCore), `examples/companion_radio` |
| Related | [FRD.md](FRD.md) (functional requirements index) · [ROADMAP.md](ROADMAP.md) · [DEVLOG.md](DEVLOG.md) · [Flashing guide](docs/pager_flashing.md) |

## 1. Vision

A small group of people each carry an identical, pocket-sized LoRa pager. Anyone can send one of a handful of **pre-defined messages** to the group ("Angekommen?", "Brauche Hilfe") with **one button**. Everyone else sees **who** sent it and **where** they were, and can answer just as quickly with a canned reply ("Ja", "Nein", "OK", 📞). The pager needs no phone, no cellular network and no typing.

## 2. Problem

Cellular coverage is unreliable in the places the group moves (outdoors, events, rural areas). Existing MeshCore companion devices need a smartphone to send messages. Standalone MeshCore UIs are built for general-purpose chat and are too complex for a quick, glanceable "are you OK?" exchange.

## 3. Target users

- A closed group (family, friends, a small team) of roughly 2–10 people.
- They are not technical. Someone technical flashes and configures the devices once.
- They use the pager in situations where they have little attention to spare.

## 4. Scope

### In scope
- **One hardware target**: Heltec WiFi LoRa 32 **V4** with the standard 0.96" 128×64 SSD1306 OLED and the GPS module sold for the V4.
- **One private, encrypted MeshCore group channel** (selected by name, default `Pager`). All pagers in the group use the same channel, the same firmware and the same radio settings.
- **Canned messages only**. Two can start a conversation, four are replies (see §7).
- **Automatic GPS position** attached to every sent message, at about 10 m precision.
- **Single-button operation** using the PRG button.
- One-time configuration (nickname, channel, radio preset) with the stock **MeshCore smartphone app over BLE**, or with a **setup YAML** uploaded over BLE by a setup frontend (nickname, channel name/key, extra questions with their own replies, [FRD-021](frd/FRD-021-ble-setup-yaml.md)).

### Out of scope (non-goals)
- Free-text input on the device.
- Direct (1:1) messages, contacts list, rooms, repeater administration.
- The Public channel or any channel other than the pager channel. They are hidden, not deleted.
- Other boards (Heltec V3, T-Beam, TFT variant of V4, …).
- Delivery confirmation. MeshCore group messages have no ACKs, and the canned reply flow covers this socially ("Angekommen?" → "Ja").
- Maps, tracking history, telemetry.

## 5. Hardware

| Function | Hardware | Pins (from `variants/heltec_v4/platformio.ini`) |
|---|---|---|
| MCU / radio | ESP32-S3 + SX1262 (+ GC1109/KCT8103L FEM) | as in env `heltec_v4_oled` |
| Display | SSD1306 128×64 I²C | SDA 17, SCL 18, RST 21, Vext EN 36 |
| GPS | Heltec V4 GPS module (UART, NMEA) | RX 38, TX 39, EN 34 (active LOW), RESET 42 (active LOW) |
| Button | **PRG** | GPIO 0 |
| LED | white onboard LED | GPIO 35 (also used as LoRa TX LED) |
| Battery | 1S LiPo via JST | ADC ctrl 37, VBAT read 1 |
| Speaker (optional) | small 8–32 Ω speaker, two wires, no resistor ([FRD-020](frd/FRD-020-message-beep.md)) | GPIO 4 + GND |

### 5.1 Button constraint (RST cannot be reassigned)
The V4 has two buttons. **Only PRG (GPIO 0) can be read by firmware.** The other button, **RST**, is wired straight to the ESP32-S3 **CHIP_PU/EN** pin. Pressing it resets the chip in hardware before any code runs, so software can't give it another function. The only alternatives are hardware changes (cutting the trace and rewiring, or adding an external button on a free GPIO). We rejected both so that every pager stays identical and unmodified.

So the whole UI runs on **one button** with three gestures: **short press**, **long press** and **double press** ([FRD-002](frd/FRD-002-single-button-input.md)). RST stays a hardware reset, and PRG+RST enters the bootloader for flashing.

## 6. User stories

| # | As a … | I want to … | so that … |
|---|---|---|---|
| US-1 | pager user | wake the pager with one press and see battery + latest messages at a glance | I know immediately whether anything happened |
| US-2 | pager user | send "Angekommen?" or "Brauche Hilfe" with a few presses | the group knows I'm asking / need help |
| US-3 | pager user | see the sender's **nickname** on every message | I know who is asking |
| US-4 | pager user | see how far away and in which direction the sender is (and their coordinates) | I can find them or judge the situation |
| US-5 | pager user | reply "Ja" / "Nein" / "OK" / 📞 to a specific message | the asker gets a quick answer and knows it was meant for them |
| US-6 | pager user | notice a new message (display wakes, LED blinks) | I don't miss a call for help |
| US-7 | device admin | flash one firmware and set nickname/channel via the MeshCore app | setting up a new pager takes minutes |
| US-9 | pager user | ask "where are you?" and get the others' positions automatically | I can find the group without anyone pressing a button |
| US-8 | device admin | have stock MeshCore app users in the same channel read pager messages | a phone user can join the group |
| US-11 | child | read and answer messages that use pictures (🏠 👍 🚗 🆘) instead of words | I can use the pager before I read well |
| US-10 | device admin | set up flashed pagers from a frontend with one YAML file each (nickname, group channel, our own questions and replies) | a group gets its own messages without rebuilding the firmware |

## 7. Canned message catalogue

| Type | Text on the wire | Shown on OLED |
|---|---|---|
| Start | `Angekommen?` | Angekommen? |
| Start | `Brauche Hilfe` | Brauche Hilfe |
| Reply | `Ja` | Ja |
| Reply | `Nein` | Nein |
| Reply | `OK` | OK |
| Reply | `📞` (UTF-8 U+1F4DE) | custom 8×8 handset glyph ([FRD-013](frd/FRD-013-phone-emoji-rendering.md)) |
| Start + reply | `Standort?` | location request: other pagers **answer automatically** with `Mein Standort` ([FRD-019](frd/FRD-019-location-messages.md)) |
| Start + reply | `Mein Standort` | share location: receivers see distance + direction, e.g. `Mein Standort 1.2km NE` |

**Extra questions** can be added per group with the setup YAML ([FRD-021](frd/FRD-021-ble-setup-yaml.md)), each with its own 1–6 replies. They appear after the built-in starters. The built-in texts can get their own replies too.

Replies carry a mention of the original sender, e.g. `@[Anna] Ja` ([FRD-007](frd/FRD-007-replies-with-mention.md)). Every message ends with a position suffix, e.g. `[52.5201,13.4050]` ([FRD-010](frd/FRD-010-gps-attachment.md)).

## 8. Key product decisions

| Topic | Decision | Rationale |
|---|---|---|
| Firmware base | Fork of the MeshCore **companion radio** firmware with a new pager UI | Reuses channel crypto, BLE app config and GPS support |
| Configuration | Nickname via the stock MeshCore app over **BLE, always on**. Channel and radio come from `pager.ini`. A **setup YAML** over BLE (FRD-021) overrides nickname and channel and adds questions | Only the nickname differs per pager. The setup lets a frontend configure flashed pagers without a rebuild |
| Pager channel & radio | Name and radio settings (EU/UK Narrow) in git-tracked **`pager.ini`**, key in gitignored **`pager.secret.ini`**, enforced at every boot | All pagers meet in the same channel right after flashing. The key stays out of the public repo |
| Message format | **Plain text** incl. position suffix | Readable in stock MeshCore apps (US-8) |
| Position precision | **4 decimals (≈10 m max)** | Enough to find someone, limits exposure of exact location |
| No GPS fix | Send last known position + age, or `[no GPS]` | **Never block sending**, especially "Brauche Hilfe" |
| Display | **Off when idle**. The first press only wakes it | Battery, and no accidental sends |
| Alerting | Display wakes + LED blinks until read, optional speaker beep ([FRD-020](frd/FRD-020-message-beep.md)) | No buzzer/vibration on the stock board |
| Language | UI and docs in English, canned messages in German | Group language vs. maintainability |

## 9. Non-functional requirements

- **Interoperability**: pager messages are valid MeshCore `GRP_TXT` messages. Stock clients in the same channel can read them and answer with free text, and pagers display such free text as-is.
- **Radio**: all pagers use the same preset (default: EU/UK Narrow, 869.618 MHz). Pagers can use existing MeshCore repeaters (flood routing).
- **Message size**: the longest pager message stays well below `MAX_TEXT_LEN` (160 bytes) ([FRD-012](frd/FRD-012-wire-format.md)).
- **Battery**: BLE is always on (a known trade-off for easy configuration). The display is off when idle. Target: at least 24 h standby on a 1000 mAh cell (to be measured in M5).
- **Responsiveness**: a button press reacts in under 100 ms. A received message appears in under 1 s after decoding.
- **Robustness**: works without a GPS fix. Survives a reboot without re-configuration (prefs and channels live in flash). The inbox doesn't have to survive a reboot.

## 10. Success criteria

1. Three pagers in the field exchange all six canned messages, with correct nicknames, in the M5 field test.
2. The receiver's distance/bearing is within ±20 m / ±1 compass sector of reality (with a GPS fix on both ends).
3. A new person can send "Brauche Hilfe" within 10 s of picking up the pager, without instructions beyond a one-page card.
4. A stock MeshCore app user in the `Pager` channel can read pager messages, including position.

## 11. Risks & open points

| Risk / open point | Mitigation |
|---|---|
| The MeshCore app over BLE can still send free text into the channel | Accepted. The pager shows it as-is |
| GPS cold start can take minutes indoors | Last-fix + age fallback. The flashing guide recommends a first fix outdoors |
| GPIO 35 LED is shared with the LoRa TX indicator | Resolved in M4: the pager env removes the TX flash, and the LED is only used for new-message alerts (FRD-014) |
| Nicknames longer than ~10 characters crowd the 21-column OLED | The guide recommends ≤10 characters. The UI ellipsizes |
| No delivery confirmation for group messages | Social protocol via canned replies. A "heard repeat" indicator is a possible roadmap item |
| BLE always on reduces battery life | Measure in M5. Revisit (e.g. auto-off BLE) if the target is missed |
