cmake_minimum_required(VERSION 3.16)

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets" CACHE PATH "Directory containing assets")
set(target_name lanterndeep)
option(EMULATOR "Save hiscores to H:/ (HostFS) instead of B:/" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Where the .bin goes")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

zos_use(zvb)
include($ENV{ZGDK_PATH}/cmake/zgdk_init.cmake)

project(lantern_deep C ASM)

add_executable(${target_name}
    src/controls.c
    src/main.c
    src/stream.c
    src/stream_shift.c
    src/shaft.c
    src/fog.c
    src/actors.c
    src/hud.c
    src/sfx.c
    src/music.c
    src/assets.c
    src/station.c
)

# Smaller code leaves more RAM for the stack
target_compile_options(${target_name} PRIVATE --opt-code-size)


# For recording videos: a bot plays (src/autopilot.c)
option(AUTOPILOT "Bot plays the game (video recording)" OFF)
if(AUTOPILOT)
    target_sources(${target_name} PRIVATE src/autopilot.c)
    target_compile_definitions(${target_name} PRIVATE AUTOPILOT=1)
endif()
option(DEBUG_FPS "Count game loop frames for tools/hfps.py" OFF)
if(DEBUG_FPS)
    target_compile_definitions(${target_name} PRIVATE DEBUG_FPS=1)
endif()
if(EMULATOR)
    target_compile_definitions(${target_name} PRIVATE EMULATOR=1)
endif()

find_package(ZVB_SDK REQUIRED CONFIG PATHS "$ENV{ZVB_SDK_PATH}/cmake" NO_DEFAULT_PATH)
find_package(ZGDK REQUIRED CONFIG PATHS "$ENV{ZGDK_PATH}/cmake" NO_DEFAULT_PATH)
target_link_libraries(${target_name} PUBLIC zgdk zvb_gfx zvb_sound)

# Native map bytes and spawn metadata are generated from assets/**/*.tmx.
# Tileset GIF and music remain tracked assets; map exports stay in the build tree.
include(cmake/maps.cmake)
include(cmake/assets.cmake)

zos_add_outputs(${target_name})

set_property(TARGET ${target_name} APPEND PROPERTY LINK_DEPENDS "$ENV{ZGDK_PATH}/lib/zgdk.lib")
