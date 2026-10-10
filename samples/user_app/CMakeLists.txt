cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_TOOLCHAIN_FILE)
	message(FATAL_ERROR
		"user_app sample must be built with the Buildroot SDK.\n"
		"Use tools/developer-kit/build_user_app.sh or pass CMAKE_TOOLCHAIN_FILE.")
endif()

project(user_app C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(user_app main.c)
target_compile_definitions(user_app PRIVATE APP_LOG_PATH="/tmp/user_app.log")

install(TARGETS user_app RUNTIME DESTINATION bin)
