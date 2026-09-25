# FRD-003 — Display power

| | |
|---|---|
| Status | Implemented (M1) |
| Milestone | M1 |
| PRD | §8 (Display), US-1, US-6 |

## Requirement
1. When idle, the display SHALL be **off**.
2. A button press while the display is off SHALL turn it on and show the **Chat** screen ([FRD-004](FRD-004-main-screen.md)). That press is **consumed** and triggers no other action.
3. A received pager-channel message SHALL turn the display on and show the Chat screen ([FRD-014](FRD-014-new-message-alert.md)).
4. The display SHALL turn off after `PAGER_DISPLAY_TIMEOUT_SECS` (default **15 s**) without a button press. Any press restarts the timer.
5. When the display turns off, the UI SHALL return to the Chat screen with no selection, and any open picker SHALL be cancelled without sending.
6. A double press on the Chat screen with no selection SHALL turn the display off immediately.

## Acceptance criteria
- The display is dark 15 s after the last interaction.
- From dark, one short press shows Chat and nothing else happens.
- From dark, one long press shows Chat and **no** Compose picker opens and nothing is sent.
- An incoming message lights up the display without a press.

## Implementation notes
- `DisplayDriver::turnOn()/turnOff()/isOn()`. Pattern from `UITask::checkDisplayOn()` in `examples/companion_radio/ui-new/UITask.cpp`.
- Suppress the gesture that caused the wake-up, including the delayed long-press event.
