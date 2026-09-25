# FRD-004 — Main screen (Chat)

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M1 (layout), M2 (data) |
| PRD | US-1, US-3 |

## Requirement
1. The main screen SHALL show, **at the same time**:
   - a **header row** with the pager's **nickname** (left), a **GPS status icon** (left of the battery: filled location pin = current fix, hollow slashed pin = no fix; a fix counts as current if the last valid GPS reading is < 5 s old) and a **battery icon** (right, fill level = charge, no percentage text). The BLE PIN is **not** shown here, only on the boot screen and in pairing mode ([FRD-017](FRD-017-ble-pairing-mode.md)), and
   - the **chat room**: the latest pager-channel messages, newest at the bottom.
2. No other status information (GPS, BLE state, unread count) SHALL be shown on this screen.
   - Header budget (128 px): battery icon ≈ 23 px, GPS icon 8 px + gaps, so the name gets **14 characters**. Longer names are ellipsized.
3. Each message SHALL take one line in the form `<sender>: <text>`:
   - `<sender>` is the nickname ([FRD-009](FRD-009-sender-nickname.md)). Own messages show the sender as `me`.
   - `<text>` is the canned message without the position suffix. Mentions are shown compactly as `@Anna Ja`.
   - 📞 is rendered as a glyph ([FRD-013](FRD-013-phone-emoji-rendering.md)).
   - Lines that are too long SHALL be ellipsized.
4. Unread messages SHALL be marked with a leading `•` (or an inverted sender name).
5. **Accessibility:** the selected message (see [FRD-002](FRD-002-single-button-input.md)) SHALL be shown **enlarged** as an inverted block:
   - the sender at normal size (with `»` if it's for me), then
   - the message body at **double size** on **one single line** (11 columns). Longer text **scrolls slowly horizontally** (marquee: 1.5 s pause, ~20 px/s, 1.5 s pause at the end, restart).
   - Around it: always one older message as a normal line above (context) and one newer line below (above the hint row).
6. With no messages, the screen SHALL show "No messages yet".
7. **Button hint** at the bottom (separator line + one text row, same place and style as on all other screens):

   | State | Hint |
   |---|---|
   | no messages | `2x:off hold:send` |
   | no pager channel | `2x:off` |
   | messages, nothing selected | `1x:↑ 2x:off hold:send` |
   | message selected | `1x:↑ 2x:← hold:open` |

   Other screens: detail `2x:← hold:reply`, pickers `1x:↓ 2x:← hold:send`, pairing `1x:cancel`.

   Arrows show the direction a gesture moves: **↑** = select the next older message (the list grows upward), **↓** = next option in a picker, **←** = back/cancel. They're CP437 characters 0x18/0x19/0x1B from the display font. Format: `1x:<action> 2x:<action> hold:<action>`, always in that order, showing **only the gestures that do something** on that screen (e.g. the detail view has no `1x`). Single spaces. The longest hint (`1x:↑ 2x:off hold:send`) is exactly 21 columns.

## Layout (128×64, 6×8 font = 21 columns × 8 rows)
```
row 0 |Anna            ⌖ [▮▮▮ ]|   header: nickname, GPS status, battery icon
row 1 |────────────────────────|   separator line
rows 2–6 (5 message lines, oldest at top, newest at bottom), then the hint row:
      |Anna: Angekommen?       |
      |me: @Anna Ja            |
      |Ben: Brauche Hilfe      |
      |•Cleo: @Ben [☎]         |
|────────────────────────|
|1x:↑ 2x:off hold:send   |
```

### Selected layout
```
|Anna            [▮▮▮ ]|   header
|Ben: @Anna Ja         |   older message (context)
|██ Cleo ██████████████|   inverted block: sender (small)
|██ Brauche Hil ███████|   body at double size, one line, scrolls if long
|Anna: Ja              |   newer message
```
Size 2 uses an 11 px character step (10 px glyph + 1 px gap) instead of GFX's 12 px, so 11 characters fit and `Angekommen?` stays on one line.

## Acceptance criteria
- Battery and at least 5 recent messages are visible together, plus the button hint.
- A new message appears at the bottom within 1 s of reception.
- The layout never overflows horizontally.

## Implementation notes
- Battery: `AbstractUITask::getBattMilliVolts()`, converted to percent as in `ui-new`.
- Header: `UITask::renderHeader()`.
- `DisplayDriver::drawTextEllipsized()`.
