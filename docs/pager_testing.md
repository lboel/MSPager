# MSPager — Manual Device Tests

Tick the boxes while testing on a real Heltec V4 + OLED. Report failures in [DEVLOG.md](../DEVLOG.md) with the step number.

Flashing: see [pager_flashing.md](pager_flashing.md). For M1, use `out/heltec_v4_pager-v0.1.0-m1-<sha>-merged.bin` (first install: **Erase Flash**, then flash at `0x0`).

---

## M1 — Skeleton (single device)

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
