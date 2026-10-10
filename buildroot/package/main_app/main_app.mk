################################################################################
#
# main_app
#
################################################################################

MAIN_APP_VERSION = 1.0.0.0
MAIN_APP_SITE = $(TOPDIR)/../app/main_app
MAIN_APP_SITE_METHOD = local
MAIN_APP_INSTALL_STAGING = NO
MAIN_APP_DEPENDENCIES = dbus rkwifibt-app
MAIN_APP_CONF_OPTS = \
	-DNET_APP_ROOT=$(TOPDIR)/../app/net_app \
	-DAPP_SHARED_ROOT=$(TOPDIR)/../app/shared \
	-DSHARED_INCLUDE_DIR=$(TOPDIR)/../app/shared

define MAIN_APP_INSTALL_INIT
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S55main_app \
		$(TARGET_DIR)/etc/init.d/S55main_app
endef
MAIN_APP_POST_INSTALL_TARGET_HOOKS += MAIN_APP_INSTALL_INIT

# main_app links librkwifibt.so. rkwifibt-app only drops that library into
# staging (for the link); copy the same file into the rootfs so the dynamic
# linker can start /usr/bin/main_app.
define MAIN_APP_INSTALL_RKWIFIBT
	$(INSTALL) -D -m 0755 $(STAGING_DIR)/usr/lib/librkwifibt.so \
		$(TARGET_DIR)/usr/lib/librkwifibt.so
endef
MAIN_APP_POST_INSTALL_TARGET_HOOKS += MAIN_APP_INSTALL_RKWIFIBT

$(eval $(cmake-package))
