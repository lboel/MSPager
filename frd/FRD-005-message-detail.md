# FRD-005 — Message detail

| | |
|---|---|
| Status | Proposed |
| Milestone | M3 |
| PRD | US-3, US-4 |

## Requirement
1. Opening a message SHALL show a detail screen with:
   - **Sender nickname** (large or bold in the first row)
   - **Message text** (the canned text, with the mention if present)
   - **Age** of the message (`2 min ago`, `1 h ago`)
   - **Distance and bearing** from the own current position to the sender's position, e.g. `1.2 km NE` (`< 1 km`: metres, rounded to 10 m)
   - **Coordinates** as sent, e.g. `52.5201, 13.4050`. If the position is a stale last fix, also its age (`fix ~12min old`)
2. All of the above SHALL fit on one screen, without scrolling.
3. If the sender sent `[no GPS]` or no position suffix: show `Position: unknown`. Distance/bearing is omitted.
4. If the own position is unknown: show the coordinates and `Distance: no own fix`.
5. Opening the detail view SHALL mark the message as read.

## Layout (128×64)
```
|Anna                   |
|Angekommen?            |
|2 min ago              |
|1.2 km NE              |
|52.5201, 13.4050       |
|Hold: reply  2x: back  |
```

## Acceptance criteria
- With both GPS fixes, the distance is within ±20 m and the bearing within ±1 of 8 compass sectors (N, NE, E, …).
- The degraded cases (3, 4) render as specified.

## Implementation notes
- Great-circle distance: haversine. Initial bearing: `atan2`, mapped to 8 sectors.
- Own position: `sensors.getLocationProvider()` ([FRD-011](FRD-011-gps-acquisition.md)).
