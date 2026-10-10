/*
 * provision_shared.h — shared definitions between main_app and net_app.
 *
 * Both binaries read/write the same JSON file at PROVISION_SHARED_PATH.
 * main_app writes Wi-Fi credentials + provision state after a successful
 * BLE configuration. net_app reads them to learn its network environment.
 *
 * The shared config file lives on a persistent partition (/etc/cfg) so it
 * survives reboots.
 */
#ifndef PARKIA_PROVISION_SHARED_H
#define PARKIA_PROVISION_SHARED_H

/* ── paths ─────────────────────────────────────────────────────────── */

/* Shared provisioning state file: written by main_app, read by net_app. */
#define PROVISION_SHARED_DIR  "/etc/cfg/provision/state"
#define PROVISION_SHARED_PATH PROVISION_SHARED_DIR "/config.json"

/*
 * Trigger file: main_app creates this after a successful Wi-Fi join.
 * The init script watches for it and starts net_app.
 * net_app itself can also check it at startup to confirm readiness.
 */
#define PROVISION_TRIGGER_PATH "/tmp/provision_ready"

/* ── provision states (integer, stored in config.json) ─────────────── */

#define PROVISION_STATE_AP_READY      0
#define PROVISION_STATE_PROVISIONING  1
#define PROVISION_STATE_PROVISIONED   2
#define PROVISION_STATE_FAILED        3

/* ── network states ────────────────────────────────────────────────── */

#define NETWORK_STATE_OFFLINE  0
#define NETWORK_STATE_ONLINE   1

/* ── network modes ─────────────────────────────────────────────────── */

#define NETWORK_MODE_AP   0
#define NETWORK_MODE_STA  1

#endif /* PARKIA_PROVISION_SHARED_H */
