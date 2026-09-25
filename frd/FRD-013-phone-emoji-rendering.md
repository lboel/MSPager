# FRD-013 — Phone emoji rendering

| | |
|---|---|
| Status | Implemented (M1, early) |
| Milestone | M4 |
| PRD | §7 |

## Background
The SSD1306 driver uses the Adafruit GFX built-in 6×8 font (ASCII / CP437). It **can't** render emoji, and the stock `translateUTF8ToBlocks()` replaces multi-byte UTF-8 characters with a block. The OLED can draw arbitrary bitmaps, though, so the emoji is drawn as a small custom icon.

## Requirement
1. On the wire, the phone reply SHALL be the real emoji `📞` (U+1F4DE, UTF-8 `F0 9F 93 9E`), so stock MeshCore apps show it natively.
2. On the OLED, every occurrence of U+1F4DE SHALL be drawn as a monochrome **old-style desk phone** (handset on top, body with dial), inline with the text: 8×8 at normal size, a separately drawn 16×16 at double size. The desk-phone shape (changed in M4) is easier to recognise at this size than a lone receiver.

   ```
   8x8          16x16
    ██████          ████████████
   ██    ██        ██████████████
                  ████        ████
     ████         ████        ████
    ██████
    ██  ██             ██████
    ██████            ██    ██
                     ██ ████ ██
                    ██  █  █  ██
                   ██   ████   ██
                   ██          ██
                   ██████████████
                   ██████████████
   ```
3. Other non-ASCII characters SHALL keep the existing fallback behaviour (German umlauts: best effort, via CP437 mapping where possible).

## Acceptance criteria
- `📞` is shown as a recognisable desk-phone icon in Chat, detail and the Reply picker, at both sizes.
- The same message shows 📞 in the MeshCore smartphone app.

## Implementation notes
- The text renderer splits strings at the 4-byte sequence and calls `DisplayDriver::drawXbm(x, y, phone_icon_8x8, 8, 8)` for the icon segment. Put the icon in `ui-pager/icons.h`.
