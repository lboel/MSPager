# FRD-008 — Pager channel selection

| | |
|---|---|
| Status | Proposed |
| Milestone | M2 |
| PRD | §4, §8 (Pager channel) |

## Requirement
1. The pager channel SHALL be the group channel whose **name equals `PAGER_CHANNEL_NAME`** (build flag, default `"Pager"`, case-sensitive).
2. The channel and its PSK SHALL be created and edited with the MeshCore app ([FRD-015](FRD-015-configuration-via-app.md)). The firmware SHALL NOT hard-code the PSK.
3. Messages on any other channel (including Public), and direct messages, SHALL NOT be shown, and SHALL NOT wake the display or trigger alerts.
4. They SHALL still be forwarded to a connected app (companion behaviour stays intact).
5. If no channel with that name exists, the Chat screen SHALL show `No 'Pager' channel` and `Configure via MeshCore app`, and sending SHALL be disabled.
6. The channel lookup SHALL be re-evaluated whenever channels change (app edits) and at boot.

## Acceptance criteria
- Two pagers with the same channel name and PSK exchange messages. With a different PSK, nothing is shown.
- A Public-channel message produces no visible or audible reaction on the pager.

## Implementation notes
- Channels: `BaseChatMesh::getChannel(idx, ChannelDetails&)` / `ChannelDetails.name`. Filter in `MyMesh::onChannelMessageRecv` (`examples/companion_radio/MyMesh.cpp:545`) before calling `_ui->newMsg(...)`.
- Sending: `BaseChatMesh::sendGroupMessage(ts, channel, _prefs.node_name, text, len)`.
