# Second raylib static lib compiled against rlsw-cc/stock/rlsw.h.
# rlgl.h does #include "external/rlsw.h" relative to itself, so we compile a
# private copy of rcore.c + rlgl.h + stock rlsw. Other raylib .c files do not
# instantiate rlsw (only rcore defines RLGL_IMPLEMENTATION).

function(rayrender_add_stock_raylib)
    set(stock_hdr "${RLSW_CC_STOCK_HEADER}")
    if(NOT stock_hdr)
        set(stock_hdr "${CMAKE_SOURCE_DIR}/rlsw-cc/stock/rlsw.h")
    endif()

    set(stock_dir "${CMAKE_BINARY_DIR}/rlsw-stock-rcore")
    file(MAKE_DIRECTORY "${stock_dir}/external")
    configure_file("${raylib_SOURCE_DIR}/src/rcore.c" "${stock_dir}/rcore.c" COPYONLY)
    configure_file("${raylib_SOURCE_DIR}/src/rlgl.h" "${stock_dir}/rlgl.h" COPYONLY)
    configure_file("${stock_hdr}" "${stock_dir}/external/rlsw.h" COPYONLY)

    add_library(raylib_stock STATIC
        "${stock_dir}/rcore.c"
        "${raylib_SOURCE_DIR}/src/raudio.c"
        "${raylib_SOURCE_DIR}/src/rmodels.c"
        "${raylib_SOURCE_DIR}/src/rshapes.c"
        "${raylib_SOURCE_DIR}/src/rtext.c"
        "${raylib_SOURCE_DIR}/src/rtextures.c"
    )

    add_custom_command(TARGET raylib_stock PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${stock_hdr}"
            "${stock_dir}/external/rlsw.h"
        COMMENT "Refresh stock rlsw overlay"
    )

    set_source_files_properties("${stock_dir}/rcore.c" PROPERTIES
        OBJECT_DEPENDS "${stock_hdr}"
    )

    target_compile_definitions(raylib_stock PRIVATE
        GRAPHICS_API_OPENGL_SOFTWARE
        PLATFORM_DESKTOP_RGFW
    )
    if(RAYRENDER_SIMD)
        target_compile_definitions(raylib_stock PRIVATE RLSW_USE_SIMD_INTRINSICS)
    endif()

    target_include_directories(raylib_stock
        PRIVATE
            "${stock_dir}"
            "${raylib_SOURCE_DIR}/src"
        PUBLIC
            "${raylib_SOURCE_DIR}/src"
    )

    get_target_property(_raylib_public_libs raylib INTERFACE_LINK_LIBRARIES)
    if(_raylib_public_libs)
        target_link_libraries(raylib_stock PUBLIC ${_raylib_public_libs})
    endif()

    get_target_property(_raylib_c_opts raylib COMPILE_OPTIONS)
    if(_raylib_c_opts)
        target_compile_options(raylib_stock PRIVATE ${_raylib_c_opts})
    endif()

    if(NOT MSVC)
        target_compile_options(raylib_stock PRIVATE
            -fno-strict-aliasing
            -Werror=implicit-function-declaration
            -Werror=pointer-arith
        )
    endif()
endfunction()
