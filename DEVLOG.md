# MSPager — Development Log

Newest entries at the top. Record decisions, their reasons, and anything surprising.

---

## 2026-09-30 — Heltec V3 and T114 builds

**Request:** support the Heltec T114 (different display size), and build firmware for T114 and V3 too. The buzzer can be left out for now.

**Done**
- New envs `heltec_v3_pager` (ESP32-S3, same OLED as the V4) and `heltec_t114_pager` (nRF52840, 240×135 ST7789 TFT). The shared pager flags/sources are now in `[pager_common]` (platformio.ini), and `heltec_v4_pager` uses it too.
- `sh build.sh build-pager-firmwares` builds every `*_pager` env: `.bin` + `-merged.bin` for V3/V4, `.uf2` + `.zip` for T114.
- **T114 display:** the upstream `ST7789Display` draws with proportional Arial fonts and scales each primitive, so the pager's 6×8 grid, the CP437 arrows/umlauts, size-3 PIN and emoji would look wrong. The new `ST7789CanvasDisplay` draws into a 128×64 `GFXcanvas1` exactly like `SSD1306Display` does, then scales it on `endFrame()` (x: 240/128 nearest neighbour, y: 2×, centred). The UI code is unchanged.
- Portability: the buzzer drive-strength calls are ESP32-only (`BUZZER_SET_DRIVE()`); the LED polarity is `PAGER_LED_ON` (T114: `LOW`); the setup file is written through the new `DataStore::openForWrite()` instead of the ESP32-only `open(…, "w", true)`.
- Sizes: T114 flash 65 % (462 KB of 712 KB), RAM 70 %. V3 flash 39 %, RAM 58 %.
- Tests: all 60 native tests pass. On macOS clang they need `PLATFORMIO_BUILD_FLAGS="-include stdlib.h"`, because upstream `ConfigSerializer.cpp` misses `<stdlib.h>` (this is older than this change; on Linux it builds as-is).

**Decisions:** no buzzer on V3/T114 yet. V3 has no GPS (location messages carry `[no GPS]`). T114 gets `ENV_INCLUDE_GPS=1` for the optional module, like the upstream T114 companion.

**Fix after the first device test:** on USB without a battery, the red charger LED of both boards flashed once per second, and fast while a selected message scrolled. Cause: `renderBattery()` read the battery on every frame (50 ms during the marquee), and each reading switches the battery divider on (V3 GPIO37, T114 P0.06). The icon now reads it every 10 s (`PAGER_BATT_READ_MILLIS`).

**Open:** device test on a real V3 and T114 (display scaling, button, LED, BLE setup upload, `/pager_cfg` on LittleFS).

---

## 2026-09-26 — Radio region and frequency in the setup YAML (FRD-023)

**Request:** a region/country setting in the YAML, so the MHz can be controlled through it.

**Done**
- New optional block `radio:` with `region` (required), `frequency` (MHz, ≤ 3 decimals), `bandwidth` (kHz), `spreading_factor`, `coding_rate`. The region defines the allowed band, and frequency ± bandwidth/2 must fit into it: EU/UK 863–870, US/CA 902–928, AU/NZ 915–928, IN 865–867, KR 920–923.
- Presets only where the values are verified in this repo: **EU/UK** = `pager.ini` (869.618/62.5/8/5), **US/CA** = MeshCore FAQ (910.525/62.5/7/5). The other regions need all four values, so no frequencies are guessed.
- `MyMesh::loadPagerRadioPrefs()`: pager.ini first, then the setup radio. It runs at boot, and after a successful upload or `clear`; `radio_driver.setParams()` is called only when something changed. The summary is extended with `radio=<region|build>:MHz/kHz/SF/CR`, and its buffer grows to one frame.
- Flash format: optional radio fields behind flag `0x08`. Old blobs still load.
- Tests: 4 new parser tests (preset, explicit values, 12 error cases, round trip). All 60 native tests pass. `heltec_v4_pager` and `heltec_v4_companion_radio_ble` build.
- Docs: protocol (key table, region table, summary), frontend handoff (JSON schema, validation, "same radio for the whole group"), both example YAMLs with `radio: region: EU`, FRD-023, checklist M4d.

**Decisions:** TX power and duty cycle aren't part of the setup. The pager only checks the band, and legal use of the frequency is up to the group.

**Deployed:** Anna and Bob run `v0.4.4-m4d` (app at `0x10000`, pairings kept) with the kids setup plus `radio: {region: EU}`. Both report `radio=EU:869.618/62.5/8/5`.

**Open:** frequency change test (checklist M4d, steps 3–7).

---

## 2026-09-26 — Pickers: selected option at double size; M4c device test

**Request:** in the send and reply menus, the selected entry should be double size, like the selected message.

**Done:** `UITask::renderPicker()` uses the same layout as `renderChatSelected()`. The previous option is small for context, then the current option sits in an inverted block at size 2 (marquee for long text, emoji at 16×16), then the next options are small. Without a previous option, two following ones fit. The marquee restarts when the option changes (`_option` is part of the scroll key). Firmware `v0.4.3-m4c`, flashed to both pagers as the app image at `0x10000`. Pairings and setup were kept.

**Device test M4c (Anna + Bob, kids setup), all ✅**
- Send menu `1/12`, the selected entry is large, emoji shown as pictures.
- `Wo bist du? 📍` shows up on the receiver small in the list and large when selected. The reply menu offers 🏠, 🏫, ⚽ and `Mein Standort`, each large when selected.
- The ❤️ reply shows as one heart, with no block after it.
- 🚲 and 🚽 are readable at 8×8 on the real display.
- Not tested: checklist M4c steps 5 (emoji from the app with a skin tone / unknown emoji) and 7 (marquee with emoji).

---

## 2026-09-26 — Flash update without losing Bluetooth pairings; kids setup on both pagers

**Findings**
- The **merged image at `0x0` deletes the BLE pairings**: it overwrites the NVS partition (`0x9000`, 20 KB), where the ESP32 stores its bonds. The setup and prefs live in SPIFFS (`0xc90000`) and survive. The symptom: the computer still has the pairing, but the pager drops every connection after ~2 s (authentication failure). This also explains the "lost pairing" in the M4b test. **For updates, use only `firmware.bin` at `0x10000`.** That's how the flashing guide already put it; now it also gives the reason.
- Confirmed on the device: Bob (merged at `0x0`) had to be paired again. Anna (app at `0x10000`) connected right away, with her setup intact.
- `build.sh` runs `rm -rf out` at the start. Test files in `out/` are lost on the next build, including the M4b test YAMLs and their key. Local setup files now go in **`pager-setups/`** (gitignored, because they contain group keys).

**Status:** Anna and Bob run `v0.4.2-m4c-51096e0e` with the kids setup (`pager_setup_kids.yaml`, a new group key, 8 questions). Ready for the M4c device test ([checklist](docs/pager_testing.md)).

---

## 2026-09-26 — Emoji pictures for kids (FRD-022)

**Request:** more emoji, so kids can use the pager. They must show on the pager.

**Done**
- **25 pictures, 82 codepoints** (including aliases such as every heart colour and similar smileys): 📞 👍 👎 ❤️ 😀 😢 😡 😴 👋 🏠 🏫 🚗 🚌 🚲 🍴 🥤 🚽 ⚽ ⏰ 📍 🆘 ✅ ❌ ❓ 🩹. Each one exists as 8×8 (chat, menus) and 16×16 (selected message, detail). The small ones leave row 7 empty, the large ones rows 14–15, the same as the font.
- The pictures are ASCII art in `bin/gen_emoji_glyphs.py`. The script generates `EmojiGlyphs.h`, `docs/pager_emoji.md` and a preview `docs/pager_emoji.png`. The 📞 glyph was carried over bit for bit (checked in a unit test).
- `ui-pager/PagerText.h` handles UTF-8 decoding, binary search over the codepoints and CP437 umlauts. U+FE0F, ZWJ and skin tones take no width. `UITask` now draws through one path (`pagerNextCell` / `drawCell`) instead of three copies of the phone special case.
- Tests: `test/test_pager_text` (6 tests). All 56 native tests pass, and the firmware builds (flash +1.5 KB, RAM unchanged).
- Example [docs/pager_setup_kids.yaml](docs/pager_setup_kids.yaml) with 8 questions for children. The frontend docs now point to the supported set and suggest a matching emoji picker.

**Decisions**
- Chose a fixed, curated set rather than a full emoji font. 8×8 only works for simple, clear shapes, and flash/RAM stay small.
- The pictures were chosen for children: yes/no, feelings, places, ways to travel, needs (eating, drinking, toilet, hurt, help), time.

**Open:** device test ([checklist M4c](docs/pager_testing.md)). Some 8×8 pictures (bicycle, toilet) are only just recognizable at that size and should be checked on the real display.

---

## 2026-09-26 — M4b device test: setup over BLE (FRD-021)

**Setup:** two Heltec V4 flashed with `heltec_v4_pager-v0.4.1-m4b-35aeeb8c-merged.bin` (esptool, `0x0`, no erase). Before the test they were running as `Bob` and `Eve`, both with channel `Pager` and the build key. Setup files: `a.yaml` (Anna) and `bob.yaml` (Bob), both with the same group (`Familie`, a new random key, 3 questions from `docs/pager_setup_example.yaml`). Uploaded with `bin/pager_setup.py` (bleak) from Ubuntu/BlueZ.

**Results (checklist M4b)**

| Step | Result |
|---|---|
| 1–2 scan, upload `a.yaml` | ✅ `nickname=Anna channel=Familie key=setup questions=3` |
| 3–4 popup, header, status | ✅ |
| 5 second pager `bob.yaml` | ✅ `nickname=Bob channel=Familie key=setup questions=3` (before: `Eve`/`Pager`/`build`) |
| 6 send menu `1/6`, no duplicate `Angekommen?` | ✅ |
| 7 reply menu for `Wann kommst du?` = `5 min`, `30 min`, `Später`, 📞 | ✅ |
| 8 reply `Später` arrives as `»Bob: @Anna Später`, `ä` rendered correctly | ✅ |
| 9 `Angekommen?` → replies from the setup (`Ja`, `Noch nicht`, `Mein Standort`) | ✅ |
| 10 other message → default replies | ✅ |
| 11–12 invalid files: `:2: unknown key 'nicknme'`, `:3: more than 6 replies`, `:2: question text longer than 40 bytes`. Afterwards the status was unchanged | ✅ |
| 13 reboot (reset over USB): setup kept, BLE name changes to `MeshCore-Anna` | ✅ |
| 16 `clear`: `channel=Pager key=build questions=0`, nickname stays | ✅ (then `a.yaml` uploaded again) |
| 14 name from the app reset at boot, 15 upload while a picker is open, 17 stock app afterwards | not tested |

**Pitfalls found (setup, not firmware)**
- **Serial port:** the user isn't in `dialout`, so esptool fails with "Permission denied" / "port doesn't exist". Fix: install the PlatformIO udev rules (or `chmod` once). `pio run -t upload` also loses the port after the 1200-bps reset. Flashing directly with esptool (`write_flash 0x0 …-merged.bin`) works.
- **BLE pairing from Linux:** after `bluetoothctl pair`, BlueZ stays connected. The pager then stops advertising, and bleak reports "device not found". Always run `bluetoothctl disconnect <addr>` after pairing.
- On the first attempt, the pager dropped every connection after about 2 s (authentication failure, the pager had lost the bond). `remove` + pairing again fixed it. The cause wasn't pinned down; it's likely a reset during or after pairing.

**Status:** FRD-021 works on the device. Still open: steps 14, 15, 17.

---

## 2026-09-26 — Setup over BLE: YAML with nickname, channel, questions (FRD-021)

**Request:** a setup frontend creates a YAML file and uploads it over Bluetooth to pagers that are already flashed. It controls the channel key, the names, and extra questions with their own reply sets, on top of the built-in canned messages.

**Decisions (with the user)**
- "Name" means both the **nickname** (per pager) and the **channel name** (per group).
- **Replies per question.** A receiver looks up the incoming text in its **own** setup, so all pagers of a group need the same `channel` + `questions`. Unknown texts get the default replies.
- **The ESP parses the YAML** and stores it as a compact binary blob (`/pager_cfg`, the example is 162 bytes). It is not stored as YAML, and the frontend doesn't translate it into binary.
- **The setup overrides the build config.** `pager.ini`/`pager.secret.ini` stay as they are (the build still requires the key) and serve as the fallback.

**Done**
- `ui-pager/PagerConfig.{h,cpp}`: a YAML-subset parser (block style, comments, quotes with `\x`/`\u`/`\U` escapes, flow lists, flow mapping for `channel`). It gives errors with line numbers, supports the PyYAML default output, has the flash format, and needs no Arduino, so there are native unit tests (`test/test_pager_config`, 10 tests).
- Companion command `0x70` with BEGIN/DATA/COMMIT/STATUS/CLEAR, handled in `MyMesh::handlePagerConfigCmd()`. It uses a static 4 KB buffer and no heap. A COMMIT only saves and applies if validation passed. Protocol for the frontend: [docs/pager_config_protocol.md](docs/pager_config_protocol.md).
- Every value is optional. Everything set is **enforced at every boot**. If the channel is renamed, the old pager channel is removed.
- UI: the send menu shows the built-ins plus the setup questions (a built-in text only replaces the replies). The reply menu depends on the question. On a new setup the pager shows `Setup updated`, and open pickers go back to chat.
- OLED: `äöüÄÖÜß é °` now come from the CP437 font instead of a block.
- `bin/pager_setup.py` (bleak) is a reference client and test tool.
- Build: RAM 8.3 % → 9.1 %. `heltec_v4_companion_radio_ble` still builds unchanged. All native tests pass.

**Limits / open**
- 12 questions, 6 replies each, 40 bytes per text (length budget FRD-012). The YAML can be at most 4 KB.
- The BLE name (`MeshCore-<nick>`) only changes after a reboot.
- The setup can't be read back as YAML yet (STATUS only gives a summary). Radio settings aren't part of the setup yet.
- Not yet tested on a device ([checklist M4b](docs/pager_testing.md)).

---

## 2026-09-26 — Message sound: two tones

**Request:** just two consecutive sounds, the second slightly longer and lower.

**Done:** `uhoh:d=8,o=7,b=200:g,32p,d.` gives G7 (~3.1 kHz, 150 ms), a ~40 ms gap, then D7 (~2.35 kHz, dotted = 225 ms). This replaces the intonation variant with the grace note and slide. NonBlockingRTTTL supports dotted notes (`d.` = 1.5× length).

---

## 2026-09-26 — "uh-oh" closer to the original intonation

**Request:** the classic melodic ICQ "oh oh".

**Note:** the original is a voice sample. There are no official notes, and it can't be reproduced as speech on a square-wave speaker. The melody now follows the **spoken intonation** instead of two flat notes:
- "uh": short, with a grace note sliding up (F#7→G7, ~180 ms)
- ~60 ms gap
- "oh": a fourth lower and twice as long (D7, 240 ms), falling away in steps (C#7, C7, B6)

It's about 0.66 s in total. Melody: `uhoh:d=32,o=7,b=125:f#,16g,32p,8d,c#,c,b6`. It's tunable on the device (tempo/pitch/lengths) based on the user's ear.

---

## 2026-09-26 — ICQ-style "uh-oh" and drive level 3

**Requests:** louder via firmware, and the classic ICQ sound.

**Done:**
- The message sound is now an **ICQ-style "uh-oh"** melody imitation (`uhoh:d=16,o=7,b=140:8g,32p,8d,c#`), transposed up to ~2.2–3.1 kHz for volume. The original is a voice recording: it can't be reproduced on a square-wave speaker and is copyrighted, so it isn't embedded.
- **Drive strength 3** (~40 mA, the ESP32-S3's absolute maximum per pin), set in the env **at the user's explicit request** after the advice that level 2 (~20 mA) is the recommended limit.

**Risk accepted by the user:** long-term pin wear with the speaker wired directly. Mitigated by short tones (~0.4 s) and the 50 % duty square wave. Fallback: `PAGER_BUZZER_DRIVE=1–2`, another pin, or a transistor.

---

## 2026-09-26 — Louder beep (option 1: firmware only)

**Request:** make the beep louder. Options offered: 1) firmware only, 2) push-pull on two GPIOs (~4× power, one wire moved), 3) NPN transistor. **The user chose option 1.**

**Done:**
- Drive strength `PAGER_BUZZER_DRIVE` 0 → **1** (≈5 → ≈10 mA, still within the ESP32-S3's recommended per-pin current).
- Tone 2.6 → **~3.1 kHz** (G7), closer to where small speakers and the ear are most sensitive.
- Pattern: 2 × 83 ms → **3 × 150 ms** (more acoustic energy).
- The melody can be overridden via `PAGER_BEEP_MELODY`.

---

## 2026-09-26 — New-message beep via speaker on GPIO 4 (FRD-020)

**Request:** a speaker (identified as a plain 8–32 Ω speaker; the user asked for no resistor) should beep on every new message.

**Done:**
- Env flags `PIN_BUZZER=4` (optional; remove it for no sound) and `PAGER_BUZZER_DRIVE=0`, plus MeshCore's `buzzer.cpp` and the NonBlockingRTTTL library.
- `UITask::beep()` plays a short double beep (`msg:d=16,o=7,b=180:e,p,e`, ~2.6 kHz) for messages from others, the same trigger as the LED alert, without repeats.
- **Pin protection for the no-resistor wiring:** `gpio_set_drive_capability(…, GPIO_DRIVE_CAP_0)` at start and on every loop pass while a tone plays (because `tone()` may re-attach the pin), then pin LOW when done. A tone only, never steady HIGH through the coil.
- Wiring and free-pin list in the flashing guide. Test steps M4-8b–8e.

**Trade-off:** at ~5 mA drive it's quiet (fine indoors). A transistor + `PAGER_BUZZER_DRIVE=3` is the path to more volume.

---

## 2026-09-26 — Channel key moved out of git (`pager.secret.ini`)

**Trigger:** the user asked to push the feature branch. `github.com/lboel/MSPager` turned out to be a **public fork** of `meshcore-dev/MeshCore`, which GitHub can't make private. Pushing would have published the group key from `pager.ini`.

**Decision (user):** keep the key out of git.

**Done:**
- The key moved to a gitignored **`pager.secret.ini`** (template `pager.secret.ini.example`), loaded via `extra_configs` after `pager.ini`. `pager.ini` only holds a placeholder.
- A missing or invalid secret makes the build fail with a clear `static_assert` hint.
- The 20 unpushed commits were rewritten with `git filter-branch` so `pager.ini` never contained the key. Verified: 0 occurrences in the branch history. The existing key stays valid (it never left the machine), so flashed pagers don't need a reflash.
- The docs (FRD-018, flashing guide, PRD) describe passing the key file on privately.

---

## 2026-09-25 — Hint format `1x / 2x / hold`

**Request:** show hints as `1x:<↑/↓> 2x:<←> hold:<send/open>`, only the options that are possible.

**Done:** all hints follow `1x:… 2x:… hold:…` (in that order, gestures without an action left out):
- overview: `2x:off hold:send` (empty), `1x:↑ 2x:off hold:send`, and with a selection `1x:↑ 2x:← hold:open`
- detail: `2x:← hold:reply`
- pickers: `1x:↓ 2x:← hold:send`
- pairing: `1x:cancel`

The overview without a selection now also shows `2x:off`, which fits in exactly 21 columns.

---

## 2026-09-25 — M4: Alerting & polish built

**User requests for M4:** no different blink speeds; blink until the overview is opened with a tap; the phone emoji should look more like a phone on the display (the wire stays the official emoji).

**Done**
- **LED alert** (FRD-014): GPIO 35 blinks 100/900 ms on every message from another pager, whether the display is on or off.
  - It stops when a press shows the overview: the wake press, the first press on an already-visible overview (acknowledge only, no selection), or returning to the overview from detail/picker.
  - Own messages and auto-answers don't alert.
- **TX flash removed** from the pager env (`build_unflags = -D P_LORA_TX_LED=35`). `HeltecV4Board.cpp` now guards its TX-LED writes with `#ifdef` (it used the macro unguarded, so the unflag broke the build). The stock companion is unchanged and still builds.
- **📞 glyph** (FRD-013): an old-style desk phone (handset + body with dial), 8×8, plus a separately designed 16×16 for double size (no longer pixel-doubled). The wire stays U+1F4DE.

**Decisions**
- The first press on an already-visible overview is **consumed** as an acknowledgement, like the wake press, so confirming an alert never selects a message by accident.
- The uniform blink replaces the FRD-014 idea of faster patterns for help/mentions.

---

## 2026-09-25 — Bug: scrolling text leaked into the next row

**Report:** in the detail view, a scrolling long message showed the next character on the line below.

**Cause:** Adafruit GFX has text wrap on by default. A character that would cross the right display edge is printed at the start of the next text row instead of being clipped. The marquee constantly places a character there.

**Fix:** new `DisplayDriver::setTextWrap(bool)` (default no-op; `SSD1306Display` passes it to Adafruit GFX). `UITask::begin()` switches wrap off, so text is clipped at the edge and characters scroll in smoothly. The shared driver change is additive only, and the stock companion still builds.

---

## 2026-09-25 — Arrow symbols in button hints

**Request:** replace "select" with ↑/↓ depending on the screen and "back" with ←, and show the Tap option on a selected message too.

**Done:** hints now use the CP437 arrows from the display font (`ARROW_UP/DOWN/LEFT`). `renderText()`/`renderTextRaw()` let 0x18–0x1B through instead of turning control characters into spaces.
- overview: `Tap:↑ Hold:send`, and with a selection `Tap:↑ Hold:open 2x:←`
- detail: `Hold:reply 2x:←`
- pickers: `Tap:↓ Hold:send 2x:←` (now also showing Tap, and ← for cancel)

All hints switched to single spaces so the longest (20 characters) fits the 21-column row.

---

## 2026-09-25 — Marquee scrolling for long text, single-line selection

**Request:** long text was only ellipsized. Add slow horizontal scrolling for the selected message (overview) and the detail message. Limit the selected message to one line so neighbours stay visible.

**Done:**
- `renderMarquee()`: time-based offset, 1.5 s pause, 20 px/s, 1.5 s pause at the end, restart. It only moves if the text is wider than the line. `renderTextRaw()` draws from a negative x and lets the display clip. The scroll position resets whenever the screen or selection changes.
- The display refreshes every **50 ms only while a marquee is moving**, otherwise every 1 s as before.
- **Selected block:** sender row + **one** size-2 line (24 px), so there's always one older line above and one newer line below.
- **Detail:** the body is one size-2 line with the same marquee (was: wrapped to 2 lines). This keeps both behaving the same. Easy to revert to "wrap if it fits, else scroll" if preferred.
- Removed the now-unused `renderWrapped()`.

---

## 2026-09-25 — GPS status icon in header

**Request:** a status icon left of the battery: a location pin with a GPS fix, crossed out without one. The position must be checked periodically in the background.

**Done:** 8×8 bitmaps (filled pin with hole / hollow pin with a diagonal slash) drawn by `renderHeader()` between the name and the battery. The background check already existed from M3 (`updateFix()`, every 1 s, independent of the display). New: a fix counts as **current** only if the last valid reading is < 5 s old (`GPS_FIX_CURRENT_MILLIS`), because `_has_fix` alone stays true after the first fix. The name budget drops to 14 characters.

---

## 2026-09-25 — M3: Location built

**Done**
- **GPS always on:** the pager build forces `gps_enabled = 1` in `MyMesh::begin()` (applied by `applyGpsPrefs()` in `main`). `UITask::updateFix()` polls the location provider every 1 s and keeps the last fix + time.
- **Position suffix on every message** (FRD-010): `[lat,lon]` with 4 decimals. With a fix older than 2 min: `[lat,lon ~Nmin|h|d]`. Never had a fix: `[no GPS]`. Sending is never blocked.
- **Parsing on receive** (FRD-012): the suffix is stripped from the body into `PagerMsg::pos`. It's tolerant of 1–6 decimals and leaves malformed suffixes and free text untouched.
- **New `ui-pager/PagerLocation.h`** with format/parse/haversine/compass helpers, host-tested (20 checks incl. Brandenburger Tor → Fernsehturm = 2.2 km E, Munich → Berlin = 504 km N).
- **Detail view** (FRD-005): the title shows sender + relative position (`1.2km NE`, `~` = stale, `you`, `no GPS`, `no own GPS`). Body at double size. Then absolute coordinates + message age.
- **Location messages** (FRD-019, user request): `Standort?` / `Mein Standort` in both pickers.
  - `Standort?` is answered automatically with `@[requester] Mein Standort` after a random 0.5–4 s delay: by everyone for a group request, only by the mentioned pager for a reply.
  - The answer appears in the answering pager's own chat (transparency).
  - `Mein Standort` shows distance/direction in the chat list.
- **Reply picker:** 6 options, so it scrolls through 4 visible rows with an `n/N` counter in the title.

**Decisions**
- **German wording** `Standort?` / `Mein Standort`, consistent with "canned messages in German" (the user named them "location?" / "share location").
- **Random answer delay**, because several pagers answering a group request at the same moment would collide.
- **Relative position** is computed at render time from the *current* own fix to the sender's position *as sent*.

**Process note:** the first commit of M3 (`e2c29eab`) went in with only part of the docs, because a doc-edit script failed after the code was committed. The docs follow in a separate commit.

---

## 2026-09-25 — Button hints on the main screen

**Request:** the main screen needs button hints like the other screens, short and consistent.

**Done:** a shared `renderHint()` (separator at y 53, text at y 56) used by every screen:
- chat: `Hold:send  2x:off` (empty), `Tap:select  Hold:send` (list), `Hold:open  2x:back` (selected)
- detail: `Hold:reply  2x:back`
- pickers: `Hold:send  2x:cancel`
- pairing: `Tap:cancel` (previously "press: cancel")

**Layout cost:** the chat list shows **5** lines instead of 6. The enlarged block lost its padding, and the older context line is only shown when the selected body fits on one line, so a 2-line body still fits above the hint.

---

## 2026-09-25 — Change: header = name + battery icon, PIN removed

**Request:** remove the always-shown BLE code, and show the battery as a symbol instead of a percentage.

**Done:** `renderHeader()` shows the nickname left (up to 16 characters) and the battery icon right (fill = charge). The PIN is now only on the boot screen (5 s) and in pairing mode (hold PRG 10 s, FRD-017). This reverts the two header changes from earlier today.

---

## 2026-09-25 — Accessibility: large selected message and detail text

**Request:** show the selected message at about twice the font size, and also the message in the detail view.

**Done:**
- `renderText()` / `textWidth()` now take a text size (1 or 2). New `renderWrapped()` does word wrapping with a maximum line count and an ellipsis on the last line.
- Size 2 advances **11 px** per character instead of GFX's 12 px, so 11 characters fit in 128 px and `Angekommen?` stays on one line.
- A 16×16 handset glyph for 📞 at size 2.
- **Chat with a selection** (`renderChatSelected()`): one older line for context, then an inverted block (sender small, body at double size, up to 2 lines), then newer lines as space allows. Without a selection, the list is unchanged.
- **Detail:** the age moved to the title bar (right) to make room, and the body is at double size (2 lines). The position line and hint stay small.
- `markVisibleRead()` matches the new selected layout.

---

## 2026-09-25 — Group config file `pager.ini` (FRD-018) + EU radio defaults

**Test feedback:** a message from one pager didn't arrive at the other. Cause: M2 gave every pager its own random channel key, and the keys were never shared by QR.

**Request:** a git-tracked config file with the channel name and key that all flashed pagers follow. Also initialise the radio to EU standard settings.

**Done:**
- `pager.ini` (repo root, included via `extra_configs`): `channel_name`, `channel_key` (32 hex, freshly generated), `radio_freq/bw/sf/cr` = EU/UK Narrow 869.618 / 62.5 / 8 / 5.
- `MyMesh::ensurePagerChannel()` now **enforces** it at every boot: exactly one channel with that name and key. The key is overwritten if different, and duplicates from earlier QR imports are removed.
- The radio settings from `pager.ini` are applied at every boot (before `radio_driver.setParams`).
- `findPagerChannel()` is back to first match (duplicates can no longer exist after boot).
- A key length other than 32 fails the build (`static_assert`).

**Decisions:**
- **Reverses "random key per device"** (M2) in favour of zero-touch group setup. Trade-off: the key is in git and in every `.bin`, so the repo must stay private. Rotating the key means reflashing all pagers.
- Hex instead of base64: `base64.hpp` has non-inline definitions and can only be included once (`BaseChatMesh.cpp`).
- Settings are *enforced* rather than just defaulted, so a stray app edit can't split the group permanently.
- CR 5 = MeshCore's default (`LORA_CR`). LoRa carries the CR in the packet header, so nodes on a different CR still decode each other.

---

## 2026-09-25 — Bug: pager only briefly visible in the app → BLE pairing mode (FRD-017)

**Report:** one of two flashed pagers is only visible in the MeshCore app shortly after boot. Pairing doesn't fail.

**Analysis:** `SerialBLEInterface` (ESP32) stops advertising while a peer is connected and only restarts once `getConnectedCount() == 0`. A phone holding a background link (the app auto-reconnect, or a second phone) makes the pager invisible. This is most likely what happened here. The firmware itself isn't broken.

**Fix (as proposed by the user):** hold PRG ≥ 10 s → pairing mode:
- BLE is disabled (drops the current link) and re-enabled 0.5 s later (advertising restarts).
- The PIN is shown in size-3 digits with a 30 s countdown.
- It ends on a new authenticated connection (`Connected`), a timeout (`Pairing timeout`) or a press (cancel).
- The 10 s hold is tracked in `UITask::checkPairingHold()` because `MomentaryButton` only reports the 1 s long press.

**Caveat:** the 1 s long press still fires first, so a hold started on a picker sends at 1 s. Documented: start from chat or with the display off. Bonds are not cleared.

---

## 2026-09-25 — Change: battery as percentage, no icon

**Request:** show the percentage, not the battery symbol.

**Done:** `renderBattery()` prints `NN%` right-aligned in a fixed 4-column slot (so the PIN doesn't move as the value changes). With 4 px gaps, the name still gets 10 characters.

---

## 2026-09-25 — Change: header always shows name + PIN + battery

**Request:** for simplicity, always show the pager's name and BLE PIN, on the same line as the battery.

**Done:** `UITask::renderHeader()`: nickname left, PIN right-aligned 3 px before the battery icon. This replaces the "PIN until first connection" behaviour from the previous entry (`_ble_seen` removed).

**Trade-off:** there's no room for the battery **percentage** text. A 10-character name + space + 6-digit PIN + icon fill the 128 px line, so the icon fill alone shows the charge. Names over 10 characters are ellipsized.

---

## 2026-09-25 — Feature: BLE PIN in chat header

**Request:** show the Bluetooth connection code top left until a connection happens after boot, and again after every reset.

**Done:** the chat header shows `PIN <pin>` top left until `hasConnection()` is true for the first time since boot (`UITask::_ble_seen`, RAM only, so it resets with every reboot). It fits next to the battery (0–59 px vs. 78+ px). FRD-004 and FRD-015 are updated, and the M2 checklist has steps 16b–16d.

---

## 2026-09-25 — M2 fix: pager-sent messages visible in the app

**Test feedback:** a message sent from the pager's button didn't show up in the app connected to that pager.

**Cause:** the companion protocol only reports messages the app sent itself (`PACKET_MSG_SENT`) or messages received over the air. There's no frame for "the device sent this on its own".

**Fix:** `MyMesh::sendPagerMessage()` now also queues the message for the app as a received channel message `<nickname>: <text>` (0 hops, SNR 0), with the same timestamp as the on-air packet. To avoid duplicating code, the frame building moved out of `onChannelMessageRecv()` into `MyMesh::queueChannelMsgForApp()` (the stock receive path is byte-identical).

**Limitation:** the stock app shows these as incoming messages from the own nickname, not as own ("sent") bubbles. Fixing that would need an app change.

---

## 2026-09-25 — M2: Messaging built

**Done**
- Messages are now actually sent: `MyMesh::sendPagerMessage()` sends the canned text (plus mention) on the pager channel via `sendGroupMessage`. The UI shows `Sent` or `Send failed`.
- **The channel appears in the app by default:** `MyMesh::ensurePagerChannel()` runs after `loadChannels()`. If there's no `Pager` channel, it creates one in the first free slot with a **random 128-bit key** and saves it.
- Lookup by name via `MyMesh::findPagerChannel()`. **Last match wins**, so importing a group QR without deleting the auto-created channel still works.
- Receive filter: only pager-channel messages reach the UI. Other channels and DMs still go to the app's offline queue but never touch the display.
- Messages the app sends into `Pager` over BLE appear on the sending pager as `me: …` (no alert). Messages whose sender equals the own node name are treated as own.
- Mentions: `@[<own nick>]` sets the "for me" flag, shown as `»` in chat.
- Fix: the unread bullet (CP437 0x07) was rendered as a space in M1. Markers are now printed raw.
- All pager changes in `MyMesh` are behind `#ifdef PAGER_CHANNEL_NAME`. The stock `heltec_v4_companion_radio_ble` still builds unchanged.
- Test checklist: [docs/pager_testing.md § M2](docs/pager_testing.md).

**Decisions**
- **Random key per device** for the auto-created channel (the user's choice over a build-time group key or a fixed repo key). It shows up in the app but needs one key shared by QR to form a group. There's no secret in the firmware or repo.
- A deleted `Pager` channel is re-created with a new key on the next reboot.

**Open**
- M1 hasn't been device-tested yet. Both checklists are pending.

**Next**
- M3: GPS always on, position suffix (4 decimals), parsing, distance/bearing in detail.

---

## 2026-09-25 — M1: Skeleton built

**Done**
- New env `heltec_v4_pager` in `variants/heltec_v4/platformio.ini` (derived from `heltec_v4_companion_radio_ble`, with `ui-new` swapped for `ui-pager`).
- New `examples/companion_radio/ui-pager/UITask.{h,cpp}`. It's a drop-in replacement for the companion `UITask` (no changes to `MyMesh` or `main.cpp`):
  - screens: boot (name, version, BLE PIN), chat (battery + latest 6), detail, compose picker, reply picker
  - PRG state machine per FRD-002, display power per FRD-003, 16-message inbox ring buffer (FRD-016)
  - 📞 drawn as an 8×8 glyph (FRD-013, pulled forward because the reply picker needs it)
- Build: `pio run -e heltec_v4_pager` succeeds (RAM 8.3 %, Flash 19.3 %). `build.sh` produces `out/heltec_v4_pager-v0.1.0-m1-<sha>{,-merged}.bin`.
- Manual test checklist: [docs/pager_testing.md](docs/pager_testing.md).

**Decisions / deviations**
- Long press is **1000 ms**, not the 800 ms first written in FRD-002. The shared `user_btn` in `variants/heltec_v4/target.cpp` defines it and we don't want to touch the variant. FRD-002 is updated.
- The companion **rescue CLI** is kept: a long press within the first 8 s after boot enters it (recovery path if BLE config breaks).
- M1 sending is a **local echo only** (`M1: local only` popup), so the UI can be tested on a single device. M1 shows messages from every channel. Filtering comes in M2.

**Next**
- Device test (checklist M1). Then M2: pager-channel lookup by name, real `sendGroupMessage`, receive filter.

---

## 2026-09-25 — M0: Specification

**Done**
- Wrote [PRD.md](PRD.md), the FRD index [FRD.md](FRD.md) with 16 numbered FRDs in [`frd/`](frd/), [ROADMAP.md](ROADMAP.md) and the [flashing guide](docs/pager_flashing.md).

**Decisions**
- **Base = companion radio fork** (`examples/companion_radio`) with a new `ui-pager` UI. We keep the BLE companion protocol so the stock MeshCore app can configure nickname, radio and channel. The rejected alternative was a USB-serial CLI (less code, but needs a computer for every change).
- **RST button can't be reused.** On the Heltec V4 it's wired to ESP32-S3 CHIP_PU/EN, so it resets the chip in hardware. The UI is PRG-only: short = next, long = select/send, double = back. An external button was rejected to keep all devices identical and unmodified.
- **Pager channel chosen by name** (`PAGER_CHANNEL_NAME="Pager"`), not by slot. Public and other channels are hidden on the pager but still forwarded to the app.
- **Canned catalogue**: start `Angekommen?`, `Brauche Hilfe` (a statement, no "?"). Replies `Ja`, `Nein`, `OK`, `📞`.
- **📞 on the OLED**: the stock GFX font can't render emoji. It goes over the wire as a real UTF-8 emoji (so apps show it) and is drawn on the OLED as a custom 8×8 bitmap.
- **Replies** use the MeshCore mention convention `@[Nick] …` and go to the whole channel.
- **Position** is plain text appended to every message, **4 decimals (≈10 m max precision)**. Stale fix: last fix + age (`~12min`; `min` rather than `m` to avoid confusion with metres). No fix ever: `[no GPS]`. Sending is never blocked.
- **Display off when idle.** The first press only wakes it, and a new message wakes it too. The main screen shows **battery + latest chat messages together**, nothing else.
- **Alerting**: display wake + LED (GPIO 35) blinking until read. The pager UI takes over the LED from the LoRa TX blink.
- **BLE always on**. The battery impact will be measured in M5.
- **Language**: UI and docs in English, canned messages in German.

**Open questions**
- Whether the GPS module can stay on continuously within the battery budget. Measure in M5.
- Exact unread/read semantics may need tuning after the first user test.

**Next**
- M1: add the `heltec_v4_pager` env and the `ui-pager` skeleton.
