BUILD_OUT     := output
# Parkia.Cube: stage next to app/ for packaging helpers (optional).
INSTALL_DIR   := ../www_dist
INSTALL_WWW   := $(INSTALL_DIR)
WWW_DEST      := $(ROOTFS_DIR)/var/www
ifeq ($(wildcard $(ROOTFS_DIR)/rootfs),)
else
WWW_DEST      := $(ROOTFS_DIR)/rootfs/var/www
endif

.PHONY: all build install clean install-rootfs

# Prefer pnpm; fall back to npm when pnpm is not on PATH (e.g. nvm not loaded).
PKG := $(shell command -v pnpm 2>/dev/null)
ifeq ($(PKG),)
PKG := $(shell command -v npm 2>/dev/null)
endif
ifeq ($(PKG),)
$(error Neither pnpm nor npm found in PATH)
endif

all: build

build:
	$(PKG) install
	$(PKG) run build

# Install built SPA + SoftAP static pages into a destination directory.
# Usage: make install DEST=/path/to/var/www
#        make install ROOTFS_DIR=/path/to/rootfs
install:
	@dest="$(DEST)"; \
	if [ -z "$$dest" ] && [ -n "$(ROOTFS_DIR)" ]; then dest="$(WWW_DEST)"; fi; \
	if [ -z "$$dest" ]; then dest="$(INSTALL_WWW)"; fi; \
	$(MAKE) install-dest DEST="$$dest"

install-dest: $(BUILD_OUT)
	@test -n "$(DEST)" || (echo "DEST required" >&2; exit 1)
	mkdir -p "$(DEST)"
	rm -rf "$(DEST)"/* "$(DEST)"/.[!.]* 2>/dev/null || true
	cp -rf $(BUILD_OUT)/. "$(DEST)/"
	@echo -e "\e[1;32mWeb installed to $(DEST) ($$(du -sh "$(DEST)" | cut -f1))\e[0m"

$(BUILD_OUT):
	$(MAKE) build

clean:
	rm -rf $(BUILD_OUT) node_modules $(INSTALL_DIR)
