# FRD-025 — Setup web app with group password

| | |
|---|---|
| Status | Draft |
| Milestone | M5 |
| PRD | US-10 |
| Extends | FRD-021 (setup over BLE), FRD-024 (firmware update over BLE) |

## Requirement
1. A static web app (`webapp/`, no server logic) SHALL set up pagers over **Web Bluetooth**, also from a phone: nickname, channel, radio region, questions with replies (FRD-021), and firmware updates (FRD-024).
2. The channel key SHALL be derivable from a **group password** plus the channel name (PBKDF2-HMAC-SHA256, 600,000 iterations, fixed salt, [protocol §6](../docs/pager_config_protocol.md)). Entering a hex key directly SHALL stay possible.
3. The app SHALL offer a generated password of **4 random words** (EFF large wordlist) and warn about weak passwords.
4. The password and key SHALL NOT be stored or sent anywhere except to the pager. The rest of the form MAY be kept in the browser.

## Acceptance criteria
- The same password and channel name give the same key on two devices; the test vector in the protocol doc matches.
- An upload from the web app is accepted by the pager, and two pagers set up with the same group exchange messages.
- A V3 update and a T114 update from an Android phone succeed.

## Notes
- Web Bluetooth: Chrome/Edge on desktop and Android. iOS only with a browser like Bluefy. The page must be served over https (or localhost).
- The firmware files a group publishes for the app still contain the build key from `pager.secret.ini` as a fallback. Build published files with a throwaway key.
