cmake_minimum_required(VERSION 3.22)
project(ForsanRaneaf C)
set(CMAKE_C_STANDARD 17)

option(RPI "Building for RPI ARM64" OFF)
option(LINUX_AMD64 "Building for Linux AMD64" OFF)

include_directories(${CMAKE_SOURCE_DIR}/Infotainment/lib/raylib/include)
include_directories(${CMAKE_SOURCE_DIR}/Infotainment/lib/raygui/include)

add_library(utilities OBJECT
        utilities/mem_manager/mem_pool.h
        utilities/mem_manager.h
        utilities/mem_manager/mem_manager.c
        utilities/mem_manager/mem_pool.c
        utilities/ini_config.h
        utilities/ini_config/ini_config.c
        utilities/defines.h
        utilities/hashmap.h
        utilities/types/hashmap.c
        utilities/logger.h
        utilities/logger/logger.c
        utilities/strutils.h
        utilities/strutils/strutils.c
        utilities/threading.h
        utilities/threading/threading.c
        utilities/timing.h
        utilities/timing/timing.c
        utilities/serial.h
        utilities/serial/serial.c
)

add_executable(frinfo
        main.c
        frinfo/frinfo.h
        frinfo/frinfo.c
        frinfo/serial_comm.h
        frinfo/serial_comm.c
        frinfo/ui.h
        frinfo/ui.c
)

if (RPI)
	target_compile_definitions(frinfo PRIVATE RPI)
	target_link_directories(frinfo PRIVATE ${CMAKE_SOURCE_DIR}/Infotainment/lib/raylib/arm64)
	target_link_libraries(frinfo
			utilities
			raylib
			m
			drm
			GLESv2
			gbm
			EGL
	)
else ()
	target_link_directories(frinfo PRIVATE ${CMAKE_SOURCE_DIR}/Infotainment/lib/raylib/amd64)
	target_link_libraries(frinfo
			utilities
			raylib
			m
	)
endif ()

configure_file(config.ini config.ini COPYONLY)
