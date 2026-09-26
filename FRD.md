# MSPager — Functional Requirements (index)

Each functional requirement lives in its own numbered file under [`frd/`](frd/). IDs are permanent. Superseded FRDs are marked, never renumbered. For product context, see [PRD.md](PRD.md). For sequencing, see [ROADMAP.md](ROADMAP.md).

**Status values:** Proposed → Accepted → Implemented → Verified (or Superseded).

| ID | Title | Milestone | Status |
|---|---|---|---|
| [FRD-001](frd/FRD-001-hardware-target.md) | Hardware target (Heltec V4 OLED, env `heltec_v4_pager`) | M1 | Implemented |
| [FRD-002](frd/FRD-002-single-button-input.md) | Single-button input (PRG short/long/double) | M1 | Implemented |
| [FRD-003](frd/FRD-003-display-power.md) | Display power (off when idle, wake-press consumed) | M1 | Implemented |
| [FRD-004](frd/FRD-004-main-screen.md) | Main screen: battery + chat room together | M1/M2 | Implemented |
| [FRD-005](frd/FRD-005-message-detail.md) | Message detail (nickname, distance/bearing, coords) | M3 | Implemented |
| [FRD-006](frd/FRD-006-canned-messages.md) | Canned messages (compose + reply pickers) | M2 | Implemented |
| [FRD-007](frd/FRD-007-replies-with-mention.md) | Replies with `@[Nick]` mention | M2 | Implemented |
| [FRD-008](frd/FRD-008-pager-channel-selection.md) | Pager channel by name (from `pager.ini`), others hidden | M2 | Implemented |
| [FRD-009](frd/FRD-009-sender-nickname.md) | Sender nickname = MeshCore node name | M2 | Implemented |
| [FRD-010](frd/FRD-010-gps-attachment.md) | GPS attachment (4 decimals ≈ 10 m, last fix + age) | M3 | Implemented |
| [FRD-011](frd/FRD-011-gps-acquisition.md) | GPS acquisition (always on, last fix kept) | M3 | Implemented |
| [FRD-012](frd/FRD-012-wire-format.md) | Wire format & length budget | M2/M3 | Implemented |
| [FRD-013](frd/FRD-013-phone-emoji-rendering.md) | 📞 rendering as custom OLED glyph | M4 | Implemented |
| [FRD-014](frd/FRD-014-new-message-alert.md) | New-message alert (display wake + LED blink until acknowledged) | M4 | Implemented |
| [FRD-015](frd/FRD-015-configuration-via-app.md) | Configuration via MeshCore app (BLE always on) | M2 | Implemented |
| [FRD-016](frd/FRD-016-inbox-storage.md) | Inbox storage (RAM ring buffer, 16 msgs) | M2 | Implemented |
| [FRD-017](frd/FRD-017-ble-pairing-mode.md) | BLE pairing mode (hold 10 s, big PIN, 30 s) | M2 | Implemented |
| [FRD-018](frd/FRD-018-group-config-file.md) | Group config file `pager.ini` (channel name/key, EU radio) | M2 | Implemented |
| [FRD-019](frd/FRD-019-location-messages.md) | Location messages `Standort?` (auto-answer) / `Mein Standort` | M3 | Implemented |
| [FRD-020](frd/FRD-020-message-beep.md) | New-message beep, optional speaker on GPIO 4 (no resistor) | M4 | Implemented |
| [FRD-021](frd/FRD-021-ble-setup-yaml.md) | Setup over BLE: YAML with nickname, channel, questions + own replies | M4b | Implemented |
| [FRD-022](frd/FRD-022-emoji-glyphs.md) | Emoji on the OLED: 25 kid-friendly pictures (8×8 + 16×16) | M4c | Implemented |

## Adding an FRD
1. Copy an existing file and take the next free number: `frd/FRD-NNN-<kebab-slug>.md`.
2. Fill in the header table (Status, Milestone, PRD reference), Requirement, Acceptance criteria and optional Implementation notes.
3. Add a row to this index and reference the FRD in [ROADMAP.md](ROADMAP.md).
4. Record the decision in [DEVLOG.md](DEVLOG.md).
