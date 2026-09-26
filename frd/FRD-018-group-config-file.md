# FRD-018 — Group config file (`pager.ini`)

| | |
|---|---|
| Status | Implemented (M2) |
| Milestone | M2 |
| PRD | §8, US-7 |
| Supersedes | the "random key per device" part of FRD-008 |

## Requirement
1. The group configuration SHALL be split into a git-tracked **`pager.ini`** (name, radio) and a **gitignored `pager.secret.ini`** (the key). The key must never be committed, because the repo is a public fork (GitHub doesn't allow forks of public repos to be private). `pager.secret.ini.example` is the template.

   | Key | Meaning | Default |
   |---|---|---|
   | `channel_name` | name of the pager channel (≤ 31 chars) | `Pager` |
   | `channel_key` | 128-bit channel key, **exactly 32 hex characters**, **only in `pager.secret.ini`** (`pager.ini` holds a placeholder) | generated once, `openssl rand -hex 16` |
   | `radio_freq` / `radio_bw` / `radio_sf` / `radio_cr` | LoRa settings | MeshCore **EU/UK (Narrow)**: 869.618 MHz / 62.5 kHz / SF 8 / CR 5 |

2. Both files are included by the root `platformio.ini` (`extra_configs`, secret file last so it overrides the placeholder), and env `heltec_v4_pager` turns them into build flags `PAGER_CHANNEL_NAME`, `PAGER_CHANNEL_KEY` and `PAGER_RADIO_*`.
3. At **every boot**, each pager SHALL:
   - ensure **exactly one** channel named `channel_name` exists, holding `channel_key`. It's created in the first free slot if missing, and its key is overwritten if different. Duplicates with the same name are removed.
   - apply the radio settings from the file.
4. Changes made in the MeshCore app to the pager channel or the radio settings SHALL only last until the next reboot.
   *(Since FRD-021/FRD-023, a setup uploaded over BLE overrides channel and radio from this file.)*
5. A missing `pager.secret.ini` or a key that isn't 32 characters long SHALL fail the build (`static_assert` with a hint to create the file). A key with non-hex characters SHALL leave the channels untouched (the UI then shows `No 'Pager' channel`).

## Acceptance criteria
- Two pagers flashed from the same checkout exchange messages right after flashing, with no app setup.
- Changing the key in the app and rebooting restores the key from `pager.ini`.

## Security note
Anyone who has `pager.secret.ini` or a built `.bin` can decrypt the group's messages. Share the key file only with people who build pagers for the group. Don't publish `.bin` files. To rotate the key: generate a new one, rebuild and **reflash all pagers**.

## Implementation notes
- `MyMesh::ensurePagerChannel()` and the `PAGER_RADIO_*` block in `MyMesh::begin()` (`examples/companion_radio/MyMesh.cpp`).
- Hex, not base64: `base64.hpp` defines non-inline functions and can only be included in one file (`BaseChatMesh.cpp`).
