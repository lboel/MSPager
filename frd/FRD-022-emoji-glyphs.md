# FRD-022 — Emoji on the OLED (kid-friendly pictures)

| | |
|---|---|
| Status | Verified (device test 2026-09-26) |
| Milestone | M4c |
| PRD | §7, US-11 |
| Extends | FRD-013 (📞 glyph), FRD-021 (setup texts) |

## Requirement
1. The pager SHALL draw a set of **important emoji as pictures** on the OLED, so texts like `Komm nach Hause 🏠` or a `👍` reply can be read by children who don't read (well) yet.
2. Each emoji has a **small 8×8** picture (chat list, menus, header) and a **large 16×16** picture (selected message, detail view), like 📞 before.
3. The set (25 pictures, 82 codepoints incl. aliases) is listed in [docs/pager_emoji.md](../docs/pager_emoji.md): answers (👍 👎 ✅ ❌ ❓), feelings (😀 😢 😡 😴 ❤️ 👋), places and ways (🏠 🏫 📍 🚗 🚌 🚲), needs (🍴 🥤 🚽 🩹 🆘), time/activity (⏰ ⚽) and 📞.
4. Variants SHALL show the same picture: all heart colours, similar smileys, skin tones (👍🏽), with or without U+FE0F (`❤️`).
5. Invisible parts of emoji sequences (U+FE0F/FE0E, U+200D, skin-tone modifiers) SHALL take no space. Any other emoji SHALL show as a filled block, as before.
6. Emoji travel over the air as normal UTF-8, so the MeshCore app shows the real emoji.

## Acceptance criteria
- A setup with [docs/pager_setup_kids.yaml](../docs/pager_setup_kids.yaml) shows every emoji as a picture in the send menu, in the chat list and enlarged in the selected message/detail view.
- `❤️` and `👍🏽` show one picture each, with no block or gap after them.
- The 📞 reply looks exactly like before (FRD-013).

## Implementation notes
- Pictures are ASCII art in `bin/gen_emoji_glyphs.py`, which generates `ui-pager/EmojiGlyphs.h` (bitmaps + sorted codepoint table), `docs/pager_emoji.md` and `docs/pager_emoji.png`. Adding an emoji means editing the art there, running the script and rebuilding.
- `ui-pager/PagerText.h`: UTF-8 decoding, emoji lookup (binary search), CP437 mapping, zero-width handling. Plain C++, unit tests in `test/test_pager_text`.
- Cost: about 1.5 KB flash, no RAM.
