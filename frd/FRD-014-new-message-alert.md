# FRD-014 — New-message alert

| | |
|---|---|
| Status | Proposed |
| Milestone | M4 |
| PRD | US-6 |

## Requirement
1. When a pager-channel message from **another** pager arrives:
   - The display SHALL turn on and show the Chat screen ([FRD-003](FRD-003-display-power.md)).
   - The onboard **LED (GPIO 35)** SHALL blink (e.g. 100 ms on / 900 ms off) **until the message is read**.
2. A message counts as **read** when the user presses the button while it's visible on the Chat screen, or opens its detail view.
3. Messages with a mention of the own nickname ([FRD-007](FRD-007-replies-with-mention.md)) and `Brauche Hilfe` SHOULD use a faster blink pattern (100 ms on / 200 ms off).
4. Own messages and messages on other channels SHALL NOT alert.
5. The pager UI SHALL own the LED. The LoRa TX-LED blink (`P_LORA_TX_LED=35`) SHALL be disabled in the `heltec_v4_pager` env.

## Acceptance criteria
- The LED blinks after reception, while the display is off too, and stops once read.
- "Brauche Hilfe" blinks visibly faster.
