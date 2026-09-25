# FRD-011 — GPS acquisition

| | |
|---|---|
| Status | Implemented (M3) |
| Milestone | M3 |
| PRD | US-4 |

## Requirement
1. The GPS SHALL be **enabled at boot**, whatever the stored `gps_enabled` pref (the companion default is off).
2. The GPS SHALL stay powered while the device is on (needed for the ≤ 2 min freshness in [FRD-010](FRD-010-gps-attachment.md)).
3. The firmware SHALL keep the **last valid fix** (lat, lon, timestamp in `millis()`) in RAM.
4. The device clock MAY be synced from GPS time (existing companion behaviour).
5. Automatic GPS-based advert/location broadcasts SHALL stay off (`gps_interval = 0`). Position is only shared inside messages.

## Acceptance criteria
- After a cold boot outdoors, a fix is obtained and used within the module's normal TTFF (< 2 min typical).
- The last fix survives a temporary signal loss.

## Implementation notes
- The pager build forces `gps_enabled = 1`, `gps_interval = 0` in `MyMesh::begin()`. `UITask::updateFix()` polls the provider once per second.
- GPS pins/power: `PIN_GPS_EN=34` (active LOW), `PIN_GPS_RESET=42`, UART RX 38 / TX 39. Handled by `EnvironmentSensorManager` + `MicroNMEALocationProvider` (`src/helpers/sensors/`).
