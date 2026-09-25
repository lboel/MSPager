# FRD-018 — Group config file (`pager.ini`)

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | §8, US-7 |
| Supersedes | the "random key per device" part of FRD-008 |

## Requirement
1. A git-tracked file **`pager.ini`** in the repo root SHALL hold the group configuration:

   | Key | Meaning | Default |
   |---|---|---|
   | `channel_name` | name of the pager channel (≤ 31 chars) | `Pager` |
   | `channel_key` | 128-bit channel key, **exactly 32 hex characters** | generated once, `openssl rand -hex 16` |
   | `radio_freq` / `radio_bw` / `radio_sf` / `radio_cr` | LoRa settings | MeshCore **EU/UK (Narrow)**: 869.618 MHz / 62.5 kHz / SF 8 / CR 5 |

2. It's included by the root `platformio.ini` (`extra_configs`), and env `heltec_v4_pager` turns it into build flags `PAGER_CHANNEL_NAME`, `PAGER_CHANNEL_KEY` and `PAGER_RADIO_*`.
3. At **every boot**, each pager SHALL:
   - ensure **exactly one** channel named `channel_name` exists, holding `channel_key`. It's created in the first free slot if missing, and its key is overwritten if different. Duplicates with the same name are removed.
   - apply the radio settings from the file.
4. Changes made in the MeshCore app to the pager channel or the radio settings SHALL only last until the next reboot.
5. A key that isn't 32 characters long SHALL fail the build (`static_assert`). A key with non-hex characters SHALL leave the channels untouched (the UI then shows `No 'Pager' channel`).

## Acceptance criteria
- Two pagers flashed from the same checkout exchange messages right after flashing, with no app setup.
- Changing the key in the app and rebooting restores the key from `pager.ini`.

## Security note
Anyone who can read `pager.ini` (repo access, or anyone who has a built `.bin`) can decrypt the group's messages. Keep the repository private. To rotate the key: generate a new one, rebuild and **reflash all pagers**.

## Implementation notes
- `MyMesh::ensurePagerChannel()` and the `PAGER_RADIO_*` block in `MyMesh::begin()` (`examples/companion_radio/MyMesh.cpp`).
- Hex, not base64: `base64.hpp` defines non-inline functions and can only be included in one file (`BaseChatMesh.cpp`).
