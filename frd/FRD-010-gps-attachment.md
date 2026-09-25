# FRD-010 — GPS attachment

| | |
|---|---|
| Status | Implemented (M3) |
| Milestone | M3 |
| PRD | §8 (Position precision, No GPS fix), US-4 |

## Requirement
1. **Every** message sent from the pager (starts and replies) SHALL end with a position suffix.
2. **Precision**: coordinates SHALL be rounded to **4 decimal places** (1e-4° ≈ 11 m latitude, ≈ 7 m longitude at 52°N). This gives **~10 m maximum precision**. More decimals SHALL NOT be sent.
3. Suffix variants:

   | Situation | Suffix | Example |
   |---|---|---|
   | valid fix (age ≤ 2 min) | `[<lat>,<lon>]` | `[52.5201,13.4050]` |
   | last known fix, older than 2 min | `[<lat>,<lon> ~<age>]` | `[52.5201,13.4050 ~12min]` |
   | never had a fix since boot | `[no GPS]` | `[no GPS]` |

   `<age>`: `<n>min` for under 60 min, `<n>h` for under 48 h, otherwise `<n>d`. The `min` unit avoids confusion with metres.
4. Sending SHALL **never** be delayed or blocked while waiting for a fix.
5. Number format: decimal point, optional leading `-`, no `+`, exactly 4 decimals, no spaces around the comma.

## Acceptance criteria
- Sent coordinates never have more than 4 decimals.
- With the GPS antenna disconnected from boot, messages end in `[no GPS]` and are sent immediately.
- After losing a fix, messages carry the last fix with the correct age.

## Implementation notes
- Code: `ui-pager/PagerLocation.h` (`formatPosSuffix`, `parsePosSuffix`, `distanceBearing`), with host-tested edge cases (rounding, negative values, malformed suffixes).
- `LocationProvider::getLatitude()/getLongitude()` return 1e-6 degrees (`long`). Round with `(v + sign*50) / 100` to get 1e-4 units, then print as `%s%ld.%04ld`.
