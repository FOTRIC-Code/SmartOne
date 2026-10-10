# Parkia.Cube www

Vue3 + Naive UI + Vite frontend (ported from Holly.Lantern `source/www`).
Hosted by `net_app` at `/var/www`.

## SoftAP = hotspot, provisioning = BLE only

- SoftAP is a normal local hotspot: open `http://192.168.4.1/` for the **full SPA**
  (preview / settings). Wi-Fi provisioning is **BLE-only** (Parkia App), not in the web UI.
- Unprovisioned SoftAP sessions auto-enter a guest login so the UI is usable
  without STA credentials.
- Legacy `/provision.html` requests are redirected to the SPA home by `net_app` (`http_api.c`).

## Layout

```text
app/www/
├── src/              # SPA source
├── softap/           # fallback index when output/ is missing
├── output/           # vite build output (gitignored)
├── package.json
└── Makefile
```

## Build

```sh
cd app/www && make build
cd ../.. && ./build.sh --tiny          # full + OTA tiny
```

## Flash / verify

```sh
# SoftAP (unprovisioned): open http://192.168.4.1/ → SPA works as hotspot UI
# Wi-Fi join: use Parkia App BLE provisioning
# STA: same SPA at device LAN IP
```
