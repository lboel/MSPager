# FRD-015 — Configuration via MeshCore app

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | US-7, §8 (Configuration) |

## Requirement
1. All per-device configuration SHALL be done with the stock **MeshCore smartphone app over BLE**, using the unchanged companion protocol:
   - **Nickname** = node name
   - **Radio preset** (frequency, BW, SF, CR, TX power), identical on all pagers
   - **Pager channel**: pre-created by the firmware with a random key ([FRD-008](FRD-008-pager-channel-selection.md)). The group shares one pager's key to the others by QR code
2. **BLE SHALL always be on** and advertising.
3. The BLE pairing PIN SHALL be shown on the boot screen and **always** in the chat header, next to the battery ([FRD-004](FRD-004-main-screen.md)).
4. Configuration SHALL persist in flash across reboots.
5. The companion protocol SHALL stay fully functional while connected. Messages sent from the app into the pager channel show up on all pagers, and on the sending pager itself as `me: …`.
6. Messages sent **from the pager's button UI** SHALL also appear in the app connected to that pager. The companion protocol has no "device-sent" frame, so they're queued as a received channel message `<nickname>: <text>` (0 hops, SNR 0). The stock app shows them as coming from the own nickname.

## Acceptance criteria
- A freshly flashed pager can be fully configured with the app alone, with no USB/CLI needed.
- After a reboot, nickname, channel and radio settings are kept.
