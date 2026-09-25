# MSPager — Manual Device Tests

Tick the boxes while testing on a real Heltec V4 + OLED. Report failures in [DEVLOG.md](../DEVLOG.md) with the step number.

Flashing: see [pager_flashing.md](pager_flashing.md). For M1, use `out/heltec_v4_pager-v0.1.0-m1-<sha>-merged.bin` (first install: **Erase Flash**, then flash at `0x0`).

---

## M1 — Skeleton (single device)

> **Testing M2 or later firmware?** Run M1 too, with these changes: step 12/20 show `Sent` instead of `M1: local only`, and step 23 only works on the `Pager` channel (Public is ignored).

**What M1 can and can't do:** Sending does **not** transmit yet. A "sent" message only appears locally as `me: …` with the popup `M1: local only`. Received messages from **any** channel (Public too) are shown. Filtering to the `Pager` channel and real sending come in M2. Position shows `unknown` until M3. The LED isn't used until M4.

### Boot
- [ ] 1. After flashing and pressing RST: the OLED shows **MSPager**, the version (`v0.1.0-m1-…`), the node name and **BLE PIN: nnnnnn**.
- [ ] 2. After ~5 s it switches to the chat screen: battery icon + percentage top right, a separator line, and "No messages yet / Hold: send".
- [ ] 3. The battery percentage looks plausible (with and without a LiPo attached).

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
- [ ] 13. Add 7 or more messages this way. Chat shows the **latest 6**, newest at the bottom.

### Selection, detail, reply (FRD-002, FRD-004, FRD-005, FRD-007)
- [ ] 14. Chat → **short** press: the newest line is highlighted (inverted).
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

Firmware: `heltec_v4_pager-v0.2.0-m2-<sha>-merged.bin`. Pagers **A** and **B**, both flashed with a full erase, same radio preset (flashing guide §5.3). Position still shows `unknown` (M3). No LED yet (M4).

### Default channel (FRD-008, FRD-015)
- [ ] 1. Fresh pager, connect the app: the channel list shows `Public` **and** `Pager` without adding anything.
- [ ] 2. Before sharing keys: send `Angekommen?` from A. B shows **nothing** (different random keys).
- [ ] 3. Share A's `Pager` QR to B per flashing guide §5.4 (delete B's own `Pager` first). Reboot B: the channel list still shows exactly one `Pager`, with A's key.
- [ ] 4. Variant: on a third pager (or B after a full erase), import A's QR **without** deleting the auto-created channel. Two `Pager` channels are listed, and messages still go to A (last slot wins).

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
- [ ] 16. With the app connected to A, send free text into `Pager` from the app. B shows `•Anna: <text>`, and A shows `me: <text>` (no alert).

### Robustness
- [ ] 17. In the app, rename A's `Pager` channel to something else. A's chat (with no messages) shows `No 'Pager' channel`, and hold → popup `No 'Pager' channel`, nothing sent.
- [ ] 18. Reboot A: a new `Pager` channel with a **new** key is created (share the group key again per §5.4).
- [ ] 19. Send 20 messages. Only the last 16 are browsable (FRD-016).
