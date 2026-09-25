# MSPager — Roadmap

Milestones are sequential. Each one ends with a working, flashable build. Requirements: [FRD.md](FRD.md).

| Milestone | Goal | FRDs | Exit criteria | Status |
|---|---|---|---|---|
| **M0 — Specification** | PRD, FRDs, roadmap, devlog, flashing guide | all | Docs reviewed and agreed | ✅ Done (2026-09-25) |
| **M1 — Skeleton** | `heltec_v4_pager` env builds. New `ui-pager` UITask with display off/wake, button state machine and battery header | 001, 002, 003, 004 (layout) | Flashes to a V4. Wake/timeout and all gestures work on empty screens | 🧪 Built, awaiting device test ([checklist](docs/pager_testing.md)) |
| **M2 — Messaging** | Pager channel by name, compose/reply pickers, sending canned messages, receiving + showing in Chat, nickname parsing, mentions, inbox | 004, 006, 007, 008, 009, 012, 015, 016 | Two pagers exchange all 6 canned messages with correct nicknames. Public channel is ignored | 🧪 Built, awaiting device test ([checklist](docs/pager_testing.md)) |
| **M3 — Location** | GPS always on, last fix kept, position suffix (4 decimals), parsing, detail screen with distance/bearing + coords | 005, 010, 011, 012 | Distance/bearing within ±20 m / ±1 sector outdoors. `[no GPS]` path works | |
| **M4 — Alerting & polish** | LED blink until read (fast for help/mentions), 📞 glyph, UI texts, edge cases | 013, 014 | Alert behaviour and 📞 verified on device and in the MeshCore app | |
| **M5 — Field test & release** | ≥3 pagers outdoors incl. a repeater hop. Battery measurement. Tagged release with a merged `.bin` | — | PRD §10 success criteria met. Release notes in DEVLOG | |

## Backlog (not scheduled)
- "Heard by repeater" indicator after sending (pseudo-delivery feedback)
- Optional BLE auto-off after N minutes to save battery (revisit after the M5 measurement)
- Configurable canned catalogue via app/CLI instead of compile-time
- Printed one-page quick-reference card for users
