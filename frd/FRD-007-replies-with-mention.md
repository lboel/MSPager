# FRD-007 — Replies with mention

| | |
|---|---|
| Status | Proposed |
| Milestone | M2 |
| PRD | US-5 |

## Requirement
1. A reply SHALL be sent to the **same pager channel** (visible to everyone), prefixed with a mention of the original sender in MeshCore app convention: `@[<nickname>] `.
   - Example: `@[Anna] Ja`
2. Replying to your own message SHALL be allowed and uses your own nickname.
3. Replies to replies are allowed. The mention always names the sender of the selected message.
4. On receive, a leading `@[<name>] ` SHALL be parsed. It's shown as `@<name>` in Chat and in full in detail.
5. If the mention names the own nickname, the message SHALL be flagged "for me" and prefixed with `»` in the chat list.

## Acceptance criteria
- A reply shows up in the stock MeshCore app as a mention of the original sender.
- A reply addressed to this pager is visibly marked.
