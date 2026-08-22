cmake_minimum_required(VERSION 3.16)
project(rayrender C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(RAYRENDER_GPU "Build against OpenGL 3.3 instead of the software backend" OFF)
option(RAYRENDER_SIMD "Enable rlsw SIMD intrinsics (NEON/SSE/AVX)" ON)
option(RAYRENDER_STOCK_RLSW "Overlay stock rlsw onto rayrender only (no pair)" OFF)

# rlsw-cc: prefer ./rlsw-cc (dev / submodule); else FetchContent from GitHub.
option(RAYRENDER_RLSW_CC_LOCAL "Use ${CMAKE_SOURCE_DIR}/rlsw-cc when present" ON)
set(RAYRENDER_RLSW_CC_GIT "https://github.com/sreekotay/rlsw-cc.git" CACHE STRING
    "Git URL for rlsw-cc (used when no local ./rlsw-cc tree)")
set(RAYRENDER_RLSW_CC_TAG "main" CACHE STRING "Git tag or branch for rlsw-cc FetchContent")

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(RAYRENDER_GPU)
    set(PLATFORM "Desktop" CACHE STRING "raylib platform" FORCE)
    set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)
else()
    # GLFW never copies the rlsw framebuffer; RGFW blits it (including macOS BGRA).
    set(PLATFORM "RGFW" CACHE STRING "raylib platform" FORCE)
    set(OPENGL_VERSION "Software" CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(raylib)

set(RAYRENDER_SOURCES
    src/main.c
    src/light.c
    src/procgen.c
    src/scenes.c
)

function(rayrender_configure_app name)
    target_include_directories(${name} PRIVATE src)
    target_compile_definitions(${name} PRIVATE
        $<$<BOOL:${RAYRENDER_GPU}>:RAYRENDER_GPU>
    )
    if(APPLE AND NOT RAYRENDER_GPU)
        target_link_libraries(${name} PRIVATE
            "-framework IOKit"
            "-framework Cocoa"
            "-framework CoreFoundation"
            "-framework CoreVideo"
        )
    elseif(UNIX AND NOT APPLE AND NOT RAYRENDER_GPU)
        # raylib 6.0 RGFW: XRR* refs; stock referee also needs -lm.
        target_link_libraries(${name} PRIVATE Xrandr m)
    endif()
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Bring in rlsw-cc (local tree or FetchContent)
# -----------------------------------------------------------------------------
function(rayrender_add_rlsw_cc)
    set(_local "${CMAKE_SOURCE_DIR}/rlsw-cc")
    if(RAYRENDER_RLSW_CC_LOCAL AND EXISTS "${_local}/CMakeLists.txt")
        message(STATUS "rayrender: rlsw-cc from ${_local}")
        add_subdirectory(rlsw-cc)
        return()
    endif()
    if(RAYRENDER_RLSW_CC_GIT STREQUAL "")
        message(FATAL_ERROR
            "rayrender: no rlsw-cc.\n"
            "  Dev: keep a checkout at ${_local}\n"
            "  Or:  cmake -DRAYRENDER_RLSW_CC_GIT=https://github.com/sreekotay/rlsw-cc.git …")
    endif()
    message(STATUS "rayrender: FetchContent rlsw-cc ${RAYRENDER_RLSW_CC_GIT} @ ${RAYRENDER_RLSW_CC_TAG}")
    FetchContent_Declare(
        rlsw_cc_src
        GIT_REPOSITORY "${RAYRENDER_RLSW_CC_GIT}"
        GIT_TAG "${RAYRENDER_RLSW_CC_TAG}"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(rlsw_cc_src)
endfunction()

if(RAYRENDER_GPU)
    add_executable(rayrender ${RAYRENDER_SOURCES})
    target_link_libraries(rayrender PRIVATE raylib)
    target_compile_definitions(rayrender PRIVATE RAYRENDER_IMPL_GPU)
    rayrender_configure_app(rayrender)
else()
    if(RAYRENDER_STOCK_RLSW)
        set(RLSW_CC_USE_STOCK ON CACHE BOOL "" FORCE)
        set(RLSW_CC_PARALLEL OFF CACHE BOOL "" FORCE)
    endif()

    rayrender_add_rlsw_cc()
    rlsw_cc_overlay_raylib(raylib "${raylib_SOURCE_DIR}")

    if(RAYRENDER_SIMD)
        target_compile_definitions(raylib PRIVATE RLSW_USE_SIMD_INTRINSICS)
    endif()

    add_executable(rayrender ${RAYRENDER_SOURCES})
    target_link_libraries(rayrender PRIVATE raylib)
    if(TARGET rlsw_cc AND NOT RLSW_CC_USE_STOCK)
        # Do not attach rlsw_cc to raylib — FetchContent install(EXPORT) rejects unknown deps.
        target_link_libraries(rayrender PRIVATE rlsw_cc)
    endif()
    if(RAYRENDER_STOCK_RLSW)
        target_compile_definitions(rayrender PRIVATE RAYRENDER_IMPL_STOCK)
    else()
        target_compile_definitions(rayrender PRIVATE RAYRENDER_IMPL_CC)
    endif()
    rayrender_configure_app(rayrender)

    if(NOT RAYRENDER_STOCK_RLSW)
        include(cmake/RaylibStock.cmake)
        rayrender_add_stock_raylib()

        add_executable(rayrender-stock ${RAYRENDER_SOURCES})
        target_link_libraries(rayrender-stock PRIVATE raylib_stock)
        target_compile_definitions(rayrender-stock PRIVATE RAYRENDER_IMPL_STOCK)
        rayrender_configure_app(rayrender-stock)
    endif()
endif()
