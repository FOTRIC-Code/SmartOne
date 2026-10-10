#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

if [ ! -d "$OVERLAY_DIR/root" ]; then
	mkdir -p "$OVERLAY_DIR/root"
fi

$RK_RSYNC "$OVERLAY_DIR/root/" "$TARGET_DIR/"

# Overlay rsync uses --chmod=u=rwX,go=rX. Capital X keeps execute only when
# the source already has +x. S39 was repeatedly packed as 0644, so rcS
# skipped it and aic_btusb never loaded. Force 0755 on init scripts.
if [ -d "$TARGET_DIR/etc/init.d" ]; then
	chmod 0755 "$TARGET_DIR"/etc/init.d/S[0-9][0-9]* 2>/dev/null || true
fi
if [ -f "$TARGET_DIR/usr/bin/net_app" ]; then
	chmod 0755 "$TARGET_DIR/usr/bin/net_app"
fi
if [ -f "$TARGET_DIR/usr/bin/fab_app" ]; then
	chmod 0755 "$TARGET_DIR/usr/bin/fab_app"
fi
if [ -f "$TARGET_DIR/usr/bin/discovery_app" ]; then
	chmod 0755 "$TARGET_DIR/usr/bin/discovery_app"
fi
