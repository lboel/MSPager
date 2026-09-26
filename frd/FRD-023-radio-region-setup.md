# FRD-023 — Radio region and frequency in the setup YAML

| | |
|---|---|
| Status | Implemented |
| Milestone | M4d |
| PRD | §8, US-7, US-10 |
| Extends | FRD-018 (radio from `pager.ini`), FRD-021 (setup over BLE) |

## Requirement
1. The setup YAML SHALL have an optional `radio` block that controls the group's LoRa settings per **country/region**:
   ```yaml
   radio:
     region: EU            # EU UK US CA AU NZ IN KR
     frequency: 869.618    # MHz
     bandwidth: 62.5       # kHz
     spreading_factor: 8
     coding_rate: 5
   ```
2. `region` is required and sets the **allowed band**: EU/UK 863–870, US/CA 902–928, AU/NZ 915–928, IN 865–867, KR 920–923 MHz. The whole channel (frequency ± bandwidth/2) SHALL lie inside it. Otherwise the upload is rejected with line and reason.
3. EU/UK and US/CA SHALL have **presets** (869.618/62.5/8/5 as in `pager.ini`, and 910.525/62.5/7/5 from the MeshCore FAQ). Values that aren't given come from the preset. Regions without a preset need all four values.
4. Validation: frequency ≤ 3 decimals; bandwidth one of the SX1262 values (7.8 … 500 kHz); SF 5–12; CR 5–8.
5. The radio settings SHALL apply **immediately after a successful upload** and at **every boot**, overriding `pager.ini`. `CLEAR` goes back to `pager.ini`.
6. The STATUS/RESULT summary SHALL show the active radio: `radio=<region|build>:<MHz>/<kHz>/<SF>/<CR>`.

## Acceptance criteria
- `radio: {region: EU}` → summary `radio=EU:869.618/62.5/8/5`.
- `region: EU, frequency: 915` → rejected: `frequency outside EU band 863-870 MHz`.
- Two pagers uploaded with the same `radio` (e.g. EU 868.000) exchange messages. A third pager still on 869.618 doesn't receive them.
- After a reboot, the radio from the setup is still active. After `clear`, it's `radio=build:869.618/62.5/8/5`.

## Notes
- TX power, duty cycle and sub-band rules aren't part of the setup. The group is responsible for using a legal frequency.
- A pager with a different radio is cut off from the group. The frontend must upload the same `radio` to every member.
- Implementation: `PagerConfig` (region table, `radioKey()`, `finishRadio()`), `MyMesh::loadPagerRadioPrefs()` (boot + `applyPagerConfig()` → `radio_driver.setParams()`, same as `CMD_SET_RADIO_PARAMS`).
