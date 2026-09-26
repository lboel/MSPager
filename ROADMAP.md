# MSPager — Roadmap

Milestones are sequential. Each one ends with a working, flashable build. Requirements: [FRD.md](FRD.md).

| Milestone | Goal | FRDs | Exit criteria | Status |
|---|---|---|---|---|
| **M0 — Specification** | PRD, FRDs, roadmap, devlog, flashing guide | all | Docs reviewed and agreed | ✅ Done (2026-09-25) |
| **M1 — Skeleton** | `heltec_v4_pager` env builds. New `ui-pager` UITask with display off/wake, button state machine and battery header | 001, 002, 003, 004 (layout) | Flashes to a V4. Wake/timeout and all gestures work on empty screens | 🧪 Built, awaiting device test ([checklist](docs/pager_testing.md)) |
| **M2 — Messaging** | Pager channel by name, compose/reply pickers, sending canned messages, receiving + showing in Chat, nickname parsing, mentions, inbox | 004, 006, 007, 008, 009, 012, 015, 016 | Two pagers exchange all 6 canned messages with correct nicknames. Public channel is ignored | 🧪 Built, awaiting device test ([checklist](docs/pager_testing.md)). Includes FRD-017 (BLE pairing mode) |
| **M3 — Location** | GPS always on, last fix kept, position suffix (4 decimals), parsing, detail screen with distance/bearing + coords | 005, 010, 011, 012, 019 | Distance/bearing within ±20 m / ±1 sector outdoors. `[no GPS]` path works | 🧪 Built, awaiting device test ([checklist](docs/pager_testing.md)). Includes FRD-019 (`Standort?` / `Mein Standort`) |
| **M4 — Alerting & polish** | LED blink until the overview is acknowledged (one uniform pattern), 📞 as desk-phone glyph | 013, 014, 020 | Alert behaviour, beep and 📞 verified on device and in the MeshCore app | 🧪 Built, awaiting device test ([checklist](docs/pager_testing.md)) |
| **M4b — Setup over BLE** | A setup frontend uploads a YAML (nickname, channel name/key, extra questions with their own replies) to a flashed pager. The pager parses, validates and stores it ([protocol](docs/pager_config_protocol.md)) | 021 | Upload, reject with line number, questions/replies on two pagers, survives reboot | 🧪 Built + parser unit tests, awaiting device test ([checklist](docs/pager_testing.md)) |
| **M4c — Emoji for kids** | 25 important emoji drawn as pictures (small + large), so setup texts/replies work for children ([list](docs/pager_emoji.md)) | 022 | Kids setup shows all emoji as pictures, 📞 unchanged | 🧪 Built + unit tests, awaiting device test |
| **M5 — Field test & release** | ≥3 pagers outdoors incl. a repeater hop. Battery measurement. Tagged release with a merged `.bin` | — | PRD §10 success criteria met. Release notes in DEVLOG | |

## Backlog (not scheduled)
- "Heard by repeater" indicator after sending (pseudo-delivery feedback)
- Optional BLE auto-off after N minutes to save battery (revisit after the M5 measurement)
- ~~Configurable canned catalogue via app/CLI instead of compile-time~~: done in M4b (FRD-021, extra questions; the built-ins stay)
- Read the active setup back as YAML (currently only a STATUS summary)
- Radio settings in the setup YAML
- Printed one-page quick-reference card for users
