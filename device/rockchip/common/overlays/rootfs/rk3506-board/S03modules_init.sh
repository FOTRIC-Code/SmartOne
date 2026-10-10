#!/bin/sh
#
# Load only the modules this board needs at boot.
# AIC Wi-Fi/BT (aic_load_fw/aic8800_fdrv/aic_btusb) are loaded by wifibt-init.sh.
# Do not insmod in-tree btusb or unused USB NIC/modem drivers.
#

MODULES="
usb-common
usbcore
phy-rockchip-inno-usb2
dwc2
usbhid
scsi_common
scsi_mod
sd_mod
usb-storage
cfg80211
mac80211
udc-core
libcomposite
u_ether
usb_f_rndis
usb_f_fs
"

load_modules() {
	for m in $MODULES; do
		modprobe "$m" 2>/dev/null || true
	done
}

unload_modules() {
	for m in $MODULES; do
		modprobe -r "$m" 2>/dev/null || true
	done
}

case "$1" in
	start)
		load_modules
		;;
	#stop)
	#	unload_modules
	#	;;
	*)
		echo "Usage: $0 {start|stop|restart}"
		exit 1
esac

exit $?
