# FRD-015 — Configuration via MeshCore app

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | US-7, §8 (Configuration) |

## Requirement
1. All per-device configuration SHALL be done with the stock **MeshCore smartphone app over BLE**, using the unchanged companion protocol:
   - **Nickname** = node name
   - **Radio preset** and **pager channel** are **not** configured in the app. They come from `pager.ini` and are re-applied at every boot ([FRD-018](FRD-018-group-config-file.md)). App changes to them last until the next reboot. TX power remains app-configurable.
2. **BLE SHALL always be on** and advertising.
3. The BLE pairing PIN SHALL be shown on the **boot screen** (first 5 s) and in **pairing mode** (hold PRG 10 s, [FRD-017](FRD-017-ble-pairing-mode.md)). It isn't shown on the chat screen.
4. Configuration SHALL persist in flash across reboots.
5. The companion protocol SHALL stay fully functional while connected. Messages sent from the app into the pager channel show up on all pagers, and on the sending pager itself as `me: …`.
6. Messages sent **from the pager's button UI** SHALL also appear in the app connected to that pager. The companion protocol has no "device-sent" frame, so they're queued as a received channel message `<nickname>: <text>` (0 hops, SNR 0). The stock app shows them as coming from the own nickname.

## Acceptance criteria
- A freshly flashed pager can be fully configured with the app alone, with no USB/CLI needed.
- After a reboot, nickname, channel and radio settings are kept.
