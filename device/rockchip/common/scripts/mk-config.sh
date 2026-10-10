#!/bin/bash -e

switch_defconfig()
{
	DEFCONFIG="$1"

	[ -f "$DEFCONFIG" ] || DEFCONFIG="$RK_CHIP_DIR/$DEFCONFIG"

	if [ ! -f "$DEFCONFIG" ]; then
		error "No such defconfig: $1"
		exit 1
	fi

	notice "Switching to defconfig: $DEFCONFIG"
	rm -f "$RK_DEFCONFIG_LINK"
	ln -rsf "$DEFCONFIG" "$RK_DEFCONFIG_LINK"

	DEFCONFIG="$(realpath "$DEFCONFIG")"
	rm -rf "$RK_CHIP_DIR"
	ln -rsf "$(dirname "$DEFCONFIG")" "$RK_CHIP_DIR"

	make $(basename "$DEFCONFIG")
}

rockchip_defconfigs()
{
	cd "$RK_CHIP_DIR"
	ls rockchip_defconfig 2>/dev/null || true
	ls *_defconfig | grep -v rockchip_defconfig || true
}

__IS_IN_ARRAY() {
	local value="$1"
	shift
	for item in "$@"; do
		if [[ "$item" == "$value" ]]; then
			return 0
		fi
	done
	return 1
}

choose_defconfig()
{
	DEFCONFIG_ARRAY=( $(rockchip_defconfigs | grep "$1" || true) )

	DEFCONFIG_ARRAY_LEN=${#DEFCONFIG_ARRAY[@]}

	case $DEFCONFIG_ARRAY_LEN in
		0)
			error "No available defconfigs${1:+" for: $1"}"
			return 1
			;;
		1)	DEFCONFIG=${DEFCONFIG_ARRAY[0]} ;;
		*)
			local LF_HARDWARE=(
				"rk3506b_smart_one")

			local LF_BOOT_MEDIA=("sdmmc" "spinand" "emmc")
			local LF_SYSTEM=("buildroot")
			local cnt=0 space8="        "

			local LUNCH_NUM=0
			local HW_INDEX
			echo "You're building on Linux"
			echo -e "${C_GREEN} "${space8}Lunch menu...pick the Parkia.Cube hardware version:"${C_NORMAL}"
			echo -e "${C_GREEN} "${space8}选择 Parkia.Cube 硬件版本:"${C_NORMAL}"

			echo "${space8}${space8}[${LUNCH_NUM}] Parkia_Cube"
			LUNCH_NUM=$((LUNCH_NUM + 1))
			echo "${space8}${space8}[${LUNCH_NUM}] custom"

			read -p "Which would you like? [0~${LUNCH_NUM}][default:0]: " HW_INDEX

			if [ -z "$HW_INDEX" ]; then
				HW_INDEX=0
			fi

			if ! [[ "$HW_INDEX" =~ ^[0-9]+$ ]]; then
				msg_error "Error: HW_INDEX is not a number."
				exit 1
			else
				if (($HW_INDEX < 0 || $HW_INDEX > $LUNCH_NUM)); then
					msg_error "Error: HW_INDEX is not in the range 0-$LUNCH_NUM."
					exit 1
				elif [ $HW_INDEX == $LUNCH_NUM ]; then
					if [ "$1" = ${DEFCONFIG_ARRAY[0]} ]; then
						# Prefer exact-match
						DEFCONFIG="$1"
					else
						message "Pick a defconfig:\n"

						echo ${DEFCONFIG_ARRAY[@]} | xargs -n 1 | \
							sed "=" | sed "N;s/\n/. /"

						local INDEX
						read -p "Which would you like? [1]: " INDEX
						INDEX=$((${INDEX:-1} - 1))
						DEFCONFIG="${DEFCONFIG_ARRAY[$INDEX]}"
						switch_defconfig $DEFCONFIG
						return
					fi
				fi
			fi

			# Get Boot Medium Version
			local BM_INDEX MAX_BM_INDEX
			echo -e "${C_GREEN} "${space8}Lunch menu...pick the boot medium:"${C_NORMAL}"
			echo -e "${C_GREEN} "${space8}选择启动媒介:"${C_NORMAL}"

			range_sd_card_spi_nand=(0)
			range_emmc=()
			range_sd_card_emmc=()

			if __IS_IN_ARRAY "$HW_INDEX" "${range_sd_card_spi_nand[@]}"; then
				echo "${space8}${space8}[0] SD_CARD"
				echo "${space8}${space8}[1] SPI_NAND"
				read -p "Which would you like? [0~1][default:0]: " BM_INDEX
				MAX_BM_INDEX=1
			elif __IS_IN_ARRAY "$HW_INDEX" "${range_emmc[@]}"; then
				echo "${space8}${space8}[0] EMMC"
				read -p "Which would you like? [0][default:0]: " BM_INDEX
				MAX_BM_INDEX=0
			elif __IS_IN_ARRAY "$HW_INDEX" "${range_sd_card_emmc[@]}"; then
				echo "${space8}${space8}[0] SD_CARD"
				echo "${space8}${space8}[1] EMMC"
				read -p "Which would you like? [0~1][default:0]: " BM_INDEX
				MAX_BM_INDEX=1
			else
				echo "Invalid HW_INDEX: $HW_INDEX"
				exit 1
			fi

			# Default is 0
			if [ -z "$BM_INDEX" ]; then
				BM_INDEX=0
			fi

			if ! [[ "$BM_INDEX" =~ ^[0-9]+$ ]]; then
				msg_error "Error: BM_INDEX is not a number."
				exit 1
			else
				if (($BM_INDEX < 0 || $BM_INDEX > $MAX_BM_INDEX)); then
					msg_error "Error: BM_INDEX is not in the range ."
					exit 1
				fi
			fi

			# Get System Version
			local SYS_INDEX MAX_SYS_INDEX
			echo -e "${C_GREEN} "${space8}Lunch menu...pick the system version:"${C_NORMAL}"
			echo -e "${C_GREEN} "${space8}选择系统版本:"${C_NORMAL}"

			# Just support buildroot
			echo "${space8}${space8}[0] Buildroot "
			read -p "Which would you like? [0~1][default:0]: " SYS_INDEX
			MAX_SYS_INDEX=0

			if [ -z "$SYS_INDEX" ]; then
				SYS_INDEX=0
			fi

			if ! [[ "$SYS_INDEX" =~ ^[0-9]+$ ]]; then
				msg_error "Error: SYS_INDEX is not a number."
				exit 1
			else
				if (($SYS_INDEX < 0 || $SYS_INDEX > $MAX_SYS_INDEX)); then
					msg_error "Error: SYS_INDEX is not in the range 0-1."
					exit 1
				fi
			fi

			# EMMC
			if __IS_IN_ARRAY "$HW_INDEX" "${range_emmc[@]}"; then
				BM_INDEX=$((BM_INDEX + 2)) #EMMC
			elif __IS_IN_ARRAY "$HW_INDEX" "${range_sd_card_emmc[@]}" && [ "$BM_INDEX" == 1 ]; then
				BM_INDEX=2 #EMMC
			fi

			DEFCONFIG="${LF_HARDWARE[$HW_INDEX]}_${LF_SYSTEM[$SYS_INDEX]}_${LF_BOOT_MEDIA[$BM_INDEX]}_defconfig"
			;;
	esac

	switch_defconfig $DEFCONFIG
}

choose_chip()
{
	CHIP_ARRAY=( $(ls "$RK_CHIPS_DIR" | grep "$1" || true) )
	CHIP_ARRAY_LEN=${#CHIP_ARRAY[@]}

	case $CHIP_ARRAY_LEN in
		0)
			error "No available chips${1:+" for: $1"}"
			return 1
			;;
		1)	CHIP=${CHIP_ARRAY[0]} ;;
		*)
			if [ "$1" = ${CHIP_ARRAY[0]} ]; then
				# Prefer exact-match
				CHIP="$1"
			else
				message "Pick a chip:\n"

				echo ${CHIP_ARRAY[@]} | xargs -n 1 | sed "=" | \
					sed "N;s/\n/. /"

				local INDEX
				read -p "Which would you like? [1]: " INDEX
				INDEX=$((${INDEX:-1} - 1))
				CHIP="${CHIP_ARRAY[$INDEX]}"
			fi
			;;
	esac

	notice "Switching to chip: $CHIP"
	rm -rf "$RK_CHIP_DIR"
	ln -rsf "$RK_CHIPS_DIR/$CHIP" "$RK_CHIP_DIR"

	choose_defconfig $2
}

prepare_config()
{
	[ -e "$RK_CHIP_DIR" ] || choose_chip

	cd "$RK_DEVICE_DIR"
	rm -f $(ls "$RK_CHIPS_DIR")
	ln -rsf "$(readlink "$RK_CHIP_DIR")" .
	cd "$RK_SDK_DIR"

	if [ ! -r "$RK_DEFCONFIG_LINK" ]; then
		warning "WARN: $RK_DEFCONFIG_LINK not exists"
		choose_defconfig
		return 0
	fi

	DEFCONFIG=$(basename "$(realpath "$RK_DEFCONFIG_LINK")")
	if [ ! "$RK_DEFCONFIG_LINK" -ef "$RK_CHIP_DIR/$DEFCONFIG" ]; then
		warning "WARN: $RK_DEFCONFIG_LINK is invalid"
		choose_defconfig
		return 0
	fi

	if [ "$RK_CONFIG" -ot "$RK_DEFCONFIG_LINK" ]; then
		warning "WARN: $RK_CONFIG is out-dated"
		make $DEFCONFIG
		return 0
	fi

	CONFIG_DIR="$(dirname "$RK_CONFIG_IN")"
	if [ "$(find "$CONFIG_DIR" -cnewer "$RK_CONFIG")" ]; then
		warning "WARN: $CONFIG_DIR is updated"
		make $DEFCONFIG
		return 0
	fi

	CFG="RK_DEFCONFIG=\"$DEFCONFIG\""
	if ! grep -wq "$CFG" "$RK_CONFIG"; then
		warning "WARN: $RK_CONFIG is invalid"
		make $DEFCONFIG
		return 0
	fi

	if [ "$RK_CONFIG" -nt "${RK_CONFIG}.old" ]; then
		make olddefconfig >/dev/null
		touch "${RK_CONFIG}.old"
	fi
}

# Hooks

usage_hook()
{
	usage_oneline "chip[:<chip>[:<config>]]" "choose chip"
	usage_oneline "defconfig[:<config>]" "choose defconfig"
	usage_oneline " *_defconfig" "switch to specified defconfig"
	echo "    available defconfigs:"
	ls "$RK_CHIP_DIR/" | grep "defconfig$" | sed "s/^/    /"
	usage_oneline " olddefconfig" "resolve any unresolved symbols in .config"
	usage_oneline " savedefconfig" "save current config to defconfig"
	usage_oneline " menuconfig" "interactive curses-based configurator"
	usage_oneline "config" "modify SDK defconfig"
}

clean_hook()
{
	rm -rf "$RK_OUTDIR"/*config* "$RK_OUTDIR/kconf"
}

INIT_CMDS="chip defconfig lunch [^:]*_defconfig olddefconfig savedefconfig menuconfig config default"
init_hook()
{
	case "${1:-default}" in
		chip) shift; choose_chip $@ ;;
		lunch|defconfig) shift; choose_defconfig $@ ;;
		*_defconfig) switch_defconfig "$1" ;;
		olddefconfig | savedefconfig | menuconfig)
			prepare_config
			make $1
			;;
		config)
			prepare_config
			make menuconfig
			make savedefconfig
			;;
		default) prepare_config ;; # End of init
		*) usage ;;
	esac
}

source "${RK_BUILD_HELPER:-$(dirname "$(realpath "$0")")/../build-hooks/build-helper}"

init_hook $@
