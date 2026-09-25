# FRD-001 — Hardware target

| | |
|---|---|
| Status | Implemented (M1) |
| Milestone | M1 |
| PRD | §4, §5 |

## Requirement
1. The firmware SHALL be built for exactly one target: **Heltec WiFi LoRa 32 V4** with the standard **SSD1306 128×64 OLED** and the Heltec V4 GPS module.
2. A new PlatformIO environment **`heltec_v4_pager`** SHALL be added to `variants/heltec_v4/platformio.ini`. It extends `heltec_v4_oled` and uses `heltec_v4_companion_radio_ble` as its template.
3. The environment SHALL define:
   - `DISPLAY_CLASS=SSD1306Display`
   - `PAGER_CHANNEL_NAME='"Pager"'`
   - `ENV_INCLUDE_GPS=1` (inherited)
   - `BLE_PIN_CODE` (inherited behaviour)
   - `-I examples/companion_radio/ui-pager` instead of `ui-new`
4. The build output SHALL be a merged binary (`firmware-merged.bin`) suitable for a full flash at offset `0x0`.

## Acceptance criteria
- `pio run -e heltec_v4_pager` builds without errors.
- `./build.sh build-firmware heltec_v4_pager` produces `out/heltec_v4_pager-<version>-<sha>-merged.bin`.
- No other pager environments exist.

## Implementation notes
- Source filter: like `heltec_v4_companion_radio_ble`, but `+<../examples/companion_radio/ui-pager/*.cpp>` instead of `ui-new`.
- Pins come from `[Heltec_lora32_v4]` / `[heltec_v4_oled]`. Don't duplicate them.
