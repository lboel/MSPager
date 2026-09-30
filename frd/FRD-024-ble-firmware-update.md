# FRD-024 — Firmware update over Bluetooth

| | |
|---|---|
| Status | Implemented (draft), device test open |
| Milestone | M5 |
| PRD | US-10 |
| Extends | FRD-021 (setup over BLE) |

## Requirement
1. A pager that runs an MSPager firmware with update support SHALL accept a new firmware over **Bluetooth**, so it can be updated from a phone. The first install stays USB.
2. ESP32 boards (V3, V4) SHALL receive the app image over the companion link (`0x71` BEGIN/DATA/END) into the spare app slot, and boot it only after the **size, MD5 and board marker** were checked. On any failure the running firmware stays active.
3. nRF52 boards (T114) SHALL restart into the bootloader's BLE DFU mode on request (`0x71` DFU). The image is then sent with Nordic's legacy DFU protocol.
4. Every image SHALL contain the board marker `MSPAGER-BOARD:<env>`. INFO reports the board, firmware version and update method. Clients SHALL refuse a file for another board.
5. Settings, the uploaded setup and Bluetooth pairings SHALL survive an update.

## Acceptance criteria
- V3: an update with the `.bin` of the same env succeeds; the pager restarts with the new version (INFO `fw=`), setup and pairing kept.
- V3: the `.bin` of `heltec_v4_pager` is refused by the client, and by the pager at END if sent anyway.
- V3: a transfer that stops midway (disconnect) leaves the old firmware running.
- T114: the `.zip` update via DFU succeeds; the pager boots the new version with its setup.

## Notes
- Protocol: [docs/pager_config_protocol.md §5](../docs/pager_config_protocol.md). Clients: `webapp/`, `bin/pager_ota.py`.
- Speed: the ESP32 answers at most every 60 ms and queues 4 frames, so DATA is acknowledged every 4th frame.
- Implementation: `ui-pager/PagerOta.{h,cpp}` (platform part), `MyMesh::handlePagerOtaCmd()`.
