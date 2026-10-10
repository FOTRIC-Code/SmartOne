cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_TOOLCHAIN_FILE)
	message(FATAL_ERROR
		"net_app must be built via Buildroot cross toolchain.\n"
		"  cd Parkia.Cube && ./build.sh")
endif()

project(net_app C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_SHARED_ROOT "" CACHE PATH "Shared application source root")
if(NOT APP_SHARED_ROOT)
  set(APP_SHARED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
endif()
get_filename_component(APP_SHARED_ROOT "${APP_SHARED_ROOT}" ABSOLUTE)
set(APP_SHARED_LIBS_ROOT "${APP_SHARED_ROOT}/libs")

# Optional SDK root from Buildroot / CI (kept for local tree convenience).
set(NET_APP_SDK_DIR "" CACHE PATH "Rockchip SDK root (Buildroot sets this)")
if(NOT NET_APP_SDK_DIR)
	set(_net_app_sibling_sdk "${CMAKE_CURRENT_SOURCE_DIR}/../../../SDK")
	if(EXISTS "${_net_app_sibling_sdk}")
		set(NET_APP_SDK_DIR "${_net_app_sibling_sdk}" CACHE PATH "Rockchip SDK root" FORCE)
	endif()
	unset(_net_app_sibling_sdk)
endif()
if(NET_APP_SDK_DIR)
	get_filename_component(NET_APP_SDK_DIR "${NET_APP_SDK_DIR}" ABSOLUTE)
endif()

set(IRTEK_THERMAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/irtek")
set(IRTEK_THERMAL_INCLUDE_ROOT "${IRTEK_THERMAL_ROOT}/include")
set(IRTEK_THERMAL_ARMHF_LIB_ROOT "${IRTEK_THERMAL_ROOT}/lib")
set(IRTEK_UVC_LIBRARY "${IRTEK_THERMAL_ARMHF_LIB_ROOT}/libIRtekUVCSDK.so")
set(IRTEK_FILE_LIBRARY "${IRTEK_THERMAL_ARMHF_LIB_ROOT}/libIRtekFileSDK.so")

set(NET_APP_DEFAULT_THERMAL_UVC OFF)
if(CMAKE_CROSSCOMPILING OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|armv7|armhf|aarch64)")
  set(NET_APP_DEFAULT_THERMAL_UVC ON)
endif()
option(NET_APP_ENABLE_THERMAL_UVC "Enable the external thermal UVC bridge" ${NET_APP_DEFAULT_THERMAL_UVC})

set(NET_APP_SOURCES
  main.c
  net_common/net_common.c
  main_process/main_process.c
  main_process/main_process_tasks.c
  main_process/alarm_transfer.c
  http_api/http_api.c
  http_api/http_api_common.c
  http_api/http_dto.c
  http_api/http_auth.c
  http_api/http_authz.c
  http_api/http_api_system.c
  http_api/http_api_alarm.c
  http_api/http_api_thermal.c
  http_api/http_api_transfer.c
  http_api/http_api_streaming.c
  http_api/config/config_manager.c
  rtp/rtp_server.c
  rtp/rtp_wire.c
  alarm/alarm_monitor.c
  alarm/intrusion_motion.c
  alarm/body_temp_history.c
  alarm/photo_album.c
  thermal/thermal_data_provider.c
  led/led_manager.c
  libs/binary/binary_writer.c
  libs/ftp/ftp_client.c
  libs/rtc/rtc_util.c
  libs/ntp/ntp_client.c
  libs/unit/unit_convert.c
  ${APP_SHARED_LIBS_ROOT}/json/json_util.c
  ${APP_SHARED_LIBS_ROOT}/json/json_bind.c
  ${APP_SHARED_LIBS_ROOT}/json/cJSON.c
  ${APP_SHARED_LIBS_ROOT}/file/file_util.c
  ${APP_SHARED_LIBS_ROOT}/network/network_manager.c
  ${APP_SHARED_LIBS_ROOT}/time/time_util.c
  ${APP_SHARED_LIBS_ROOT}/platform/command.c
  ${APP_SHARED_LIBS_ROOT}/log/log.c
  ${APP_SHARED_LIBS_ROOT}/upgrade/app_slot_upgrade.c
  ${APP_SHARED_LIBS_ROOT}/mongoose/mongoose.c
)

if(NET_APP_ENABLE_THERMAL_UVC)
  list(APPEND NET_APP_SOURCES
    thermal/thermal_uvc_bridge.cpp
    libs/irtek/irtek_file_sdk_loader.cpp
  )
else()
  list(APPEND NET_APP_SOURCES thermal/thermal_uvc_stub.c)
endif()

list(APPEND NET_APP_SOURCES
  video_process/video_process.c
  video_process/image_convert.c
  video_process/stills_mp4.c
  video_process/zlm/zlm_server.c
  video_process/zlm/zlm_encode.c
  video_process/zlm/h264_encoder.c
)
set_source_files_properties(video_process/zlm/h264_encoder.c PROPERTIES
  COMPILE_FLAGS "-Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable")

add_executable(net_app ${NET_APP_SOURCES})

target_include_directories(net_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libs
  ${APP_SHARED_ROOT}
  ${APP_SHARED_LIBS_ROOT}
  ${APP_SHARED_LIBS_ROOT}/json
)

if(NET_APP_ENABLE_THERMAL_UVC)
  if(NOT EXISTS "${IRTEK_THERMAL_INCLUDE_ROOT}/IRtekUVCSDK/IRtekUVCSDK.h")
    message(FATAL_ERROR "Missing IRtekUVCSDK headers under ${IRTEK_THERMAL_INCLUDE_ROOT}")
  endif()
  if(NOT EXISTS "${IRTEK_THERMAL_INCLUDE_ROOT}/IRtekFileSDK/IRtekFileSDK.h")
    message(FATAL_ERROR "Missing IRtekFileSDK headers under ${IRTEK_THERMAL_INCLUDE_ROOT}")
  endif()
  if(NOT EXISTS "${IRTEK_UVC_LIBRARY}")
    message(FATAL_ERROR "Missing libIRtekUVCSDK.so under ${IRTEK_THERMAL_ARMHF_LIB_ROOT}")
  endif()
  if(NOT EXISTS "${IRTEK_FILE_LIBRARY}")
    message(FATAL_ERROR "Missing libIRtekFileSDK.so under ${IRTEK_THERMAL_ARMHF_LIB_ROOT}")
  endif()

  target_include_directories(net_app PRIVATE
    "${IRTEK_THERMAL_INCLUDE_ROOT}"
    "${IRTEK_THERMAL_INCLUDE_ROOT}/IRtekUVCSDK"
  )

  add_library(IRtekUVCSDK SHARED IMPORTED GLOBAL)
  set_target_properties(IRtekUVCSDK PROPERTIES
    IMPORTED_LOCATION "${IRTEK_UVC_LIBRARY}"
  )
endif()

# mongoose appends at most MG_IO_SIZE bytes per mg_mgr_poll(), and the main
# loop polls once every ~10 ms, so this value is the static-file throughput
# ceiling (2048 B → ~200 KB/s, measured). 16 KB keeps the SoftAP page load
# bound by the radio instead of by the poll cadence.
# MG_MAX_RECV_SIZE: multipart OTA body is buffered in-memory; default 3MB
# truncates ~5MB tiny packages — raise to 16MB.
target_compile_definitions(net_app PRIVATE
  _GNU_SOURCE
  APP_DEFAULT_LOG_PATH="/tmp/net_app.log"
  APP_LOG_NAME="net_app"
  MG_ENABLE_PACKED_FS=0
  MG_IO_SIZE=16384
  MG_MAX_RECV_SIZE=16777216
)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
target_link_libraries(net_app PRIVATE Threads::Threads)
target_include_directories(net_app PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(net_app PRIVATE ${CURL_LIBRARIES})

# Math may be needed by mongoose on some toolchains
target_link_libraries(net_app PRIVATE m)

# Vendored runtime .so stay under the app (lib/net_app/), not system /usr/lib.
# Prefer OTA slot lib/ (no S14 bind needed), then factory private dir.
set(NET_APP_PRIVATE_LIBDIR "lib/net_app")
set_target_properties(net_app PROPERTIES
  INSTALL_RPATH "/userdata/parkia/app/lib:/usr/lib/net_app"
  BUILD_WITH_INSTALL_RPATH TRUE
)

if(NET_APP_ENABLE_THERMAL_UVC)
  target_link_libraries(net_app PRIVATE IRtekUVCSDK dl)
  install(FILES
    "${IRTEK_UVC_LIBRARY}"
    "${IRTEK_FILE_LIBRARY}"
    DESTINATION "${NET_APP_PRIVATE_LIBDIR}"
  )
endif()

set(ZLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/zlm")
set(ZLM_INCLUDE_DIR "${ZLM_ROOT}/include")
set(ZLM_LIBRARY "${ZLM_ROOT}/libmk_api.so")

if(NOT EXISTS "${ZLM_INCLUDE_DIR}/mk_mediakit.h")
  message(FATAL_ERROR
    "Missing offline ZLMediaKit headers under ${ZLM_INCLUDE_DIR}.")
endif()
if(NOT EXISTS "${ZLM_LIBRARY}")
  message(FATAL_ERROR
    "Missing offline ZLMediaKit library: ${ZLM_LIBRARY}. "
    "Use the checked-in library or rebuild with ./app/_legacy/build_zlm.sh.")
endif()

add_library(ZLM::MediaKit SHARED IMPORTED GLOBAL)
set_target_properties(ZLM::MediaKit PROPERTIES
  IMPORTED_LOCATION "${ZLM_LIBRARY}")

set(NET_APP_SSL_LIBRARY "")
set(NET_APP_CRYPTO_LIBRARY "")
set(NET_APP_Z_LIBRARY "")
if(CMAKE_SYSROOT AND EXISTS "${CMAKE_SYSROOT}/usr/lib/libssl.so")
  set(NET_APP_SSL_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libssl.so")
  set(NET_APP_CRYPTO_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libcrypto.so")
  if(EXISTS "${CMAKE_SYSROOT}/usr/lib/libz.so")
    set(NET_APP_Z_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libz.so")
  endif()
endif()

if(NOT NET_APP_SSL_LIBRARY)
  find_library(NET_APP_SSL_LIBRARY NAMES ssl HINTS "${CMAKE_SYSROOT}/usr/lib")
endif()
if(NOT NET_APP_CRYPTO_LIBRARY)
  find_library(NET_APP_CRYPTO_LIBRARY NAMES crypto HINTS "${CMAKE_SYSROOT}/usr/lib")
endif()
if(NOT NET_APP_Z_LIBRARY)
  find_library(NET_APP_Z_LIBRARY NAMES z HINTS "${CMAKE_SYSROOT}/usr/lib")
endif()

# libjpeg (buildroot package jpeg) powers the alarm FTP JPEG snapshot.
set(NET_APP_JPEG_LIBRARY "")
if(CMAKE_SYSROOT AND EXISTS "${CMAKE_SYSROOT}/usr/lib/libjpeg.so")
  set(NET_APP_JPEG_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libjpeg.so")
endif()
if(NOT NET_APP_JPEG_LIBRARY)
  find_library(NET_APP_JPEG_LIBRARY NAMES jpeg HINTS "${CMAKE_SYSROOT}/usr/lib")
endif()
if(NOT NET_APP_JPEG_LIBRARY)
  message(FATAL_ERROR "net_app needs libjpeg from the buildroot jpeg package")
endif()

if(NOT NET_APP_SSL_LIBRARY OR NOT NET_APP_CRYPTO_LIBRARY)
  message(FATAL_ERROR "ZLMediaKit needs OpenSSL (libssl and libcrypto)")
endif()

target_include_directories(net_app PRIVATE "${ZLM_INCLUDE_DIR}")
target_link_libraries(net_app PRIVATE
  ZLM::MediaKit
  ${NET_APP_SSL_LIBRARY}
  ${NET_APP_CRYPTO_LIBRARY}
  ${NET_APP_JPEG_LIBRARY}
  stdc++
  atomic
  pthread
  dl
)
if(NET_APP_Z_LIBRARY)
  target_link_libraries(net_app PRIVATE ${NET_APP_Z_LIBRARY})
endif()

install(TARGETS net_app RUNTIME DESTINATION bin)
install(FILES "${ZLM_LIBRARY}" DESTINATION "${NET_APP_PRIVATE_LIBDIR}")
