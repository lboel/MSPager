# FRD-019 — Location messages (`Standort?` / `Mein Standort`)

| | |
|---|---|
| Status | Implemented (M3) |
| Milestone | M3 |
| PRD | §7, US-4 |

## Requirement
1. Two special canned messages SHALL be available in **both** pickers (compose and reply):
   - **`Mein Standort`** (share location): an ordinary message whose point is the attached position. On receiving pagers, the chat line and the enlarged selection SHALL show it with **distance and direction**, e.g. `Anna: Mein Standort 1.2km NE`.
   - **`Standort?`** (location request): asks for the position of other pagers.
2. **Automatic answer:** a pager receiving `Standort?` SHALL answer with `@[<requester>] Mein Standort` plus its position suffix, without user interaction:
   - `Standort?` to the group (no mention): **every** other pager answers.
   - `@[Anna] Standort?` (sent as a reply): **only Anna** answers. Requests addressed to someone else are ignored.
   - The answer is sent after a **random 0.5–4 s delay**, so several answering pagers don't collide on air.
   - `Mein Standort` never triggers an answer (no loops). Own messages are never answered.
3. **Transparency:** the automatic answer SHALL appear in the answering pager's own chat history as `me: @<requester> Mein Standort`, with the popup `Location shared`.
4. Relative and absolute position are shown in the detail view like for any other message ([FRD-005](FRD-005-message-detail.md)).

## Acceptance criteria
- A sends `Standort?`. B and C answer within ~4 s. A shows `B: @A Mein Standort 1.2km NE` etc., and B and C show the answer as `me: …`.
- A sends `@[B] Standort?` as a reply: only B answers.

## Implementation notes
- `UITask::checkLocationRequest()` schedules `_auto_reply_at`. `loop()` sends via `sendCanned(LOC_SHARE, requester, "Location shared")`.
- The reply picker now has 6 options: it shows 4 rows, scrolls, and has an `n/6` counter in the title bar.
