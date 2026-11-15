cmake_minimum_required(VERSION 3.13)

project(Klaxon-ActionMax C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $ENV{DEVKITPRO}/wut/include
    $ENV{DEVKITPRO}/portlibs/wiiu/include
    $ENV{DEVKITPRO}/portlibs/wiiu/include/SDL2
)

link_directories(
    $ENV{DEVKITPRO}/wut/lib
    $ENV{DEVKITPRO}/portlibs/wiiu/lib
)

set(SOURCES
    src/main.cpp
    src/app.cpp
    src/hud.cpp
    src/tape_player.cpp
    src/input.cpp
    src/game_registry.cpp
    src/hit_detector.cpp
    src/sha256.cpp
    src/common.cpp
)

add_executable(Klaxon-ActionMax ${SOURCES})
set_target_properties(Klaxon-ActionMax PROPERTIES SUFFIX ".elf")

target_link_libraries(Klaxon-ActionMax
    SDL2_mixer
    SDL2_image
    SDL2_ttf
    SDL2
    # SDL2_mixer deps
    modplug
    mpg123
    vorbisfile
    vorbis
    ogg
    # SDL2_ttf deps
    harfbuzz
    freetype
    # SDL2_image deps
    webp
    jpeg
    # Leaf deps (must come last, in dependency order)
    png
    brotlidec
    brotlicommon
    bz2
    z
    # System
    wut
    m
)

# ---------------------------------------------------------------------------
# Manual ELF -> RPX -> WUHB pipeline (proven on this machine)
# ---------------------------------------------------------------------------

set(RPX_FILE  ${CMAKE_CURRENT_BINARY_DIR}/Klaxon-ActionMax.rpx)
set(WUHB_FILE ${CMAKE_CURRENT_BINARY_DIR}/Klaxon-ActionMax.wuhb)

find_program(ELF2RPL_EXE elf2rpl
    HINTS
        $ENV{DEVKITPRO}/wut/bin
        $ENV{DEVKITPRO}/tools/bin
)

find_program(WUHBTool_EXE wuhbtool
    HINTS
        $ENV{DEVKITPRO}/tools/bin
        $ENV{DEVKITPRO}/wut/bin
)

if(NOT ELF2RPL_EXE)
    message(FATAL_ERROR "elf2rpl not found (look in devkitPro wut/bin)")
endif()
if(NOT WUHBTool_EXE)
    message(FATAL_ERROR "wuhbtool not found (look in devkitPro tools/bin)")
endif()

message(STATUS "elf2rpl  : ${ELF2RPL_EXE}")
message(STATUS "wuhbtool : ${WUHBTool_EXE}")

# Step 1: ELF -> RPX
add_custom_command(TARGET Klaxon-ActionMax POST_BUILD
    COMMAND ${ELF2RPL_EXE}
        $<TARGET_FILE:Klaxon-ActionMax>
        ${RPX_FILE}
    COMMENT "elf2rpl: ELF -> RPX"
    VERBATIM
)

# Step 2: RPX + romfs -> WUHB
# Branding assets (icon / banners / boot splash / boot sound) are wired in
# here once the final art is produced; --icon currently uses the classic
# Action Max sprite as a placeholder.
add_custom_command(TARGET Klaxon-ActionMax POST_BUILD
    COMMAND ${WUHBTool_EXE}
        ${RPX_FILE}
        ${WUHB_FILE}
        --content=${CMAKE_CURRENT_SOURCE_DIR}/romfs
        --name="Klaxon Action Max"
        --short-name="ActionMax"
        --author="Kappyscope"
        --icon=${CMAKE_CURRENT_SOURCE_DIR}/romfs/assets/sprites/sprite_actionmax.png
    COMMENT "wuhbtool: RPX + romfs -> WUHB"
    VERBATIM
)
