# FRD-014 — New-message alert

| | |
|---|---|
| Status | Implemented (M4) |
| Milestone | M4 |
| PRD | US-6 |

## Requirement
1. When a pager-channel message from **another** pager arrives:
   - The display SHALL turn on and show the Chat screen ([FRD-003](FRD-003-display-power.md)).
   - The onboard **LED (GPIO 35)** SHALL blink with **one uniform pattern** (100 ms on / 900 ms off) for every kind of message. There's no faster pattern for help requests or mentions (user decision).
2. The blinking SHALL continue **until the user opens the overview with a press**:
   - display off → the wake press shows the overview and stops the LED,
   - display already on showing the overview (e.g. woken by the message) → the **first press only acknowledges**: the LED stops, visible messages are marked read, and no selection or other action happens,
   - on another screen (detail, picker) → the LED stops when the user returns to the overview.
3. Own messages (including automatic `Mein Standort` answers) and messages on other channels SHALL NOT alert.
4. The pager UI SHALL own the LED. The LoRa TX-LED flash is removed from env `heltec_v4_pager` (`build_unflags = -D P_LORA_TX_LED=35`). `HeltecV4Board` guards its TX-LED code with `#ifdef P_LORA_TX_LED`.

## Acceptance criteria
- The LED blinks after reception, also while the display is off, and stops after one press that shows the overview.
- The acknowledging press doesn't select a message.
- Sending doesn't flash the LED.

## Implementation notes
- `UITask::_led_alert` / `ledLoop()`, pin `PAGER_LED_PIN=35` (build flag).
