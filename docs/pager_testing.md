# MSPager — Manual Device Tests

Tick the boxes while testing on a real Heltec V4 + OLED. Report failures in [DEVLOG.md](../DEVLOG.md) with the step number.

Flashing: see [pager_flashing.md](pager_flashing.md). For M1, use `out/heltec_v4_pager-v0.1.0-m1-<sha>-merged.bin` (first install: **Erase Flash**, then flash at `0x0`).

---

## M1 — Skeleton (single device)

> **Testing M2 or later firmware?** Run M1 too, with these changes: step 12/20 show `Sent` instead of `M1: local only`, and step 23 only works on the `Pager` channel (Public is ignored).

**What M1 can and can't do:** Sending does **not** transmit yet. A "sent" message only appears locally as `me: …` with the popup `M1: local only`. Received messages from **any** channel (Public too) are shown. Filtering to the `Pager` channel and real sending come in M2. Position shows `unknown` until M3. The LED isn't used until M4.

### Boot
- [ ] 1. After flashing and pressing RST: the OLED shows **MSPager**, the version (`v0.1.0-m1-…`), the node name and **BLE PIN: nnnnnn**.
- [ ] 2. After ~5 s it switches to the chat screen: a header with the node name and battery icon, a separator line, "No messages yet" and the hint `Hold:send  2x:off` at the bottom.
- [ ] 3. The battery icon fill looks plausible (with and without a LiPo attached).

### Display power (FRD-003)
- [ ] 4. Without touching anything, the display turns off ~15 s after the chat screen appears (~20 s after boot).
- [ ] 5. Display off → **short** press: the display comes on showing chat, and nothing else happens.
- [ ] 6. Display off → **long** press (hold ≥ 1 s): the display comes on showing chat. The compose picker does **not** open.
- [ ] 7. Display off → **double** press: the display comes on, and nothing else happens.
- [ ] 8. On chat with nothing selected → **double** press: the display turns off immediately.

### Compose & local messages (FRD-002, FRD-006)
- [ ] 9. Chat → **long** press: the "Send to group" picker shows `Angekommen?` (highlighted) and `Brauche Hilfe`.
- [ ] 10. **Short** press cycles the highlight and wraps around.
- [ ] 11. **Double** press cancels back to chat, and nothing is added.
- [ ] 12. Long press → select `Brauche Hilfe` → **long** press: the popup `M1: local only` appears, and chat shows `me: Brauche Hilfe`.
- [ ] 13. Add 7 or more messages this way. Chat shows the **latest 5**, newest at the bottom, with the hint `Tap:select  Hold:send`.

### Selection, detail, reply (FRD-002, FRD-004, FRD-005, FRD-007)
- [ ] 14. Chat → **short** press: the newest message is highlighted (enlarged, inverted), and the hint changes to `Hold:open  2x:back`.
- [ ] 15. Further **short** presses move the highlight to older messages. The list scrolls up past the top line. After the oldest, the selection clears.
- [ ] 16. With a selection → **double** press: the selection clears (the display stays on).
- [ ] 17. With a selection → **long** press: the detail screen shows the sender in the title bar, the message, the age ("just now" / "N min ago"), "Position: unknown" and "Hold:reply 2x:back".
- [ ] 18. Detail → **double** press: back to chat, with the selection kept.
- [ ] 19. Detail → **long** press: the "Reply to me" picker shows `Ja`, `Nein`, `OK` and a **phone handset icon** (FRD-013).
- [ ] 20. Pick the phone icon → **long** press: chat shows `me: @<own node name> [handset icon]`.
- [ ] 21. Reply picker → **double** press: back to detail.

### Timeout resets state
- [ ] 22. Open a picker and wait 15 s: the display turns off. The next press wakes it to **chat** with no selection, and nothing was sent.

### Receiving (optional, needs a second MeshCore device or app)
- [ ] 23. Send a message into any channel the pager knows (e.g. Public) from another MeshCore node. The display wakes and chat shows `<sender>: <text>` with a leading `•` (unread).
- [ ] 24. A short press while it's visible removes the `•`.

### Recovery
- [ ] 25. The MeshCore app still connects over BLE with the PIN from step 1 (companion protocol intact).
- [ ] 26. Pressing **RST** reboots the device (expected. RST can't be used by firmware).

---

## M2 — Messaging (two pagers, plus the MeshCore app)

Firmware: `heltec_v4_pager-v0.2.0-m2-<sha>-merged.bin`. Pagers **A** and **B**, both flashed with a full erase from the same checkout (same `pager.ini`). Position still shows `unknown` (M3). No LED yet (M4).

### Group config (FRD-008, FRD-018)
- [ ] 1. Flash A and B from the same checkout (full erase). Connect the app to each: the channel list shows `Public` **and** `Pager`, and the radio settings show 869.618 MHz / BW 62.5 / SF 8 / CR 5.
- [ ] 2. With **no app setup**, send `Angekommen?` from A. B receives it.
- [ ] 3. In the app, change A's `Pager` key (or delete the channel) and reboot A. The key from `pager.ini` is back, and A and B talk again.
- [ ] 4. In the app, change A's frequency and reboot A. It's back at 869.618 MHz.
- [ ] 4b. Upgrade test: a pager that still has an M2 random-key `Pager` channel (and maybe a QR-imported duplicate) ends up after flashing with exactly **one** `Pager` channel holding the `pager.ini` key.

### Send & receive (FRD-006, FRD-009, FRD-012)
- [ ] 5. Set node names in the app: A = `Anna`, B = `Ben`.
- [ ] 6. A: hold → `Angekommen?` → hold. A shows the popup `Sent` and `me: Angekommen?`.
- [ ] 7. B: the display wakes (even when off) and shows `•Anna: Angekommen?`.
- [ ] 8. A: send `Brauche Hilfe`. B shows `•Anna: Brauche Hilfe`.
- [ ] 9. The MeshCore app connected to B (or a phone in the `Pager` channel) shows `Anna: Angekommen?` as normal channel text.

### Replies & mentions (FRD-007)
- [ ] 10. B: select Anna's message → hold (detail) → hold (reply) → `Ja` → hold. B shows `me: @Anna Ja`.
- [ ] 11. A: shows `•»Ben: @Anna Ja` (the `»` means "for me").
- [ ] 12. Repeat with `Nein`, `OK` and the phone icon. In the MeshCore app the last one shows `@[Anna] 📞` (a real emoji).
- [ ] 13. A reply from B to **Ben's own** message arrives at A **without** `»`.

### Filtering (FRD-008)
- [ ] 14. Send a message on **Public** from the app or another node. Neither pager display reacts, but the app still receives it.
- [ ] 15. Send a **direct message** to A from the app. A's display doesn't react, and the app still receives it.
- [ ] 15b. With the app connected to A: send `Angekommen?` from **A's button UI**. The app shows it in `Pager` as `Anna: Angekommen?` (listed as coming from `Anna`, not as your own bubble).
- [ ] 16. With the app connected to A, send free text into `Pager` from the app. B shows `•Anna: <text>`, and A shows `me: <text>` (no alert).

### Large text (FRD-004, FRD-005)
- [ ] 16k. Select a message: it's shown as an inverted block with the sender small and the text at **double size**. The older message is visible above it, and newer ones below if there's room.
- [ ] 16l. `Angekommen?` fits on **one** large line. `Brauche Hilfe` wraps to two (`Brauche` / `Hilfe`).
- [ ] 16m. A selected `📞` reply shows a **large** handset icon.
- [ ] 16n. Step through all messages with short presses: the block moves, and nothing overlaps or runs off the bottom (also for the oldest and newest message).
- [ ] 16o. Open the detail view: the sender is left and the age right in the title bar, and the message is at double size.

### Button hints (FRD-004)
- [ ] 16p. Every screen shows its hint in the bottom row below a line: chat (3 variants per FRD-004), detail `Hold:reply  2x:back`, pickers `Hold:send  2x:cancel`, pairing `Tap:cancel`.
- [ ] 16q. No text overlaps the hint row (check a selected 2-line message such as `Brauche Hilfe`, and the detail view).

### Header: name + battery icon (FRD-004)
- [ ] 16b. The chat header shows only the nickname (left) and a battery icon (right). No PIN and no percentage.
- [ ] 16c. The PIN is visible on the boot screen and in pairing mode (hold 10 s).
- [ ] 16d. A 14-character name is shown in full. Longer names get cut off with `...`.

### BLE pairing mode (FRD-017)
- [ ] 16e. With the app connected: on the chat screen, **hold PRG 10 s**. The app disconnects, and the screen shows "Bluetooth pairing", the PIN in large digits, the name and a 30 s countdown.
- [ ] 16f. Connect the app (enter the PIN if asked): the popup `Connected` appears, then chat.
- [ ] 16g. Enter pairing mode, don't connect: after 30 s the popup `Pairing timeout` appears, and the app can still find and connect to the pager afterwards.
- [ ] 16h. Enter pairing mode, short press: cancelled, back to chat.
- [ ] 16i. Display off → hold 10 s: the display wakes at 1 s and pairing mode opens at 10 s.
- [ ] 16j. Reproduce the bug: the pager is invisible in the app scan after a while → hold 10 s → it appears in the scan.

### Robustness
- [ ] 17. In the app, rename A's `Pager` channel to something else. A's chat (with no messages) shows `No 'Pager' channel`, and hold → popup `No 'Pager' channel`, nothing sent.
- [ ] 18. Reboot A: `Pager` is back with the `pager.ini` key (the renamed copy stays as an extra channel, which the pager ignores).
- [ ] 19. Detail → **long** press: the "Reply to me" picker shows `Ja`, `Nein`, `OK` and a **phone handset icon** (FRD-013).
- [ ] 20. Pick the phone icon → **long** press: chat shows `me: @<own node name> [handset icon]`.
- [ ] 21. Reply picker → **double** press: back to detail.

### Timeout resets state
- [ ] 22. Open a picker and wait 15 s: the display turns off. The next press wakes it to **chat** with no selection, and nothing was sent.

### Receiving (optional, needs a second MeshCore device or app)
- [ ] 23. Send a message into any channel the pager knows (e.g. Public) from another MeshCore node. The display wakes and chat shows `<sender>: <text>` with a leading `•` (unread).
- [ ] 24. A short press while it's visible removes the `•`.

### Recovery
- [ ] 25. The MeshCore app still connects over BLE with the PIN from step 1 (companion protocol intact).
- [ ] 26. Pressing **RST** reboots the device (expected. RST can't be used by firmware).

---

## M2 — Messaging (two pagers, plus the MeshCore app)

Firmware: `heltec_v4_pager-v0.2.0-m2-<sha>-merged.bin`. Pagers **A** and **B**, both flashed with a full erase from the same checkout (same `pager.ini`). Position still shows `unknown` (M3). No LED yet (M4).

### Group config (FRD-008, FRD-018)
- [ ] 1. Flash A and B from the same checkout (full erase). Connect the app to each: the channel list shows `Public` **and** `Pager`, and the radio settings show 869.618 MHz / BW 62.5 / SF 8 / CR 5.
- [ ] 2. With **no app setup**, send `Angekommen?` from A. B receives it.
- [ ] 3. In the app, change A's `Pager` key (or delete the channel) and reboot A. The key from `pager.ini` is back, and A and B talk again.
- [ ] 4. In the app, change A's frequency and reboot A. It's back at 869.618 MHz.
- [ ] 4b. Upgrade test: a pager that still has an M2 random-key `Pager` channel (and maybe a QR-imported duplicate) ends up after flashing with exactly **one** `Pager` channel holding the `pager.ini` key.

### Send & receive (FRD-006, FRD-009, FRD-012)
- [ ] 5. Set node names in the app: A = `Anna`, B = `Ben`.
- [ ] 6. A: hold → `Angekommen?` → hold. A shows the popup `Sent` and `me: Angekommen?`.
- [ ] 7. B: the display wakes (even when off) and shows `•Anna: Angekommen?`.
- [ ] 8. A: send `Brauche Hilfe`. B shows `•Anna: Brauche Hilfe`.
- [ ] 9. The MeshCore app connected to B (or a phone in the `Pager` channel) shows `Anna: Angekommen?` as normal channel text.

### Replies & mentions (FRD-007)
- [ ] 10. B: select Anna's message → hold (detail) → hold (reply) → `Ja` → hold. B shows `me: @Anna Ja`.
- [ ] 11. A: shows `•»Ben: @Anna Ja` (the `»` means "for me").
- [ ] 12. Repeat with `Nein`, `OK` and the phone icon. In the MeshCore app the last one shows `@[Anna] 📞` (a real emoji).
- [ ] 13. A reply from B to **Ben's own** message arrives at A **without** `»`.

### Filtering (FRD-008)
- [ ] 14. Send a message on **Public** from the app or another node. Neither pager display reacts, but the app still receives it.
- [ ] 15. Send a **direct message** to A from the app. A's display doesn't react, and the app still receives it.
- [ ] 15b. With the app connected to A: send `Angekommen?` from **A's button UI**. The app shows it in `Pager` as `Anna: Angekommen?` (listed as coming from `Anna`, not as your own bubble).
- [ ] 16. With the app connected to A, send free text into `Pager` from the app. B shows `•Anna: <text>`, and A shows `me: <text>` (no alert).

### Header: name + PIN + battery (FRD-004)
- [ ] 16b. The chat header shows the nickname (left), the BLE PIN (right-aligned, same as the boot screen) and the battery percentage (e.g. `87%`), all on one line with no overlap. The PIN doesn't shift between `100%` and `9%`.
- [ ] 16c. It stays like that while and after the app is connected.
- [ ] 16d. Set a 10-character name (e.g. `Alexandria`): it's shown in full. With 12 characters it gets cut off with `...`.

### BLE pairing mode (FRD-017)
- [ ] 16e. With the app connected: on the chat screen, **hold PRG 10 s**. The app disconnects, and the screen shows "Bluetooth pairing", the PIN in large digits, the name and a 30 s countdown.
- [ ] 16f. Connect the app (enter the PIN if asked): the popup `Connected` appears, then chat.
- [ ] 16g. Enter pairing mode, don't connect: after 30 s the popup `Pairing timeout` appears, and the app can still find and connect to the pager afterwards.
- [ ] 16h. Enter pairing mode, short press: cancelled, back to chat.
- [ ] 16i. Display off → hold 10 s: the display wakes at 1 s and pairing mode opens at 10 s.
- [ ] 16j. Reproduce the bug: the pager is invisible in the app scan after a while → hold 10 s → it appears in the scan.

### Robustness
- [ ] 17. In the app, rename A's `Pager` channel to something else. A's chat (with no messages) shows `No 'Pager' channel`, and hold → popup `No 'Pager' channel`, nothing sent.
- [ ] 18. Reboot A: a new `Pager` channel with a **new** key is created (share the group key again per §5.4).
- [ ] 19. Send 20 messages. Only the last 16 are browsable (FRD-016).

---

## M3 — Location (two or three pagers, outdoors)

Pagers **A**, **B** (and **C** for the group test), the same `pager.ini`, different nicknames, GPS modules attached. For the first fix, go outdoors with a clear view of the sky for 2–5 min.

### GPS & suffix (FRD-010, FRD-011, FRD-012)
- [ ] 0a. Indoors after boot: the header shows the **slashed pin** left of the battery.
- [ ] 0b. Outdoors: once the fix is there, it turns into a **filled pin** (within ~1 s). Back indoors, it's slashed again within ~5 s of losing the fix.
- [ ] 1. Before any fix (indoors, fresh boot): send `Angekommen?` from A. The app connected to B shows `A: Angekommen? [no GPS]`, and B's detail shows `no GPS` / `no GPS fix`.
- [ ] 2. Outdoors, after a fix: send again. The app shows `[lat,lon]` with **exactly 4 decimals**, and B's detail shows the same coordinates.
- [ ] 3. Take A indoors until the fix is lost (> 2 min), then send. The suffix carries `~Nmin`, and B's relative position starts with `~`.
- [ ] 4. The chat list never shows the `[...]` suffix, only in the app and as coordinates in detail.

### Detail view (FRD-005)
- [ ] 5. Pagers ~50–200 m apart, both with a fix. B opens A's message: the title shows `A  NNNm <dir>` with plausible distance (±20 m) and direction (±1 sector, check with a phone map). The row below shows the coordinates + message age.
- [ ] 6. Opening your own message shows `you` in the title.
- [ ] 7. B without a fix: the title shows `no own GPS`, and the coordinates are still shown.

### Location messages (FRD-019)
- [ ] 8. Compose picker: 4 options, and the title shows `1/4`. Reply picker: 6 options, scrolls after the 4th, title `n/6`.
- [ ] 9. A: `Mein Standort` → hold. B's chat shows `A: Mein Standort 340m SW` (distance/direction appended). When selected, the large block shows it too.
- [ ] 10. A: `Standort?` to the group. Within ~4 s, B **and** C answer automatically. A shows `B: @A Mein Standort …` and `C: @A Mein Standort …`.
- [ ] 11. On B (and C), the answer is in the own chat as `me: @A Mein Standort`, and the popup `Location shared` appears if the display is on.
- [ ] 12. A: select one of B's messages → reply `Standort?`. **Only B** answers, and C stays silent.
- [ ] 13. `Mein Standort` never triggers an answer (no loop). Watch for 30 s after step 10.
- [ ] 14. Free text from the app without a suffix shows `no position` in detail and doesn't break anything.
