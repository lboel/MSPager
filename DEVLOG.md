# MSPager — Development Log

Newest entries at the top. Record decisions, their reasons, and anything surprising.

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
