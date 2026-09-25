# MSPager — Development Log

Newest entries at the top. Record decisions, their reasons, and anything surprising.

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
