# FRD-017 — BLE pairing mode

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 (bug fix) |
| PRD | US-7 |

## Background
Bug report: one of two pagers was only visible in the MeshCore app for a short time after boot. Pairing itself didn't fail.

Cause: the ESP32 BLE stack **stops advertising while any peer holds a connection** (`SerialBLEInterface::checkRecvFrame`), and only restarts once the connection count is back to 0. If a phone grabs the link in the background (the MeshCore app auto-reconnecting, or another phone that paired earlier), the pager disappears from every scan.

## Requirement
1. Holding **PRG for ≥ 10 s** (`PAGER_PAIRING_HOLD_MILLIS`) SHALL enter **pairing mode**, from any screen, including with the display off.
2. On entry, the pager SHALL **drop the current BLE connection** and **restart advertising** (BLE disable, then enable ~0.5 s later).
3. The pairing screen SHALL show the **BLE PIN in large digits** (text size 3), the nickname, a countdown and the hint `Tap:cancel`.
4. Pairing mode SHALL end:
   - when a **new authenticated connection** is established → popup `Connected`, or
   - after **30 s** (`PAGER_PAIRING_SECS`) → popup `Pairing timeout`, or
   - on a short or double press → cancelled.

   BLE stays enabled and advertising in all cases.
5. The display SHALL stay on while pairing mode is active.

## Acceptance criteria
- With a phone holding a background connection, a 10 s hold makes the pager show up in the app's device list again.
- The large PIN disappears as soon as the app has connected.

## Notes
- The 1 s long press still fires on the way to 10 s. Start the hold from the **chat screen** or with the display off: holding on a picker would send the highlighted message at 1 s.
- Bonds are **not** cleared. If a phone has stale pairing data (e.g. after a full erase of the pager), remove the pager in the phone's Bluetooth settings.
