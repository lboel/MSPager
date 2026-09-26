# FRD-020 — New-message beep (optional speaker)

| | |
|---|---|
| Status | Implemented (M4) |
| Milestone | M4 |
| PRD | US-6 |

## Background
The Heltec V4 has no buzzer. A small 8–32 Ω magnetic speaker can be added with **two wires, no resistor**: speaker → **GPIO 4** (red), speaker → **GND** (black). Speaker polarity doesn't matter.

Wired straight to a GPIO, the speaker is nearly a short circuit, so the firmware protects the pin:
- **reduced drive strength** (`GPIO_DRIVE_CAP_1`, about 10 mA, within the ESP32-S3's ~20 mA recommendation),
- **tone only**: a square wave, never a steady HIGH through the coil,
- **short beeps**, and the pin is **left LOW** afterwards.

## Requirement
1. When a pager-channel message from **another** pager arrives (the same cases that start the LED alert, [FRD-014](FRD-014-new-message-alert.md)), the pager SHALL play **one beep sequence**: 3 × 150 ms at about 3.1 kHz (G7). That's near the frequency range where small speakers and the human ear are most sensitive.
2. Own messages and automatic `Mein Standort` answers SHALL NOT beep.
3. The beep SHALL NOT repeat. The LED keeps blinking until acknowledged.
4. It SHALL be optional per build: `-D PIN_BUZZER=<gpio>` (default **4** in `heltec_v4_pager`). Without the flag, no buzzer code is compiled in.
5. The drive strength SHALL be configurable: `-D PAGER_BUZZER_DRIVE=0..3` (default **1**). Use 2–3 only with a transistor or series resistor. The melody can be overridden with `-D PAGER_BEEP_MELODY='"…"'` (RTTTL).

## Acceptance criteria
- A message from another pager produces three short beeps, audible indoors near the pager.
- No beep when sending, or for automatic answers.
- The GPIO is LOW when idle.

## Implementation notes
- MeshCore's `genericBuzzer` (`src/helpers/ui/buzzer.*`, NonBlockingRTTTL), melody `msg:d=8,o=7,b=200:g,16p,g,16p,g`.
- `UITask::beep()` / `buzzerLoop()`: re-apply `gpio_set_drive_capability()` while a tone plays (`tone()` may re-attach the pin), then `pinMode(OUTPUT)` + LOW once it's done.
- Even louder without a transistor: push-pull on two GPIOs in opposite phase (black wire to a second GPIO instead of GND, about 4× the power). Not implemented yet.
- Louder: NPN transistor (GPIO → 1 kΩ → base, emitter → GND, collector → speaker → 3V3), then `PAGER_BUZZER_DRIVE=3`.
