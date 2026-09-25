# FRD-008 — Pager channel selection

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | §4, §8 (Pager channel) |

## Requirement
1. The pager channel SHALL be the group channel whose **name equals `PAGER_CHANNEL_NAME`** (build flag, default `"Pager"`, case-sensitive).
2. **Default channel:** at boot, if no channel with that name exists, the firmware SHALL create it in the first free slot with a **random 128-bit key** and save it. It then shows up in the MeshCore app with no manual setup. The firmware SHALL NOT hard-code a PSK.
   - To form a group, one pager's key is shared to the others via the app (QR code). See the [flashing guide §5.4](../docs/pager_flashing.md).
   - If several channels share the name, the **last** matching slot is used for sending (an imported group key lands after the auto-created one). Received messages on any of them are shown.
3. Messages on any other channel (including Public), and direct messages, SHALL NOT be shown, and SHALL NOT wake the display or trigger alerts.
4. They SHALL still be forwarded to a connected app (companion behaviour stays intact).
5. If no channel with that name exists (e.g. renamed in the app until the next reboot), the Chat screen SHALL show `No 'Pager' channel` and `Configure via MeshCore app`, and sending SHALL be disabled.
6. The channel lookup SHALL be re-evaluated whenever channels change (app edits) and at boot.

## Acceptance criteria
- Two pagers with the same channel name and PSK exchange messages. With a different PSK, nothing is shown.
- A Public-channel message produces no visible or audible reaction on the pager.

## Implementation notes
- Seeding: `MyMesh::ensurePagerChannel()` after `loadChannels()`. Lookup: `MyMesh::findPagerChannel()`.
- Channels: `BaseChatMesh::getChannel(idx, ChannelDetails&)` / `ChannelDetails.name`. Filter in `MyMesh::onChannelMessageRecv` (`examples/companion_radio/MyMesh.cpp:545`) before calling `_ui->newMsg(...)`.
- Sending: `MyMesh::sendPagerMessage(text)` → `BaseChatMesh::sendGroupMessage(ts, channel, _prefs.node_name, text, len)`.
- Direct messages are also kept off the pager display (still delivered to the app).
