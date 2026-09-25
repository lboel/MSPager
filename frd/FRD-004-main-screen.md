# FRD-004 — Main screen (Chat)

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M1 (layout), M2 (data) |
| PRD | US-1, US-3 |

## Requirement
1. The main screen SHALL show, **at the same time**:
   - a **header row** with the pager's **nickname** (left), and a **battery icon** (right, fill level = charge, no percentage text). The BLE PIN is **not** shown here, only on the boot screen and in pairing mode ([FRD-017](FRD-017-ble-pairing-mode.md)), and
   - the **chat room**: the latest pager-channel messages, newest at the bottom.
2. No other status information (GPS, BLE state, unread count) SHALL be shown on this screen.
   - Header budget (128 px): battery icon ≈ 23 px + 1 column gap, so the name gets **16 characters**. Longer names are ellipsized.
3. Each message SHALL take one line in the form `<sender>: <text>`:
   - `<sender>` is the nickname ([FRD-009](FRD-009-sender-nickname.md)). Own messages show the sender as `me`.
   - `<text>` is the canned message without the position suffix. Mentions are shown compactly as `@Anna Ja`.
   - 📞 is rendered as a glyph ([FRD-013](FRD-013-phone-emoji-rendering.md)).
   - Lines that are too long SHALL be ellipsized.
4. Unread messages SHALL be marked with a leading `•` (or an inverted sender name).
5. **Accessibility:** the selected message (see [FRD-002](FRD-002-single-button-input.md)) SHALL be shown **enlarged** as an inverted block:
   - the sender at normal size (with `»` if it's for me), then
   - the message body at **double size** (11 columns, word-wrapped, at most 2 lines, then ellipsized).
   - Around it: one older message as a normal line above (context) and as many newer lines below as fit.
6. With no messages, the screen SHALL show "No messages yet" and "Hold: send".

## Layout (128×64, 6×8 font = 21 columns × 8 rows)
```
row 0 |Anna              [▮▮▮ ]|   header: nickname, battery icon
row 1 |────────────────────────|   separator line
rows 2–7 (6 message lines, oldest at top, newest at bottom):
      |Anna: Angekommen?       |
      |me: @Anna Ja            |
      |Ben: Brauche Hilfe      |
      |•Cleo: @Ben [☎]         |
```

### Selected layout
```
|Anna            [▮▮▮ ]|   header
|Ben: @Anna Ja         |   older message (context)
|██ Cleo ██████████████|   inverted block: sender (small)
|██ Brauche ███████████|   body at double size
|██ Hilfe ████████████ |   (2nd line if needed)
```
Size 2 uses an 11 px character step (10 px glyph + 1 px gap) instead of GFX's 12 px, so 11 characters fit and `Angekommen?` stays on one line.

## Acceptance criteria
- Battery and at least 6 recent messages are visible together.
- A new message appears at the bottom within 1 s of reception.
- The layout never overflows horizontally.

## Implementation notes
- Battery: `AbstractUITask::getBattMilliVolts()`, converted to percent as in `ui-new`.
- Header: `UITask::renderHeader()`.
- `DisplayDriver::drawTextEllipsized()`.
