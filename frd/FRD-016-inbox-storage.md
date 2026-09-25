# FRD-016 — Inbox storage

| | |
|---|---|
| Status | Proposed |
| Milestone | M2 |
| PRD | §9 (Robustness) |

## Requirement
1. The pager SHALL keep the last **16** pager-channel messages (sent and received) in a RAM ring buffer. The oldest message is dropped first.
2. Each entry SHALL store: sender (≤ 31 B), mention target (optional), body, parsed position + age (optional), local receive time, flags (own, unread, for-me).
3. The inbox SHALL NOT be persisted. It's empty after a reboot.
4. Duplicate packets (same packet hash, e.g. via multiple repeaters) SHALL be stored only once (MeshCore's built-in duplicate filter covers this).

## Acceptance criteria
- After 20 messages, the 16 most recent are browsable, in order.
