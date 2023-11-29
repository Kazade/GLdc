cmake_minimum_required(VERSION 3.9)
project(GLdc)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set the default backend
if(PLATFORM_DREAMCAST)
    set(BACKEND "kospvr" CACHE STRING "Backend to use")
else()
    set(BACKEND "software" CACHE STRING "Backend to use")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT FLTO_SUPPORTED OUTPUT FLTO_ERROR)

# List of possible backends
set_property(CACHE BACKEND PROPERTY STRINGS kospvr software)

message("\nCompiling using backend: ${BACKEND}\n")

string(TOUPPER ${BACKEND} BACKEND_UPPER)
add_definitions(-DBACKEND_${BACKEND_UPPER})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

include_directories(include)

if(NOT PLATFORM_DREAMCAST)
    set(FIND_LIBRARY_USE_LIB32_PATHS true)
    set(FIND_LIBRARY_USE_LIB64_PATHS false)
else()
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mfsrra" COMPILER_HAS_FSRRA)
    check_c_compiler_flag("-mfsca"  COMPILER_HAS_FSCA)
    if(COMPILER_HAS_FSRRA)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfsrra")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfsrra")

        set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -mfsrra")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -mfsrra")
    endif()
    if(COMPILER_HAS_FSCA)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfsca")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfsca")

        set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -mfsca")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -mfsca")
    endif()
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffp-contract=fast -ffast-math")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")

    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ffp-contract=fast -ffast-math")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ffast-math")
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fexpensive-optimizations -fomit-frame-pointer -finline-functions")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -O3 -g0 -s -fomit-frame-pointer -fstrict-aliasing")

set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3 -fexpensive-optimizations -fomit-frame-pointer -finline-functions")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++14 -O3 -fomit-frame-pointer -fstrict-aliasing")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -Wall -Wextra")

set(
    SOURCES
    containers/aligned_vector.c
    containers/named_array.c
    containers/stack.c
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
    GL/util.c
    GL/alloc/alloc.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)

execute_process(
    COMMAND git describe --abbrev=4 --dirty --always --tags
    OUTPUT_VARIABLE GLDC_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(GL/version.c.in ${CMAKE_CURRENT_BINARY_DIR}/version.c)

if(PLATFORM_DREAMCAST)
    set(SOURCES ${SOURCES} GL/platforms/sh4.c)
else()
    find_package(PkgConfig)
    pkg_check_modules(SDL2 REQUIRED sdl2)

    include_directories(${SDL2_INCLUDE_DIRS})
    link_libraries(${SDL2_LIBRARIES})
    set(
        SOURCES
        ${SOURCES}
        GL/platforms/software.c
        GL/platforms/software/edge_equation.c
        GL/platforms/software/parameter_equation.c
    )
endif()

add_library(GL STATIC ${SOURCES})

if(FLTO_SUPPORTED)
    set_property(TARGET GL PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(NOT PLATFORM_DREAMCAST)
set_target_properties(GL PROPERTIES
    COMPILE_OPTIONS "-m32"
    LINK_OPTIONS "-m32"
)
endif()

link_libraries(m)

include_directories(include)

link_libraries(GL)

function(gen_sample sample)
    set(SAMPLE_SRCS ${ARGN})
    set(GENROMFS "$ENV{KOS_BASE}/utils/genromfs/genromfs")
    set(BIN2O $ENV{KOS_BASE}/utils/bin2o/bin2o)
    set(ROMDISK_IMG "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk.img")
    set(ROMDISK_O "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk.o")
    set(ROMDISK_DIR "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk")

    add_executable(${sample} ${SAMPLE_SRCS})

    if(FLTO_SUPPORTED)
        # FIXME: Cubes + LTO causes an ICE
        if(NOT ${sample} MATCHES "cubes")
            set_property(TARGET ${sample} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()

    if(PLATFORM_DREAMCAST)
        if(EXISTS "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk")
            message("Generating romdisk for sample: ${sample}")
            add_custom_command(
                OUTPUT ${ROMDISK_IMG}
                COMMAND ${GENROMFS} -f ${ROMDISK_IMG} -d ${ROMDISK_DIR} -v
            )

            add_custom_command(
                OUTPUT ${ROMDISK_O}
                COMMAND ${BIN2O} romdisk.img romdisk romdisk.o
                DEPENDS ${ROMDISK_IMG}
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/samples/${sample}"
            )

            add_custom_target(${sample}_romdisk DEPENDS ${ROMDISK_O})
            add_dependencies(${sample} ${sample}_romdisk)
            target_link_libraries(${sample} ${ROMDISK_O})

        else()
            message("No such romdisk for sample: ${sample} at 'samples/${sample}/romdisk'")
        endif()
    else()
        set_target_properties(${sample} PROPERTIES
            COMPILE_OPTIONS "-m32"
            LINK_OPTIONS "-m32"
        )
    endif()
endfunction()

add_subdirectory(tests)

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
gen_sample(nehe06_vq samples/nehe06_vq/main.c)
gen_sample(nehe06_4444twid samples/nehe06_4444twid/main.c)
gen_sample(nehe08 samples/nehe08/main.c samples/nehe08/pvr-texture.c)
gen_sample(nehe10 samples/nehe10/main.c samples/loadbmp.c)
gen_sample(nehe20 samples/nehe20/main.c samples/loadbmp.c)
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

if(PLATFORM_DREAMCAST)
    gen_sample(trimark samples/trimark/main.c)
    gen_sample(quadmark samples/quadmark/main.c samples/profiler.c)
    gen_sample(prof_texture_upload samples/prof_texture_upload/main.c samples/profiler.c)
else()
    gen_sample(quadmark samples/quadmark/main.c)
    gen_sample(prof_texture_upload samples/prof_texture_upload/main.c)
endif()
