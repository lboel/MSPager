# FRD-008 — Pager channel selection

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | §4, §8 (Pager channel) |

## Requirement
1. The pager channel SHALL be the group channel whose **name equals `PAGER_CHANNEL_NAME`** (build flag, default `"Pager"`, case-sensitive).
2. **Channel name and key come from the group config file `pager.ini`** ([FRD-018](FRD-018-group-config-file.md)). They're enforced at every boot: exactly one channel with that name, holding the configured key, visible in the MeshCore app. All pagers built from the same config meet in the same channel with no app setup.
   - *History:* M2 first used a random key per device, shared by QR. That was replaced after test feedback because pagers didn't hear each other until keys were shared manually.
3. Messages on any other channel (including Public), and direct messages, SHALL NOT be shown, and SHALL NOT wake the display or trigger alerts.
4. They SHALL still be forwarded to a connected app (companion behaviour stays intact).
5. If no channel with that name exists (e.g. renamed in the app, until the next reboot), the Chat screen SHALL show `No 'Pager' channel` and `Configure via MeshCore app`, and sending SHALL be disabled.
6. The channel lookup SHALL be re-evaluated whenever channels change (app edits) and at boot.

## Acceptance criteria
- Two pagers with the same channel name and PSK exchange messages. With a different PSK, nothing is shown.
- A Public-channel message produces no visible or audible reaction on the pager.

## Implementation notes
- Enforcement: `MyMesh::ensurePagerChannel()` after `loadChannels()`. Lookup: `MyMesh::findPagerChannel()`.
- Channels: `BaseChatMesh::getChannel(idx, ChannelDetails&)` / `ChannelDetails.name`. Filter in `MyMesh::onChannelMessageRecv` (`examples/companion_radio/MyMesh.cpp:545`) before calling `_ui->newMsg(...)`.
- Sending: `MyMesh::sendPagerMessage(text)` → `BaseChatMesh::sendGroupMessage(ts, channel, _prefs.node_name, text, len)`.
- Direct messages are also kept off the pager display (still delivered to the app).
