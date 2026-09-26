# FRD-020 — New-message beep (optional speaker)

| | |
|---|---|
| Status | Implemented (M4) |
| Milestone | M4 |
| PRD | US-6 |

## Background
The Heltec V4 has no buzzer. A small 8–32 Ω magnetic speaker can be added with **two wires, no resistor**: speaker → **GPIO 4** (red), speaker → **GND** (black). Speaker polarity doesn't matter.

Wired straight to a GPIO, the speaker is nearly a short circuit, so the firmware protects the pin:
- **fixed drive strength** (`PAGER_BUZZER_DRIVE`; the default build uses **level 3 ≈ 40 mA**, the ESP32-S3's absolute maximum per pin, by user choice for maximum volume. It's a known wear risk for the pin, so lower it to 1–2 if the sound degrades),
- **tone only**: a square wave, never a steady HIGH through the coil,
- **short beeps**, and the pin is **left LOW** afterwards.

## Requirement
1. When a pager-channel message from **another** pager arrives (the same cases that start the LED alert, [FRD-014](FRD-014-new-message-alert.md)), the pager SHALL play **two consecutive tones** ("uh-oh" style): the first at ~3.1 kHz for 150 ms, a ~40 ms gap, then the second **lower** (~2.35 kHz) and **slightly longer** (225 ms), ≈0.4 s total, transposed to ~2.2–3.1 kHz where small speakers are loudest. It's a melody imitation, not the original voice recording (which can't be played on a square-wave speaker and is copyrighted).
2. Own messages and automatic `Mein Standort` answers SHALL NOT beep.
3. The beep SHALL NOT repeat. The LED keeps blinking until acknowledged.
4. It SHALL be optional per build: `-D PIN_BUZZER=<gpio>` (default **4** in `heltec_v4_pager`). Without the flag, no buzzer code is compiled in.
5. The drive strength SHALL be configurable: `-D PAGER_BUZZER_DRIVE=0..3` (firmware default 1; **the `heltec_v4_pager` env sets 3**). The melody can be overridden with `-D PAGER_BEEP_MELODY='"…"'` (RTTTL).

## Acceptance criteria
- A message from another pager produces a recognisable "uh-oh".
- No beep when sending, or for automatic answers.
- The GPIO is LOW when idle.

## Implementation notes
- MeshCore's `genericBuzzer` (`src/helpers/ui/buzzer.*`, NonBlockingRTTTL), melody `uhoh:d=8,o=7,b=200:g,32p,d.` (G7 · pause · dotted D7).
- `UITask::beep()` / `buzzerLoop()`: re-apply `gpio_set_drive_capability()` while a tone plays (`tone()` may re-attach the pin), then `pinMode(OUTPUT)` + LOW once it's done.
- Even louder without a transistor: push-pull on two GPIOs in opposite phase (black wire to a second GPIO instead of GND, about 4× the power). Not implemented yet.
- Louder: NPN transistor (GPIO → 1 kΩ → base, emitter → GND, collector → speaker → 3V3), then `PAGER_BUZZER_DRIVE=3`.
