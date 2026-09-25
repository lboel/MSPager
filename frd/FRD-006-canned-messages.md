# FRD-006 — Canned messages

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | §7, US-2, US-5 |

## Requirement
1. Users SHALL NOT be able to enter free text on the device.
2. **Compose picker** (starting a conversation) SHALL offer exactly, in this order:
   1. `Angekommen?`
   2. `Brauche Hilfe`  *(a statement, no question mark)*
   3. `Standort?`  *(location request, [FRD-019](FRD-019-location-messages.md))*
   4. `Mein Standort`  *(share location, FRD-019)*
3. **Reply picker** (answering a selected message) SHALL offer exactly, in this order:
   1. `Ja`
   2. `Nein`
   3. `OK`
   4. `📞` (UTF-8 `F0 9F 93 9E`, U+1F4DE TELEPHONE RECEIVER)
   5. `Standort?`
   6. `Mein Standort`
4. The catalogue SHALL be defined in one place in the source (a const array per picker), so it can be changed at compile time.
5. The picker SHALL show up to 4 options at a time (scrolling with the selection) with the current one highlighted, an `n/N` counter in the title bar, and the hint `1x:↓ 2x:← hold:send`.
6. After sending, the UI SHALL show `Sent` for about 1 s, then the Chat screen with the own message appended.

## Acceptance criteria
- Only the listed texts can be sent from the device.
- The texts arrive byte-exact (plus nickname prefix, optional mention and position suffix per [FRD-012](FRD-012-wire-format.md)).
