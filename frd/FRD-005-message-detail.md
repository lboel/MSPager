# FRD-005 — Message detail

| | |
|---|---|
| Status | Implemented (M3) |
| Milestone | M3 |
| PRD | US-3, US-4 |

## Requirement
1. Opening a message SHALL show a detail screen with:
   - **Sender nickname** (inverted title bar, left) and the **relative position** (title bar, right: `1.2km NE`)
   - **Message text at double size** (accessibility), word-wrapped, at most 2 lines, with the full mention if present
   - **Distance and bearing** from the own last fix to the sender's position, e.g. `1.2km NE` (`< 1 km`: metres rounded to 10 m, `< 10 km`: one decimal). A leading `~` means the sender's fix was older than 2 min when sent. Own messages show `you`
   - **Absolute coordinates** as sent (`52.5201,13.4050`) and the **message age** (`now`, `5min`, `2h`) in one row below the body
2. All of the above SHALL fit on one screen, without scrolling.
3. If the sender sent `[no GPS]`: the title shows `no GPS` and the coordinates row shows `no GPS fix`. Without any suffix (e.g. free text from an app): `no position`.
4. If the own position is unknown: the title shows `no own GPS`, and the coordinates are still shown.
5. Opening the detail view SHALL mark the message as read.

## Layout (128×64)
```
|Anna           1.2km NE|   title bar: sender + relative position
|Angekommen?            |   body at double size
|                       |   2nd body line if needed
|52.5201,13.4050    5min|   absolute position + message age
|───────────────────────|
|Hold:reply  2x:back    |
```

## Acceptance criteria
- With both GPS fixes, the distance is within ±20 m and the bearing within ±1 of 8 compass sectors (N, NE, E, …).
- The degraded cases (3, 4) render as specified.

## Implementation notes
- Great-circle distance: haversine. Initial bearing: `atan2`, mapped to 8 sectors.
- Own position: `sensors.getLocationProvider()` ([FRD-011](FRD-011-gps-acquisition.md)).
