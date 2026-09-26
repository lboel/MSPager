# FRD-021 — Setup over BLE (YAML: nickname, channel, questions)

| | |
|---|---|
| Status | Implemented |
| Milestone | M4b |
| PRD | §8, US-7, US-10 |
| Extends | FRD-006 (catalogue), FRD-008/FRD-018 (channel), FRD-009 (nickname), FRD-015 (app config) |

## Requirement
1. A setup frontend SHALL be able to configure an **already flashed** pager over **Bluetooth** with a **YAML file**. Protocol and format: [docs/pager_config_protocol.md](../docs/pager_config_protocol.md).
2. The YAML SHALL control:
   - `nickname`: the pager's node name (per pager)
   - `channel.name` and `channel.key`: the group channel (32 hex characters)
   - `questions`: up to 12 extra questions, **each with its own 1–6 replies** (texts ≤ 40 bytes)
3. The pager SHALL **parse and validate** the YAML itself. An invalid file SHALL change nothing and SHALL be answered with the **line number and reason**. A valid one SHALL be stored as a **compact binary blob** (`/pager_cfg`) and applied immediately.
4. Every value is optional. A value in the setup **overrides** the build config (`pager.ini` / `pager.secret.ini`), and a missing value keeps it. An upload replaces the previous setup as a whole. `CLEAR` deletes it.
5. The setup SHALL be **enforced at every boot**, like FRD-018: nickname, channel name and key.
6. **Send menu:** the built-in starters (`Angekommen?`, `Brauche Hilfe`, `Standort?`, `Mein Standort`) followed by the setup questions. A setup question with a built-in text only replaces that question's replies.
7. **Reply menu:** if the selected message's text (without the `@[Nick] ` mention) equals a setup question, the pager SHALL offer that question's replies. Otherwise it offers the default replies (`Ja`, `Nein`, `OK`, `📞`, `Standort?`, `Mein Standort`).
8. After a successful upload, the pager SHALL wake and show `Setup updated`.
9. German letters (`äöüÄÖÜß`, plus `é`, `°`) SHALL render correctly on the OLED, because setup texts are free-form.

## Acceptance criteria
- Uploading [pager_setup_example.yaml](../docs/pager_setup_example.yaml) with `bin/pager_setup.py` gives status 0. The pager shows `Setup updated`, and the header shows the new nickname.
- Two pagers set up with the same `channel` and `questions` exchange a setup question and answer it with that question's replies.
- A file with a typo (`nicknme: Anna`) is rejected with `line 1: unknown key 'nicknme'`, and the previous setup stays active.
- After a reboot, the setup is still active. A nickname changed in the MeshCore app is reset to the setup's nickname.
- `CLEAR` goes back to the `pager.ini` channel and the built-in catalogue.

## Implementation notes
- Parser and flash format: `examples/companion_radio/ui-pager/PagerConfig.{h,cpp}` (plain C++, unit tests in `test/test_pager_config`, `pio test -e native`).
- BLE command `CMD_PAGER_CONFIG = 0x70`, ops BEGIN/DATA/COMMIT/STATUS/CLEAR, handled in `MyMesh::handlePagerConfigCmd()`. There's no dynamic allocation: static 4 KB upload buffer, max blob 3.5 KB (usually < 300 bytes).
- UI: `UITask::buildComposeOptions()` / `buildReplyOptions()`. `MyMesh::getPagerConfigGen()` tells the UI about a new setup.
- Radio settings aren't part of the setup and stay in `pager.ini`.
