#!/bin/bash -e

source "${RK_POST_HELPER:-$(dirname "$(realpath "$0")")/../post-hooks/post-helper}"

[ "$TARGET_DIR" ] || exit 0
[ -d "$TARGET_DIR/etc/init.d" ] || exit 0

# Runs after 90-overlay.sh. rcS only executes /etc/init.d/S??* when mode
# is 0755. Keep S39aic-btusb-bind and the other SysV hooks executable in
# the packed rootfs even if overlay source bits were 0644.
chmod 0755 "$TARGET_DIR"/etc/init.d/S[0-9][0-9]* 2>/dev/null || true
if [ -f "$TARGET_DIR/etc/init.d/S39aic-btusb-bind" ]; then
	chmod 0755 "$TARGET_DIR/etc/init.d/S39aic-btusb-bind"
fi
