cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_TOOLCHAIN_FILE)
	message(FATAL_ERROR
		"main_app must be built via Buildroot cross toolchain.\n"
		"  cd Parkia.Cube && ./build.sh")
endif()

project(main_app C)

set(BLE_APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")


# Shared include dir (provision_shared.h).
if(NOT SHARED_INCLUDE_DIR)
	set(SHARED_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
endif()

set(CMAKE_C_STANDARD 99)

set(MAIN_APP_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")

set(BLE_APP_SOURCES
	main.c
	ble_button.c
	ble_config.c
	ble_provision.c
	ble_wifi.c
	ble/ble.c
	ble/ble_advertiser.c
	led/ble_led.c
	${MAIN_APP_LIBS_DIR}/json/json_util.c
	${MAIN_APP_LIBS_DIR}/json/json_bind.c
	${MAIN_APP_LIBS_DIR}/json/cJSON.c
	${MAIN_APP_LIBS_DIR}/file/file_util.c
	${SHARED_INCLUDE_DIR}/libs/time/time_util.c
	${MAIN_APP_LIBS_DIR}/platform/command.c
	${SHARED_INCLUDE_DIR}/libs/log/log.c
)

add_executable(main_app ${BLE_APP_SOURCES})

# bluez_adv_helper is a separate binary.
add_executable(main_app_ble_adv
	ble/bluez_adv_helper.c
)

target_include_directories(main_app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${MAIN_APP_LIBS_DIR}
	${MAIN_APP_LIBS_DIR}/json
	${SHARED_INCLUDE_DIR}
	${SHARED_INCLUDE_DIR}/libs
)

target_compile_definitions(main_app PRIVATE
	APP_DEFAULT_LOG_PATH="/tmp/main_app.log"
	APP_LOG_NAME="main_app"
	NET_APP_NAME="main_app"
	NET_APP_VERSION="1.0.0.0"
)

find_package(Threads REQUIRED)
target_link_libraries(main_app PRIVATE Threads::Threads m)

# ── rkwifibt ────────────────────────────────────────────────────────
set(RKWIFIBT_INCLUDE_DIR "")
set(RKWIFIBT_LIBRARY "")

if(CMAKE_SYSROOT AND EXISTS "${CMAKE_SYSROOT}/usr/include/RkBle.h")
	set(RKWIFIBT_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include")
	if(EXISTS "${CMAKE_SYSROOT}/usr/lib/librkwifibt.so")
		set(RKWIFIBT_LIBRARY "${CMAKE_SYSROOT}/usr/lib/librkwifibt.so")
	endif()
endif()

if(NOT RKWIFIBT_INCLUDE_DIR AND DEFINED RKWIFIBT_FALLBACK_INCLUDE
   AND EXISTS "${RKWIFIBT_FALLBACK_INCLUDE}/RkBle.h")
	set(RKWIFIBT_INCLUDE_DIR "${RKWIFIBT_FALLBACK_INCLUDE}")
endif()

if(NOT RKWIFIBT_LIBRARY)
	if(DEFINED RKWIFIBT_FALLBACK_LIB_ROOTFS AND EXISTS "${RKWIFIBT_FALLBACK_LIB_ROOTFS}")
		set(RKWIFIBT_LIBRARY "${RKWIFIBT_FALLBACK_LIB_ROOTFS}")
	elseif(DEFINED RKWIFIBT_FALLBACK_LIB AND EXISTS "${RKWIFIBT_FALLBACK_LIB}")
		set(RKWIFIBT_LIBRARY "${RKWIFIBT_FALLBACK_LIB}")
	endif()
endif()

if(NOT RKWIFIBT_INCLUDE_DIR OR NOT RKWIFIBT_LIBRARY)
	message(FATAL_ERROR
		"Failed to locate rkwifibt. Run Parkia.Cube ./build.sh.")
endif()

target_include_directories(main_app PRIVATE ${RKWIFIBT_INCLUDE_DIR})
target_link_libraries(main_app PRIVATE ${RKWIFIBT_LIBRARY})
target_link_options(main_app PRIVATE "-Wl,--allow-shlib-undefined")

# ── dbus (for bluez_adv_helper) ────────────────────────────────────
set(DBUS_INCLUDE_DIR "")
set(DBUS_ARCH_INCLUDE_DIR "")
set(DBUS_LIBRARY "")

foreach(_dbus_sysroot IN ITEMS "${CMAKE_SYSROOT}" "${BR_SYSROOT}")
	if(NOT _dbus_sysroot)
		continue()
	endif()
	if(EXISTS "${_dbus_sysroot}/usr/include/dbus-1.0/dbus/dbus.h" AND
	   EXISTS "${_dbus_sysroot}/usr/lib/dbus-1.0/include/dbus/dbus-arch-deps.h" AND
	   EXISTS "${_dbus_sysroot}/usr/lib/libdbus-1.so")
		set(DBUS_INCLUDE_DIR "${_dbus_sysroot}/usr/include/dbus-1.0")
		set(DBUS_ARCH_INCLUDE_DIR "${_dbus_sysroot}/usr/lib/dbus-1.0/include")
		set(DBUS_LIBRARY "${_dbus_sysroot}/usr/lib/libdbus-1.so")
		break()
	endif()
endforeach()

if(NOT DBUS_INCLUDE_DIR OR NOT DBUS_ARCH_INCLUDE_DIR OR NOT DBUS_LIBRARY)
	message(FATAL_ERROR
		"Failed to locate libdbus-1. Run Parkia.Cube ./build.sh.")
endif()

target_compile_definitions(main_app_ble_adv PRIVATE _GNU_SOURCE)
target_include_directories(main_app_ble_adv PRIVATE
	${DBUS_INCLUDE_DIR}
	${DBUS_ARCH_INCLUDE_DIR}
)
target_link_libraries(main_app_ble_adv PRIVATE ${DBUS_LIBRARY})
target_link_options(main_app_ble_adv PRIVATE "-Wl,--allow-shlib-undefined")

install(TARGETS main_app RUNTIME DESTINATION bin)
install(TARGETS main_app_ble_adv RUNTIME DESTINATION libexec/main_app)
