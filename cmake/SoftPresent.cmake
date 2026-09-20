# Cross-platform software present overlay:
#   macOS  — RGBA + CGImage (cmake/overlays/macos_sw_present.c)
#   else   — BGRA-aligned RGFW blit (zero-copy when formats match)
# Idempotent via cmake/overlays/apply_sw_present.py

function(rayrender_apply_sw_present_overlay raylib_target raylib_src_dir)
    if(RAYRENDER_GPU)
        return()
    endif()

    set(_apply "${CMAKE_SOURCE_DIR}/cmake/overlays/apply_sw_present.py")
    set(_present_c "${CMAKE_SOURCE_DIR}/cmake/overlays/macos_sw_present.c")
    set(_platform_c "${raylib_src_dir}/src/platforms/rcore_desktop_rgfw.c")
    set(_rgfw_h "${raylib_src_dir}/src/external/RGFW/RGFW.h")
    set(_stamp "${CMAKE_BINARY_DIR}/sw_present_overlay.stamp")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${_apply}" "${raylib_src_dir}"
        RESULT_VARIABLE _overlay_rc
    )
    if(NOT _overlay_rc EQUAL 0)
        message(FATAL_ERROR "rayrender: software present overlay failed (${_overlay_rc})")
    endif()

    add_custom_command(
        OUTPUT "${_stamp}"
        COMMAND "${Python3_EXECUTABLE}" "${_apply}" "${raylib_src_dir}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
        DEPENDS "${_apply}" "${_platform_c}" "${_rgfw_h}" "${_present_c}"
        COMMENT "rayrender: apply software present overlay"
        VERBATIM
    )
    add_custom_target(rayrender_sw_present_overlay DEPENDS "${_stamp}")
    add_dependencies(${raylib_target} rayrender_sw_present_overlay)

    set_source_files_properties("${raylib_src_dir}/src/rcore.c" PROPERTIES
        OBJECT_DEPENDS "${_platform_c};${_rgfw_h};${_stamp}"
    )

    if(APPLE)
        target_sources(${raylib_target} PRIVATE "${_present_c}")
        target_compile_definitions(${raylib_target} PRIVATE SW_FRAMEBUFFER_OUTPUT_BGRA=0)
        target_link_libraries(${raylib_target} PRIVATE "-framework CoreGraphics")
        set_source_files_properties("${raylib_src_dir}/src/rcore.c" PROPERTIES
            OBJECT_DEPENDS "${_platform_c};${_rgfw_h};${_present_c};${_stamp}"
        )
    endif()
endfunction()

function(rayrender_sw_present_attach_stock)
    if(RAYRENDER_GPU OR NOT TARGET raylib_stock)
        return()
    endif()
    if(APPLE)
        set(_present_c "${CMAKE_SOURCE_DIR}/cmake/overlays/macos_sw_present.c")
        target_sources(raylib_stock PRIVATE "${_present_c}")
        target_compile_definitions(raylib_stock PRIVATE SW_FRAMEBUFFER_OUTPUT_BGRA=0)
        target_link_libraries(raylib_stock PRIVATE "-framework CoreGraphics")
    endif()
    if(TARGET rayrender_sw_present_overlay)
        add_dependencies(raylib_stock rayrender_sw_present_overlay)
    endif()
endfunction()

# Back-compat aliases for older CMakeLists call sites
function(rayrender_apply_macos_present_overlay raylib_target raylib_src_dir)
    rayrender_apply_sw_present_overlay(${raylib_target} "${raylib_src_dir}")
endfunction()
function(rayrender_macos_present_attach_stock)
    rayrender_sw_present_attach_stock()
endfunction()
