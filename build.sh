#!/usr/bin/env bash
# ────────────────────────────────────────────────────────────────────
# Parkia.Cube 统一构建入口（原 dist.sh 已并入本脚本）
#
# 项目模式（优先匹配）：
#   ./build.sh              增量构建：只重编 app/* + www，重打包 rootfs/固件，
#                           dist/ 输出整包 img + tiny OTA 包（日常开发用）
#   ./build.sh --rebuild    全量构建：整编 SDK（u-boot/kernel/rootfs/app），
#                           不删已有产物，各构建系统自行判断增量；
#                           从零构建先执行 ./build.sh clean
#   ./build.sh clean        清理所有构建产物（dist/ output/ buildroot/output、
#                           kernel/u-boot 原地编译产物、www/output）
#   ./build.sh publish      发布打包：全量构建 + 签名 + sha256 清单
#   ./build.sh fetch-sdk    重新下载 / 校验 / 解压 SmartOne-SDK 底包（公开仓库；
#                           平时缺 SDK 或版本不对时其它模式会自动执行）
#
# 其余参数原样透传给 Rockchip SDK 构建脚本，例如：
#   ./build.sh kernel | uboot | rootfs | menuconfig | shell | cleanall ...
#   ./build.sh help         查看 SDK 全部目标（-h/--help 查看项目模式帮助）
#
# 注意：Buildroot 对 local 源码包用 stamp 记录进度，普通 make 不会感知
# app/net_app 等源码改动，因此增量模式显式执行 <pkg>-dirclean + <pkg>-rebuild
# 强制从干净目录重编四个自研 app（main_app / net_app / fab_app / discovery_app），
# 避免 rsync -au 留下的已删除源文件被误包含。
# ────────────────────────────────────────────────────────────────────

PARKIA_SDK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 原顶层 build.sh 是指向该脚本的符号链接（Rockchip SDK 构建脚本）
PARKIA_SDK_SH="$PARKIA_SDK_DIR/device/rockchip/common/scripts/build.sh"

parkia_die() { echo "ERROR: $*" >&2; exit 1; }

# buildroot/dl 不存在或为空时，从 docs/dl.tar.gz 解压预置下载缓存。
# 归档顶层为 dl/，解压到 buildroot/ 后形成 buildroot/dl/。
# 没有预置包时由 Buildroot 在线下载源码包。
parkia_ensure_dl_cache() {
	local dl="$PARKIA_SDK_DIR/buildroot/dl"
	local archive="$PARKIA_SDK_DIR/docs/dl.tar.gz"

	if [ -d "$dl" ] && [ -n "$(ls -A "$dl" 2>/dev/null)" ]; then
		return 0
	fi

	if [ ! -f "$archive" ]; then
		echo "buildroot/dl 为空且无预置包 $archive，Buildroot 将在线下载源码包（首次构建较慢）。"
		return 0
	fi

	echo "Seeding buildroot/dl from docs/dl.tar.gz ..."
	mkdir -p "$PARKIA_SDK_DIR/buildroot"
	tar -xzf "$archive" -C "$PARKIA_SDK_DIR/buildroot"
	[ -d "$dl" ] && [ -n "$(ls -A "$dl" 2>/dev/null)" ] || \
		parkia_die "解压失败，未得到 $dl（请确认 docs/dl.tar.gz 顶层为 dl/）"
	echo "Seeded: $dl"
}

# ── SmartOne-SDK 底包 ─────────────────────────────────────────────
# 公开仓库中下面的标记块记录 release sdk-v<X.Y.Z> 里的 SDK 包（kernel / u-boot /
# buildroot 主体 / 工具链 / 官方工具）与 dl 包及其 sha256（"<文件名> <sha256>"）。
# 缺失或版本不一致时自动下载、校验并解压到仓库根；版本为空时不使用底包。
# >>> smartone-sdk
PARKIA_SDK_VERSION="1.0.0"
PARKIA_SDK_URL="https://github.com/FOTRIC-Code/SmartOne/releases/download/sdk-v1.0.0"
PARKIA_SDK_PACKAGES=(
	"SmartOne-SDK-v1.0.0.tar.xz 3f4d2876de7a56bb9110cd5186617d1ab036f62dbdff5ddb1503b1316bf4cb6e"
)
PARKIA_SDK_DL="SmartOne-dl-v1.0.0.tar.gz 355d530291d082ba9f0f27d9d84dbeec077a8e763a62d2751c14702eeee505d8"
# <<< smartone-sdk
PARKIA_SDK_META="$PARKIA_SDK_DIR/.smartone_sdk"
PARKIA_SDK_CACHE="$PARKIA_SDK_META/cache"

parkia_sha256() { sha256sum "$1" | cut -d' ' -f1; }

# 下载 <name> 到缓存并校验 sha256，成功时打印缓存路径。
# 缓存里已有且校验通过则不再下载；未下完的 .part 文件支持续传。
parkia_sdk_fetch_asset() {
	local name="$1" sha="$2" base_url="$3"
	local dest="$PARKIA_SDK_CACHE/$name" url="$3/$1"

	mkdir -p "$PARKIA_SDK_CACHE"
	if [ -f "$dest" ] && [ "$(parkia_sha256 "$dest")" = "$sha" ]; then
		printf '%s\n' "$dest"
		return 0
	fi
	rm -f "$dest"
	echo "Downloading $url ..." >&2
	local ok=1
	case "$base_url" in
		file://*) cp -f "${url#file://}" "$dest.part" || ok=0 ;;
		*)
			if command -v curl >/dev/null 2>&1; then
				curl -fL --retry 3 -C - -o "$dest.part" "$url" >&2 || ok=0
			elif command -v wget >/dev/null 2>&1; then
				wget -c -O "$dest.part" "$url" >&2 || ok=0
			else
				parkia_die "curl or wget is required to download $url"
			fi
			;;
	esac
	# 续传时 .part 可能已经完整（服务器返回 416），以校验结果为准
	if [ ! -f "$dest.part" ] || [ "$(parkia_sha256 "$dest.part")" != "$sha" ]; then
		[ "$ok" -eq 1 ] || parkia_die "download failed: $url (rerun to resume)"
		rm -f "$dest.part"
		parkia_die "sha256 mismatch for $name (corrupted or wrong file); rerun to retry"
	fi
	mv -f "$dest.part" "$dest"
	printf '%s\n' "$dest"
}

# 按旧 MANIFEST 删除旧版 SDK 文件；git 跟踪的文件一律保留。
parkia_sdk_remove_old() {
	local manifest="$PARKIA_SDK_META/MANIFEST"
	[ -f "$manifest" ] || return 0
	if ! git -C "$PARKIA_SDK_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
		echo "WARN: not a git checkout, keeping old SDK files (stale files may remain)" >&2
		return 0
	fi
	echo "Removing files of the old SDK ..."
	LC_ALL=C comm -23 <(LC_ALL=C sort "$manifest") \
		<(git -C "$PARKIA_SDK_DIR" ls-files | LC_ALL=C sort) \
		| while IFS= read -r f; do
			rm -f -- "$PARKIA_SDK_DIR/$f"
		done
}

# $1=force 时即使版本一致也重新校验并解压
parkia_ensure_sdk() {
	local force="${1:-}"
	local want="$PARKIA_SDK_VERSION" have base_url line name sha cached
	local parts=()

	[ -n "$want" ] || return 0
	base_url="${SMARTONE_SDK_URL:-$PARKIA_SDK_URL}"
	base_url="${base_url%/}"
	[ -n "$base_url" ] || parkia_die "PARKIA_SDK_URL is empty"
	have="$(sed -n 's/^version=//p' "$PARKIA_SDK_META/VERSION" 2>/dev/null || true)"

	if [ "$have" != "$want" ] || [ "$force" = force ]; then
		echo "SmartOne-SDK: need v$want (installed: ${have:+v}${have:-none})"
		for line in "${PARKIA_SDK_PACKAGES[@]}"; do
			name="${line%% *}"
			sha="${line#* }"
			cached="$(parkia_sdk_fetch_asset "$name" "$sha" "$base_url")" || exit 1
			parts+=("$cached")
		done
		[ "${#parts[@]}" -gt 0 ] || parkia_die "PARKIA_SDK_PACKAGES is empty"

		if [ -n "$have" ] && [ "$have" != "$want" ]; then
			parkia_sdk_remove_old
		fi
		rm -f "$PARKIA_SDK_META/VERSION"
		echo "Extracting SmartOne-SDK v$want ..."
		cat "${parts[@]}" | tar -xJf - -C "$PARKIA_SDK_DIR" || parkia_die "SDK extraction failed"
		[ "$(sed -n 's/^version=//p' "$PARKIA_SDK_META/VERSION" 2>/dev/null)" = "$want" ] || \
			parkia_die "SDK package does not contain v$want"
		rm -f "${parts[@]}"
		echo "SmartOne-SDK v$want installed."
	fi

	# Buildroot 下载缓存：buildroot/dl 已有内容时不需要 dl 包
	local dl="$PARKIA_SDK_DIR/buildroot/dl" archive="$PARKIA_SDK_DIR/docs/dl.tar.gz"
	if [ -d "$dl" ] && [ -n "$(ls -A "$dl" 2>/dev/null)" ]; then
		return 0
	fi
	line="$PARKIA_SDK_DL"
	[ -n "$line" ] || return 0
	name="${line%% *}"
	sha="${line#* }"
	if [ -f "$archive" ] && [ "$(parkia_sha256 "$archive")" = "$sha" ]; then
		return 0
	fi
	cached="$(parkia_sdk_fetch_asset "$name" "$sha" "$base_url")" || exit 1
	mv -f "$cached" "$archive"
}

# Parkia.Cube 板级 defconfig（按启动介质）
parkia_board_defconfig() {
	case "$1" in
		SPI_NAND|spi_nand|spinand|SPI-NAND) echo "rk3506b_smart_one_buildroot_spinand_defconfig" ;;
		SD_CARD|sd_card|sdmmc|SD) echo "rk3506b_smart_one_buildroot_sdmmc_defconfig" ;;
		*) parkia_die "unknown boot medium: $1 (SPI_NAND | SD_CARD)" ;;
	esac
}

# Buildroot 输出目录（含 target/ 的那一层）
parkia_br_output_dir() {
	local d
	for d in "$PARKIA_SDK_DIR"/buildroot/output/rockchip_rk3506_board \
		"$PARKIA_SDK_DIR/output/buildroot"; do
		[ -d "$d/target" ] && { printf '%s\n' "$d"; return 0; }
	done
	return 1
}

parkia_usage() {
	cat <<EOF
Usage: $(basename "$0") [mode] [options]

Project modes (pick one; default is incremental):
  (no mode)             Incremental: rebuild app/* + www only, repack firmware.
                        Outputs full img + tiny OTA into dist/.
                        Requires an existing build tree (run --rebuild once first).
  --rebuild             Full build: build the whole SDK (u-boot / kernel /
                        buildroot rootfs / apps) WITHOUT wiping outputs;
                        each build system decides what to recompile.
                        Run './build.sh clean' first for a from-scratch build.
  clean                 Remove all build outputs (dist/, output/,
                        buildroot/output/, kernel & u-boot in-tree objects,
                        app/www/output).
  publish               Release: full rebuild + sign + sha256 manifest.
  fetch-sdk             SmartOne: download, verify and extract the SmartOne-SDK
                        package pinned in build.sh (done automatically by
                        other modes when missing or outdated).
  pack-opt              Pack repo buildroot/dl + buildroot/output into
                        dist/parkia-cube-opt_V<ver>.tar.gz for distribution.

Options:
  -v, --version VER       Image version (default: 1.0.0.0)
  --boot-medium NAME      Boot medium: SPI_NAND | SD_CARD (default: SPI_NAND)
  -k, --private-key F     Optional RSA private key; if set, sign net_app → sibling "sign"
                          (omit -k or pass empty path to skip signing)
  --rebuild-third-party   Force rebuilding net_app third-party libraries (zlm)
  -h, --help              Show help

Environment:
  VERSION / PRIVATE_KEY / RK_BOOT_MEDIUM
  SMARTONE_SDK_URL      Override the SmartOne-SDK download URL (http(s):// or file://)

Anything else is passed through to the Rockchip SDK build system, e.g.:
  ./build.sh kernel | uboot | rootfs | menuconfig | shell | cleanall
  ./build.sh help           List all SDK targets

Examples:
  ./build.sh                          # daily dev: apps changed → img + tiny
  ./build.sh --rebuild -v 1.0.0.1     # full build (keeps caches)
  ./build.sh clean && ./build.sh --rebuild   # from-scratch build
  ./build.sh clean
  ./build.sh publish -v 1.0.0.1 -k keys/ota_private.pem
EOF
}

parkia_ensure_node_tools() {
	if [ -d /opt/nodejs/current/bin ]; then
		export PATH="/opt/nodejs/current/bin:$PATH"
	fi
	if ! command -v pnpm >/dev/null 2>&1 && [ -s "${NVM_DIR:-$HOME/.nvm}/nvm.sh" ]; then
		# shellcheck disable=SC1091
		. "${NVM_DIR:-$HOME/.nvm}/nvm.sh" >/dev/null 2>&1 || true
	fi
	if command -v pnpm >/dev/null 2>&1 || command -v npm >/dev/null 2>&1; then
		return 0
	fi
	parkia_die "pnpm/npm not found (install Node under /opt/nodejs or load nvm)"
}

parkia_build_www() {
	local www="$PARKIA_SDK_DIR/app/www"
	[ -d "$www" ] || parkia_die "app/www missing"
	parkia_ensure_node_tools
	echo "Building www..."
	make -C "$www" build
	[ -f "$www/output/index.html" ] || parkia_die "www build failed"
}

# 重编四个自研 app（Buildroot local 包）。
# 先 <pkg>-dirclean 再 <pkg>-rebuild：Buildroot 同步 local 源码用的是
# rsync -au，只增不删，源码里已删除的文件会一直留在 build/<pkg>-<ver>/ 里。
# 2026-09-24 删掉 discovery_app 私有的 libs/network/ 后，编译目录里的旧
# network_manager.h 仍被 #include "libs/network/..." 命中，discovery_app 按
# 新旧两版结构体布局混编，iface_count 读错偏移、设备搜不到。四个 app 均为
# 小体量 C/C++（zlm 为预编译 .so），从空目录编一遍代价可忽略。
parkia_rebuild_app_packages() {
	local out pkg
	local packages=(main_app net_app fab_app discovery_app)
	local clean_targets=() build_targets=()
	out="$(parkia_br_output_dir)" || \
		parkia_die "buildroot output not found (run ./build.sh --rebuild first)"
	for pkg in "${packages[@]}"; do
		clean_targets+=("$pkg-dirclean")
		build_targets+=("$pkg-rebuild")
	done
	echo "Cleaning stale build dirs of ${packages[*]}..."
	make -C "$out" "${clean_targets[@]}"
	echo "Rebuilding ${packages[*]}..."
	ZLM_FORCE="$PARKIA_REBUILD_THIRD_PARTY" make -C "$out" "${build_targets[@]}"
}

# 新鲜检出没有 device/rockchip/.chip 链接时，SDK 列不出 defconfig，
# 后续会退化成交互式 lunch 菜单（CI 无 TTY 必挂，且 init 阶段的失败被
# SDK 以 || true 吞掉、退出码仍为 0）。按 SDK 自己的约定预建该链接。
parkia_ensure_chip_link() {
	local device_dir="$PARKIA_SDK_DIR/device/rockchip"
	[ -e "$device_dir/.chip" ] || \
		ln -rsf "$device_dir/.chips/rk3506" "$device_dir/.chip"
}

# 增量模式：重编 app 后重新生成 rootfs.img 和 update.img
parkia_repack_firmware() {
	[ -f "$PARKIA_SDK_DIR/output/firmware/parameter.txt" ] || \
		parkia_die "output/firmware incomplete (run ./build.sh --rebuild first)"
	parkia_ensure_chip_link
	echo "Selecting board: $(parkia_board_defconfig "$PARKIA_BOOT_MEDIUM")"
	"$PARKIA_SDK_SH" "$(parkia_board_defconfig "$PARKIA_BOOT_MEDIUM")"
	# brmake 是增量的，但 target-finalize 每次都会重跑 → rootfs.img 更新
	echo "Repacking rootfs image..."
	"$PARKIA_SDK_SH" rootfs
	echo "Repacking update.img..."
	"$PARKIA_SDK_SH" firmware
}

# 全量构建：不删已有产物，让各构建系统自己判断增量——
# Buildroot 普通包靠 stamp 跳过、dl 只是下载缓存、kernel/u-boot 原地增量编译；
# 唯一感知不到源码改动的是四个 local 源码 app，显式 <pkg>-rebuild 强制重编。
# 需要从零构建时：./build.sh clean && ./build.sh --rebuild
parkia_run_sdk_full_build() {
	parkia_ensure_chip_link
	echo "Selecting board: $(parkia_board_defconfig "$PARKIA_BOOT_MEDIUM")"
	"$PARKIA_SDK_SH" "$(parkia_board_defconfig "$PARKIA_BOOT_MEDIUM")"
	# 已有构建树时先强制重编 app（无构建树则跳过，由 all 从零编译）
	if parkia_br_output_dir >/dev/null; then
		parkia_rebuild_app_packages
	fi
	echo "Running full SDK build..."
	ZLM_FORCE="$PARKIA_REBUILD_THIRD_PARTY" "$PARKIA_SDK_SH" all
}

parkia_clean_all() {
	echo "Cleaning build outputs..."
	rm -rf "$PARKIA_SDK_DIR/dist" "$PARKIA_SDK_DIR/output" \
		"$PARKIA_SDK_DIR/rockdev" "$PARKIA_SDK_DIR"/buildroot/output \
		"$PARKIA_SDK_DIR/app/www/output" \
		"$PARKIA_SDK_DIR/.vscode/fw_tiny"
	# kernel / u-boot 为原地编译，SDK cleanall 不会清
	[ ! -d "$PARKIA_SDK_DIR/kernel" ] || \
		make -C "$PARKIA_SDK_DIR/kernel" distclean >/dev/null 2>&1 || true
	[ ! -d "$PARKIA_SDK_DIR/u-boot" ] || \
		make -C "$PARKIA_SDK_DIR/u-boot" distclean >/dev/null 2>&1 || true
	echo "Cleaned."
}

# RSA-SHA256 sign net_app → sibling file named "sign".
parkia_sign_net_app() {
	local out net_app sign_file
	[[ -n "$PARKIA_PRIVATE_KEY" ]] || return 0

	out="$(parkia_br_output_dir)" || parkia_die "buildroot target dir not found"
	net_app="$out/target/usr/bin/net_app"
	sign_file="$(dirname "$net_app")/$PARKIA_SIGN_FILENAME"

	[ -f "$PARKIA_PRIVATE_KEY" ] || parkia_die "private key not found: $PARKIA_PRIVATE_KEY"
	command -v openssl >/dev/null 2>&1 || parkia_die "openssl not found (required for -k signing)"
	[ -f "$net_app" ] || parkia_die "net_app missing for signing: $net_app"

	echo "Signing $net_app → $sign_file"
	openssl dgst -sha256 -sign "$PARKIA_PRIVATE_KEY" -out "$sign_file" "$net_app"
	[ -s "$sign_file" ] || parkia_die "empty signature: $sign_file"
	echo "Signed: $sign_file ($(stat -c%s "$sign_file") bytes)"
}

# 打 tiny OTA：四 app（含 fab_app）+ net_app 私有 lib/*.so + www + data.json + 可选 sign。
parkia_pack_tiny() {
	local prefix="$PARKIA_TINY_PREFIX"
	local out target net_app sign_file www_out stage out_bin app so
	local apps=(main_app discovery_app fab_app)
	local private_libdir vendor_libs=(libmk_api.so libIRtekUVCSDK.so libIRtekFileSDK.so)

	out="$(parkia_br_output_dir)" || parkia_die "buildroot target dir not found"
	target="$out/target"
	net_app="$target/usr/bin/net_app"
	sign_file="$(dirname "$net_app")/$PARKIA_SIGN_FILENAME"
	www_out="$PARKIA_SDK_DIR/app/www/output"
	stage="$PARKIA_SDK_DIR/.vscode/fw_tiny/$PARKIA_VERSION"
	out_bin="$PARKIA_DIST_DIR/${prefix}_V${PARKIA_VERSION}.bin"
	private_libdir="$target/usr/lib/net_app"

	[ -d "$www_out" ] || parkia_die "missing $www_out (build www first)"
	[ -f "$net_app" ] || parkia_die "missing $net_app (build first)"

	rm -rf "$stage"
	mkdir -p "$stage" "$stage/lib" "$PARKIA_DIST_DIR"

	cp -p "$net_app" "$stage/net_app"
	if [ -f "$sign_file" ]; then
		cp -p "$sign_file" "$stage/$PARKIA_SIGN_FILENAME"
	fi
	for app in "${apps[@]}"; do
		[ -f "$target/usr/bin/$app" ] || parkia_die "missing $target/usr/bin/$app (build first)"
		cp -p "$target/usr/bin/$app" "$stage/$app"
	done
	# net_app 私有库（与 OTA 解包路径 lib/ 对齐，安装到 /userdata/parkia/app/lib）
	for so in "${vendor_libs[@]}"; do
		if [ -f "$private_libdir/$so" ]; then
			cp -p "$private_libdir/$so" "$stage/lib/$so"
		elif [ -f "$target/usr/lib/$so" ]; then
			# 过渡：旧 rootfs 仍把库放在 /usr/lib
			cp -p "$target/usr/lib/$so" "$stage/lib/$so"
		else
			parkia_die "missing net_app private lib: $so (under $private_libdir)"
		fi
	done
	mkdir -p "$stage/www"
	rsync -a --exclude '*.gz' "$www_out/" "$stage/www/"

	cat >"$stage/data.json" <<EOF
{
  "version": "$PARKIA_VERSION",
  "firmware_name": "$prefix"
}
EOF
	tar -czf "$out_bin" -C "$stage" .
	echo "Generated: $out_bin"
}

parkia_collect_full() {
	local fw="$PARKIA_SDK_DIR/output/firmware"
	mkdir -p "$PARKIA_DIST_DIR"
	if [ -f "$fw/update.img" ]; then
		cp -f "$fw/update.img" "$PARKIA_DIST_DIR/${PARKIA_FULL_PREFIX}_V${PARKIA_VERSION}.img"
		echo "Generated: $PARKIA_DIST_DIR/${PARKIA_FULL_PREFIX}_V${PARKIA_VERSION}.img"
	else
		parkia_die "missing $fw/update.img after SDK build"
	fi
}

# 发布：产物 + sha256 清单，方便发布归档
parkia_write_manifest() {
	( cd "$PARKIA_DIST_DIR" && sha256sum ./*.img ./*.bin > "sha256_V${PARKIA_VERSION}.txt" )
	echo "Generated: $PARKIA_DIST_DIR/sha256_V${PARKIA_VERSION}.txt"
	cat "$PARKIA_DIST_DIR/sha256_V${PARKIA_VERSION}.txt"
}

# 打出分发包（顶层 parkia-cube/，含 dl + buildroot-output）。
parkia_pack_opt() {
	local dl_real out_real dl_base out_base out
	dl_real="$(readlink -f "$PARKIA_SDK_DIR/buildroot/dl")"
	out_real="$(readlink -f "$PARKIA_SDK_DIR/buildroot/output/rockchip_rk3506_board")"
	out="$PARKIA_DIST_DIR/parkia-cube-opt_V${PARKIA_VERSION}.tar.gz"

	[ -d "$dl_real" ] || parkia_die "buildroot/dl missing (build first)"
	[ -d "$out_real" ] || parkia_die "buildroot output missing (build first)"
	# sdk 链接不打进包，由使用者的 build.sh 自行创建
	command -v pigz >/dev/null 2>&1 || parkia_die "pigz not found (sudo apt install pigz)"

	dl_base="$(basename "$dl_real")"
	out_base="$(basename "$out_real")"
	mkdir -p "$PARKIA_DIST_DIR"
	echo "Packing dl + buildroot-output → $out"
	tar -cf - \
		--transform="s|^${dl_base}|parkia-cube/dl|" \
		--transform="s|^${out_base}|parkia-cube/buildroot-output|" \
		-C "$(dirname "$dl_real")" "$dl_base" \
		-C "$(dirname "$out_real")" "$out_base" \
		| pigz -1 > "$out"
	echo "Generated: $out ($(du -h "$out" | cut -f1))"
}

parkia_main() {
	set -euo pipefail

	PARKIA_VERSION="${VERSION:-1.0.0.0}"
	# incremental (默认) | rebuild | clean | publish
	PARKIA_MODE="incremental"
	PARKIA_REBUILD_THIRD_PARTY=0
	PARKIA_PRIVATE_KEY="${PRIVATE_KEY:-}"
	PARKIA_SIGN_FILENAME="sign"
	PARKIA_BOOT_MEDIUM="${RK_BOOT_MEDIUM:-SPI_NAND}"
	PARKIA_FULL_PREFIX="parkia_cube_fw"
	PARKIA_TINY_PREFIX="parkia_cube_tiny_fw"
	PARKIA_DIST_DIR="$PARKIA_SDK_DIR/dist"

	parkia_set_mode() {
		[ "$PARKIA_MODE" = "incremental" ] || parkia_die "use only one of --rebuild / clean / publish / pack-opt / fetch-sdk"
		PARKIA_MODE="$1"
	}

	while [[ $# -gt 0 ]]; do
		case "$1" in
			-h|--help) parkia_usage; exit 0 ;;
			-v|--version)
				PARKIA_VERSION="${2:-}"
				[[ -n "$PARKIA_VERSION" ]] || parkia_die "-v needs a version"
				shift 2
				;;
			--boot-medium)
				[[ -n "${2:-}" && "$2" != -* ]] || parkia_die "--boot-medium needs SPI_NAND|SD_CARD"
				PARKIA_BOOT_MEDIUM="$2"
				shift 2
				;;
			-k|--private-key)
				# Empty / missing value = skip signing (CI may pass -k %private_key_path%)
				if [[ $# -lt 2 || "$2" == -* ]]; then
					PARKIA_PRIVATE_KEY=""
					shift
				else
					PARKIA_PRIVATE_KEY="$2"
					shift 2
				fi
				;;
			--rebuild) parkia_set_mode rebuild; shift ;;
			--rebuild-third-party) PARKIA_REBUILD_THIRD_PARTY=1; shift ;;
			clean) parkia_set_mode clean; shift ;;
			publish) parkia_set_mode publish; shift ;;
			pack-opt) parkia_set_mode pack-opt; shift ;;
			fetch-sdk) parkia_set_mode fetch-sdk; shift ;;
			*) parkia_die "unknown argument: $1 (see --help)" ;;
		esac
	done

	if [[ "$PARKIA_MODE" == "clean" ]]; then
		parkia_clean_all
		exit 0
	fi

	if [[ "$PARKIA_MODE" == "fetch-sdk" ]]; then
		parkia_ensure_sdk force
		parkia_ensure_dl_cache
		exit 0
	fi

	parkia_ensure_sdk
	[ -f "$PARKIA_SDK_SH" ] || parkia_die "SDK build script not found: $PARKIA_SDK_SH"
	parkia_ensure_dl_cache

	if [[ "$PARKIA_MODE" == "pack-opt" ]]; then
		parkia_pack_opt
		exit 0
	fi

	echo "Project: Parkia.Cube"
	echo "SDK:     $PARKIA_SDK_DIR"
	echo "Version: $PARKIA_VERSION"
	echo "Mode:    $PARKIA_MODE"
	echo "Medium:  $PARKIA_BOOT_MEDIUM"
	if [[ -n "$PARKIA_PRIVATE_KEY" ]]; then
		echo "Sign key: $PARKIA_PRIVATE_KEY"
	fi

	case "$PARKIA_MODE" in
		incremental)
			parkia_br_output_dir >/dev/null || \
				parkia_die "no existing build tree; run ./build.sh --rebuild first"
			parkia_build_www
			parkia_rebuild_app_packages
			parkia_repack_firmware
			parkia_sign_net_app
			parkia_collect_full
			parkia_pack_tiny
			;;
	rebuild|publish)
		parkia_build_www
			parkia_run_sdk_full_build
			parkia_sign_net_app
			parkia_collect_full
			parkia_pack_tiny
			[[ "$PARKIA_MODE" == "publish" ]] && parkia_write_manifest
			;;
		*)
			parkia_die "unknown mode: $PARKIA_MODE"
			;;
	esac

	echo "Done."
}

# 项目模式优先匹配；其余参数透传给 Rockchip SDK 构建脚本
case "${1:-}" in
	""|--rebuild|clean|publish|pack-opt|fetch-sdk| \
	-v|--version|--boot-medium|-k|--private-key|--rebuild-third-party| \
	-h|--help)
		parkia_main "$@"
		exit $?
		;;
	*)
		parkia_ensure_sdk
		[ -f "$PARKIA_SDK_SH" ] || parkia_die "SDK build script not found: $PARKIA_SDK_SH"
		parkia_ensure_dl_cache
		exec "$PARKIA_SDK_SH" "$@"
		;;
esac
