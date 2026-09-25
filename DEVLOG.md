# MSPager — Development Log

Newest entries at the top. Record decisions, their reasons, and anything surprising.

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
