cmake_minimum_required(VERSION 3.17)
project(GLdc)

set(CMAKE_VERBOSE_MAKEFILE ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel
    )
endif()

# Options to control optional targets
option(BUILD_SAMPLES "Build example/sample programs" ON)
option(BUILD_TESTS   "Build test executables" ON)

# set the default backend
if(PLATFORM_DREAMCAST)
    set(BACKEND "kospvr" CACHE STRING "Backend to use")
else()
    set(BACKEND "software" CACHE STRING "Backend to use")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT FLTO_SUPPORTED OUTPUT FLTO_ERROR)

if(PLATFORM_DREAMCAST)
    include(kallistios)
endif()

# List of possible backends
set_property(CACHE BACKEND PROPERTY STRINGS kospvr software)

message("\nCompiling using backend: ${BACKEND}\n")

string(TOUPPER ${BACKEND} BACKEND_UPPER)

add_library(GL STATIC
    containers/aligned_vector.c
    containers/named_array.c
    containers/stack.c
    GL/attributes.c
    GL/draw.c
    GL/error.c
    GL/flush.c
    GL/fog.c
    GL/framebuffer.c
    GL/glu.c
    GL/immediate.c
    GL/lighting.c
    GL/matrix.c
    GL/state.c
    GL/texture.c
    GL/tnl_effects.c
    GL/util.c
    GL/alloc/alloc.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)

set_target_properties(GL PROPERTIES
    COMPILE_DEFINITIONS -DBACKEND_${BACKEND_UPPER}
    C_STANDARD 99
    INTERPROCEDURAL_OPTIMIZATION ${FLTO_SUPPORTED}
    PUBLIC_HEADER "include/GL/gl.h;include/GL/glext.h;include/GL/glu.h;include/GL/glkos.h"
)

target_include_directories(GL PUBLIC ${CMAKE_SOURCE_DIR}/include)

if(NOT PLATFORM_DREAMCAST)
    target_compile_options(GL PUBLIC -m32)
    target_link_options(GL PUBLIC -m32)

    set(FIND_LIBRARY_USE_LIB32_PATHS true)
    set(FIND_LIBRARY_USE_LIB64_PATHS false)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL Debug)
    target_compile_options(GL PRIVATE -Wall -Wextra)
elseif("${CMAKE_BUILD_TYPE}" MATCHES Release|RelWithDebInfo)
    target_compile_options(GL PRIVATE -ffast-math)
endif()

execute_process(
    COMMAND git describe --abbrev=4 --dirty --always --tags
    OUTPUT_VARIABLE GLDC_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(GL/version.c.in ${CMAKE_CURRENT_BINARY_DIR}/version.c)

if(PLATFORM_DREAMCAST)
    target_sources(GL PRIVATE GL/platforms/sh4.c)
else()
    find_package(PkgConfig)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_link_libraries(GL PUBLIC ${SDL2_LIBRARIES})
    target_include_directories(GL PUBLIC ${SDL2_INCLUDE_DIRS})

    target_sources(GL PRIVATE
        GL/platforms/software.c
        GL/platforms/software/edge_equation.c
        GL/platforms/software/parameter_equation.c
    )
endif()

include(GNUInstallDirs)

install(TARGETS GL
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/GL
)

function(gen_sample sample)
    add_executable(${sample} ${ARGN})
    target_link_libraries(${sample} PRIVATE m GL)
    set_target_properties(${sample} PROPERTIES
        C_STANDARD 99
        CXX_STANDARD 14
    )

    if(FLTO_SUPPORTED)
        # FIXME: Cubes + LTO causes an ICE
        if(NOT ${sample} MATCHES "cubes")
            set_property(TARGET ${sample} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()

    if(PLATFORM_DREAMCAST)
        if(EXISTS "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk")
            message("Generating romdisk for sample: ${sample}")
            kos_add_romdisk(${sample} ${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk ${sample})
        else()
            message("No such romdisk for sample: ${sample} at 'samples/${sample}/romdisk'")
        endif()
    endif()
endfunction()

# --- Tests ---
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --- Samples ---
if(BUILD_SAMPLES)
    gen_sample(blend_test samples/blend_test/main.c)
    gen_sample(depth_funcs samples/depth_funcs/main.c)
    gen_sample(depth_funcs_alpha_testing samples/depth_funcs_alpha_testing/main.c samples/depth_funcs_alpha_testing/gl_png.c)
    gen_sample(depth_funcs_ortho samples/depth_funcs_ortho/main.c)
    gen_sample(lerabot01 samples/lerabot01/main.c samples/loadbmp.c)
    gen_sample(lights samples/lights/main.c samples/loadbmp.c)
    gen_sample(mipmap samples/mipmap/main.c samples/loadbmp.c)
    gen_sample(multitexture_arrays samples/multitexture_arrays/main.c samples/multitexture_arrays/pvr-texture.c)
    gen_sample(nehe02 samples/nehe02/main.c)
    gen_sample(nehe02de samples/nehe02de/main.c)
    gen_sample(nehe02va samples/nehe02va/main.c)
    gen_sample(nehe03 samples/nehe03/main.c)
    gen_sample(nehe04 samples/nehe04/main.c)
    gen_sample(nehe05 samples/nehe05/main.c)
    gen_sample(nehe06 samples/nehe06/main.c samples/loadbmp.c)
    gen_sample(nehe06_strided samples/nehe06_strided/main.c samples/loadbmp.c)
    gen_sample(nehe06_vq samples/nehe06_vq/main.c)
    gen_sample(nehe06_dt samples/nehe06_dt/main.c)
    gen_sample(nehe06_4444twid samples/nehe06_4444twid/main.c)
    gen_sample(nehe08 samples/nehe08/main.c samples/nehe08/pvr-texture.c)
    gen_sample(nehe10 samples/nehe10/main.c samples/loadbmp.c)
    gen_sample(nehe16 samples/nehe16/main.c samples/nehe16/pvr-texture.c)
    gen_sample(nehe20 samples/nehe20/main.c samples/loadbmp.c)
    gen_sample(nehe20_strided samples/nehe20_strided/main.c samples/loadbmp.c)
    gen_sample(ortho2d samples/ortho2d/main.c)
    gen_sample(paletted samples/paletted/main.c)
    gen_sample(paletted_pcx samples/paletted_pcx/main.c)
    gen_sample(polygon_offset samples/polygon_offset/main.c)
    gen_sample(terrain samples/terrain/main.c)
    gen_sample(zclip samples/zclip/main.c)
    gen_sample(zclip_triangle samples/zclip_triangle/main.c)
    gen_sample(zclip_trianglestrip samples/zclip_trianglestrip/main.c)
    gen_sample(scissor samples/scissor/main.c)
    gen_sample(polymark samples/polymark/main.c)
    gen_sample(cubes samples/cubes/main.cpp)
    gen_sample(zclip_test tests/zclip/main.cpp)
    gen_sample(primitive_modes samples/primitive_modes/main.c)
    gen_sample(tnl_effects samples/tnl_effects/main.c)

    if(PLATFORM_DREAMCAST)
        gen_sample(trimark samples/trimark/main.c)
        gen_sample(quadmark samples/quadmark/main.c samples/profiler.c)
        gen_sample(prof_texture_upload samples/prof_texture_upload/main.c samples/profiler.c)
    else()
        gen_sample(quadmark samples/quadmark/main.c)
        gen_sample(prof_texture_upload samples/prof_texture_upload/main.c)
    endif()
endif()
