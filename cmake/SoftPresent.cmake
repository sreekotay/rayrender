# Soft present now lives in rlsw-cc (rlsw_cc_overlay_raylib / rlsw_cc_apply_soft_present).
# Thin wrappers keep older rayrender call sites working.

function(rayrender_apply_sw_present_overlay raylib_target raylib_src_dir)
    if(COMMAND rlsw_cc_apply_soft_present)
        rlsw_cc_apply_soft_present(${raylib_target} "${raylib_src_dir}")
    else()
        message(FATAL_ERROR "rayrender: rlsw-cc soft present unavailable — add_subdirectory/FetchContent rlsw-cc first")
    endif()
endfunction()

function(rayrender_sw_present_attach_stock)
    if(COMMAND rlsw_cc_soft_present_attach)
        rlsw_cc_soft_present_attach(raylib_stock)
    endif()
endfunction()

function(rayrender_apply_macos_present_overlay raylib_target raylib_src_dir)
    rayrender_apply_sw_present_overlay(${raylib_target} "${raylib_src_dir}")
endfunction()

function(rayrender_macos_present_attach_stock)
    rayrender_sw_present_attach_stock()
endfunction()
