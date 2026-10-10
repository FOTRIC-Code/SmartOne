################################################################################
#
# net_app
#
################################################################################

NET_APP_VERSION = 1.0.0.0
NET_APP_SITE = $(TOPDIR)/../app/net_app
NET_APP_SITE_METHOD = local
NET_APP_INSTALL_STAGING = NO
NET_APP_DEPENDENCIES = libusb openssl libcurl jpeg
NET_APP_CONF_OPTS = \
	-DNET_APP_ENABLE_THERMAL_UVC=ON \
	-DNET_APP_SDK_DIR=$(TOPDIR)/.. \
	-DAPP_SHARED_ROOT=$(TOPDIR)/../app/shared
# NET_APP_SDK_DIR: absolute SDK root (not relative to package extract).

# ZLMediaKit ships as a checked-in prebuilt shared library
# (app/net_app/libs/zlm/libmk_api.so + include/); rebuild from source
# with app/_legacy/build_zlm.sh (requires zlm sources, not in this repo).

# After rsync: snapshot www output into the build dir so the install hook can
# find it even if the SDK re-syncs the app/ tree later during the same build.
# The snapshot is refreshed whenever app/www/output is newer; otherwise an
# obsolete _www_output/ would ship forever.
define NET_APP_SNAPSHOT_WWW
	@if [ -d "$(TOPDIR)/../app/www/output" ] && \
	    [ -f "$(TOPDIR)/../app/www/output/index.html" ] && \
	    [ -f "$(TOPDIR)/../app/www/output/bundle.js" ]; then \
		if [ ! -f "$(@D)/_www_output/bundle.js" ] || \
		   [ "$(TOPDIR)/../app/www/output/bundle.js" -nt \
		     "$(@D)/_www_output/bundle.js" ]; then \
			rm -rf "$(@D)/_www_output"; \
			cp -a "$(TOPDIR)/../app/www/output" "$(@D)/_www_output"; \
			echo "net_app: www snapshot refreshed from app/www/output"; \
		else \
			echo "net_app: www snapshot up to date"; \
		fi; \
	elif [ -f "$(TOPDIR)/../app/www/package.json" ]; then \
		echo "net_app: www output missing, attempting build..."; \
		$(MAKE) -C "$(TOPDIR)/../app/www" build 2>&1 || true; \
		if [ -f "$(TOPDIR)/../app/www/output/index.html" ] && \
		   [ -f "$(TOPDIR)/../app/www/output/bundle.js" ]; then \
			rm -rf "$(@D)/_www_output"; \
			cp -a "$(TOPDIR)/../app/www/output" "$(@D)/_www_output"; \
			echo "net_app: www built and staged"; \
		else \
			echo "WARNING: www build failed; web UI may be missing" >&2; \
		fi; \
	fi
endef

NET_APP_POST_RSYNC_HOOKS += NET_APP_SNAPSHOT_WWW

define NET_APP_INSTALL_RUNTIME_WRAPPERS
	# Init scripts + www; net_app binary itself comes from cmake install → /usr/bin/net_app.
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S39aic-btusb-bind \
		$(TARGET_DIR)/etc/init.d/S39aic-btusb-bind
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S14_Upgrade \
		$(TARGET_DIR)/etc/init.d/S14_Upgrade
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S15_SD \
		$(TARGET_DIR)/etc/init.d/S15_SD
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S61discovery_app \
		$(TARGET_DIR)/etc/init.d/S61discovery_app
	$(INSTALL) -D -m 0755 \
		$(TOPDIR)/../device/rockchip/common/overlays/rootfs/custom/root/etc/init.d/S62fab_app \
		$(TARGET_DIR)/etc/init.d/S62fab_app
	# Sibling module app/www → /var/www: built SPA (output/).
	# Never ship Vue source / node_modules into rootfs. The *.gz siblings must
	# stay: mongoose serves <file>.gz when the client sends Accept-Encoding.
	# Prefer staged _www_output/ in the build dir (survives SDK re-syncs),
	# fall back to the traditional app/www/output/ path.
	mkdir -p $(TARGET_DIR)/var/www
	www_installed=0; \
	for www_dir in "$(@D)/_www_output" "$(TOPDIR)/../app/www/output"; do \
		if [ -d "$$www_dir" ] && \
		   [ -f "$$www_dir/index.html" ] && \
		   [ -f "$$www_dir/bundle.js" ]; then \
			rm -rf $(TARGET_DIR)/var/www/* $(TARGET_DIR)/var/www/.[!.]* 2>/dev/null || true; \
			cp -a "$$www_dir/." $(TARGET_DIR)/var/www/; \
			www_installed=1; \
			break; \
		fi; \
	done; \
	if [ "$$www_installed" = "0" ]; then \
		if [ -f $(TOPDIR)/../app/www/softap/index.html ]; then \
			echo "WARNING: www output missing; installing SoftAP index only" >&2; \
			$(INSTALL) -m 0644 $(TOPDIR)/../app/www/softap/index.html \
				$(TARGET_DIR)/var/www/index.html; \
		fi; \
	fi
endef

NET_APP_POST_INSTALL_TARGET_HOOKS += NET_APP_INSTALL_RUNTIME_WRAPPERS

# Vendored zlm/irtek .so belong under /usr/lib/net_app (app-private).
# Strip any leftover copies from system /usr/lib after incremental rebuilds.
define NET_APP_CLEAN_SYSTEM_VENDOR_LIBS
	rm -f $(TARGET_DIR)/usr/lib/libmk_api.so \
		$(TARGET_DIR)/usr/lib/libIRtekUVCSDK.so \
		$(TARGET_DIR)/usr/lib/libIRtekFileSDK.so
endef
NET_APP_POST_INSTALL_TARGET_HOOKS += NET_APP_CLEAN_SYSTEM_VENDOR_LIBS

$(eval $(cmake-package))
