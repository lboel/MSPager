# FRD-004 — Main screen (Chat)

| | |
|---|---|
| Status | Implemented (M1, layout + local data) |
| Milestone | M1 (layout), M2 (data) |
| PRD | US-1, US-3 |

## Requirement
1. The main screen SHALL show, **at the same time**:
   - a **battery indicator** (icon + percentage) in the header row, right-aligned, and
   - the **chat room**: the latest pager-channel messages, newest at the bottom.
2. No other status information (GPS, BLE, node name, unread count) SHALL be shown on this screen.
3. Each message SHALL take one line in the form `<sender>: <text>`:
   - `<sender>` is the nickname ([FRD-009](FRD-009-sender-nickname.md)). Own messages show the sender as `me`.
   - `<text>` is the canned message without the position suffix. Mentions are shown compactly as `@Anna Ja`.
   - 📞 is rendered as a glyph ([FRD-013](FRD-013-phone-emoji-rendering.md)).
   - Lines that are too long SHALL be ellipsized.
4. Unread messages SHALL be marked with a leading `•` (or an inverted sender name).
5. The selected message (see [FRD-002](FRD-002-single-button-input.md)) SHALL be shown inverted. The list scrolls so the selection stays visible.
6. With no messages, the screen SHALL show "No messages yet" and "Hold: send".

## Layout (128×64, 6×8 font = 21 columns × 8 rows)
```
row 0 |                 ▮▮▮ 87%|   header: battery only
row 1 |────────────────────────|   separator line
rows 2–7 (6 message lines, oldest at top, newest at bottom):
      |Anna: Angekommen?       |
      |me: @Anna Ja            |
      |Ben: Brauche Hilfe      |
      |•Cleo: @Ben [☎]         |
```

## Acceptance criteria
- Battery and at least 6 recent messages are visible together.
- A new message appears at the bottom within 1 s of reception.
- The layout never overflows horizontally.

## Implementation notes
- Battery: `AbstractUITask::getBattMilliVolts()`, converted to percent as in `ui-new`.
- `DisplayDriver::drawTextEllipsized()`.
