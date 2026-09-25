# FRD-002 — Single-button input

| | |
|---|---|
| Status | Proposed |
| Milestone | M1 |
| PRD | §5.1, US-1, US-2, US-5 |

## Background
Only **PRG (GPIO 0)** is readable by firmware. **RST** is hard-wired to the ESP32-S3 CHIP_PU/EN pin and **cannot** be remapped in software (see PRD §5.1).

## Requirement
1. All interaction SHALL use the PRG button with three gestures:

   | Gesture | Definition | Generic meaning |
   |---|---|---|
   | **Short** | press < 800 ms, no second press within the multi-click window | *next* |
   | **Long** | press held ≥ 800 ms (fires once, on threshold) | *select / send / confirm* |
   | **Double** | two short presses within the multi-click window | *back / cancel* |

2. Triple-click and other gestures SHALL be ignored.
3. The first press while the display is off only wakes it ([FRD-003](FRD-003-display-power.md)). A long press that wakes the display SHALL NOT trigger a send.
4. The screen-specific mapping is defined in the state table below.

## State table

| Screen | Short | Long | Double |
|---|---|---|---|
| *Display off* | wake → Chat | wake → Chat | wake → Chat |
| Chat (nothing selected) | select newest message | open **Compose** picker | display off |
| Chat (message selected) | select next older message (after the oldest: deselect) | open **Message detail** | deselect |
| Message detail | — | open **Reply** picker for this message | back to Chat |
| Compose picker | next option (wraps) | **send** selected option → Chat | cancel → Chat |
| Reply picker | next option (wraps) | **send** reply → Chat | cancel → Message detail |

## Acceptance criteria
- Every row of the state table can be reached and behaves as specified on a real device.
- A long press on the Compose/Reply picker sends exactly one message.
- Pressing RST reboots the device (expected hardware behaviour, documented in the guide).

## Implementation notes
- Reuse `MomentaryButton` (`src/helpers/ui/MomentaryButton.h`): `MomentaryButton(PIN_USER_BTN, 800, true, false, true)`. It emits `BUTTON_EVENT_CLICK`, `BUTTON_EVENT_DOUBLE_CLICK` and `BUTTON_EVENT_LONG_PRESS`.
- The multi-click window delays single-click events. Keep it short (~300 ms) so the UI stays responsive.
