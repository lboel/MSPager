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
3. On first boot, the BLE pairing PIN SHALL be shown on the OLED (existing companion behaviour), keeping the display on until paired or pressed.
4. Configuration SHALL persist in flash across reboots.
5. The companion protocol SHALL stay fully functional while connected. Messages sent from the app into the pager channel show up on all pagers, and on the sending pager itself as `me: …`.

## Acceptance criteria
- A freshly flashed pager can be fully configured with the app alone, with no USB/CLI needed.
- After a reboot, nickname, channel and radio settings are kept.
