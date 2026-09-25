# FRD-002 — Single-button input

| | |
|---|---|
| Status | Implemented (M1) |
| Milestone | M1 |
| PRD | §5.1, US-1, US-2, US-5 |

## Background
Only **PRG (GPIO 0)** is readable by firmware. **RST** is hard-wired to the ESP32-S3 CHIP_PU/EN pin and **cannot** be remapped in software (see PRD §5.1).

## Requirement
1. All interaction SHALL use the PRG button with three gestures:

   | Gesture | Definition | Generic meaning |
   |---|---|---|
   | **Short** | press < 1000 ms, no second press within the multi-click window | *next* |
   | **Long** | press held ≥ 1000 ms (fires once, on threshold) | *select / send / confirm* |
   | **Double** | two short presses within the multi-click window | *back / cancel* |

2. Triple-click SHALL be ignored. A **hold ≥ 10 s** is the one extra gesture (BLE pairing mode, FRD-017).
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
| BLE pairing ([FRD-017](FRD-017-ble-pairing-mode.md)) | cancel → Chat | — | cancel → Chat |

**Hold ≥ 10 s** (any screen, display on or off) enters BLE pairing mode ([FRD-017](FRD-017-ble-pairing-mode.md)). The 1 s long press fires first on the way there.

## Acceptance criteria
- Every row of the state table can be reached and behaves as specified on a real device.
- A long press on the Compose/Reply picker sends exactly one message.
- Pressing RST reboots the device (expected hardware behaviour, documented in the guide).

## Implementation notes
- Reuse the variant's `user_btn` (`variants/heltec_v4/target.cpp`: `MomentaryButton(PIN_USER_BTN, 1000, true)`). It emits `BUTTON_EVENT_CLICK`, `BUTTON_EVENT_DOUBLE_CLICK` and `BUTTON_EVENT_LONG_PRESS`.
- The multi-click window delays single-click events. It's fixed at 280 ms (`MULTI_CLICK_WINDOW_MS`) so the UI stays responsive.
