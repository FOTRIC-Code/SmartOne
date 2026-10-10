################################################################################
#
# fab_app (prebuilt)
#
################################################################################

FAB_APP_VERSION = 1.0.0.0
FAB_APP_SITE = $(TOPDIR)/../prebuilts/apps/fab_app
FAB_APP_SITE_METHOD = local
FAB_APP_INSTALL_STAGING = NO

define FAB_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fab_app $(TARGET_DIR)/usr/bin/fab_app
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S62fab_app \
		$(TARGET_DIR)/etc/init.d/S62fab_app
endef

$(eval $(generic-package))
