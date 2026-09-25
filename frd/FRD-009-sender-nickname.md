# FRD-009 — Sender nickname

| | |
|---|---|
| Status | Proposed |
| Milestone | M2 |
| PRD | US-3 |

## Requirement
1. Each pager's **nickname** SHALL be its MeshCore **node name** (`NodePrefs.node_name`), set with the MeshCore app.
2. Outgoing messages carry the nickname through the standard MeshCore group-text prefix `<nickname>: ` (added by `sendGroupMessage`).
3. On receive, the sender SHALL be parsed as everything before the first `": "`. Without that separator, the sender is shown as `?`.
4. The nickname SHALL be shown wherever a message is shown (Chat, detail, reply target).
5. The recommended nickname length is **≤ 10 characters** (display width). Longer names are ellipsized, not rejected (maximum 31 bytes, `node_name[32]`).

## Acceptance criteria
- Changing the node name in the app changes the sender shown on other pagers for new messages.
