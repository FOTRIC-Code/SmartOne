################################################################################
#
# discovery_app (prebuilt)
#
################################################################################

DISCOVERY_APP_VERSION = 1.0.0.0
DISCOVERY_APP_SITE = $(TOPDIR)/../prebuilts/apps/discovery_app
DISCOVERY_APP_SITE_METHOD = local
DISCOVERY_APP_INSTALL_STAGING = NO

define DISCOVERY_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/discovery_app $(TARGET_DIR)/usr/bin/discovery_app
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S61discovery_app \
		$(TARGET_DIR)/etc/init.d/S61discovery_app
endef

$(eval $(generic-package))
