# MSPager setup web app

A static page that sets up and updates pagers over **Web Bluetooth**, also from a phone ([FRD-025](../frd/FRD-025-web-app-group-password.md)):

- group: channel name, key from a **group password** (or a hex key), radio region, questions with replies
- per pager: nickname; upload, show and reset the setup
- firmware update over Bluetooth ([FRD-024](../frd/FRD-024-ble-firmware-update.md)): V3/V4 with the `.bin`, T114 with the `.zip`

Everything runs in the browser. The password and key only go to the pager. Protocol: [docs/pager_config_protocol.md](../docs/pager_config_protocol.md).

## Run

Web Bluetooth needs a secure context: https, or `localhost`.

```sh
python3 -m http.server 8000 -d webapp     # then open http://localhost:8000 in Chrome/Edge
```

On a phone the page has to come from an https host (e.g. GitHub Pages). Browsers: Chrome/Edge on desktop and Android; on iOS a Web Bluetooth browser like Bluefy.

## Files

| File | |
|---|---|
| `index.html` | page and styles |
| `app.js` | BLE link, setup upload, firmware update (ESP32 + Nordic legacy DFU), password → key, YAML, MD5, zip reader |
| `words.js` | EFF large wordlist (CC BY 3.0 US, [eff.org/dice](https://www.eff.org/dice)) |
