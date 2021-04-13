cmake_minimum_required(VERSION 3.0)
project(GLdc)

# set the default backend
if(PLATFORM_DREAMCAST)
    set(BACKEND "kospvr" CACHE STRING "Backend to use")
else()
    set(BACKEND "software" CACHE STRING "Backend to use")
endif()

# List of possible backends
set_property(CACHE BACKEND PROPERTY STRINGS kospvr software)

message("\nCompiling using backend: ${BACKEND}\n")

string(TOUPPER ${BACKEND} BACKEND_UPPER)
add_definitions(-DBACKEND_${BACKEND_UPPER})

set(CMAKE_C_STANDARD 99)

include_directories(include)

if(NOT PLATFORM_DREAMCAST)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

set(
    SOURCES
    containers/aligned_vector.c
    containers/named_array.c
    containers/stack.c
    GL/clip.c
    GL/draw.c
    GL/error.c
    GL/flush.c
    GL/fog.c
    GL/framebuffer.c
    GL/glu.c
    GL/immediate.c
    GL/lighting.c
    GL/matrix.c
    GL/profiler.c
    GL/state.c
    GL/texture.c
    GL/util.c
    GL/yalloc/yalloc.c
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
    find_package(SDL2 REQUIRED)
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

add_library(GLdc STATIC ${SOURCES})
link_libraries(m)

include_directories(include)

link_libraries(GLdc)

function(gen_sample sample)
    set(SAMPLE_SRCS ${ARGN})
    set(GENROMFS "$ENV{KOS_BASE}/utils/genromfs/genromfs")
    set(BIN2O $ENV{KOS_BASE}/utils/bin2o/bin2o)
    set(ROMDISK_IMG "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk.img")
    set(ROMDISK_O "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk.o")
    set(ROMDISK_DIR "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk")

    add_executable(${sample} ${SAMPLE_SRCS})

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
    endif()
endfunction()

gen_sample(blend_test samples/blend_test/main.c)
gen_sample(depth_funcs samples/depth_funcs/main.c)
gen_sample(depth_funcs_alpha_testing samples/depth_funcs_alpha_testing/main.c samples/depth_funcs_alpha_testing/gl_png.c)
gen_sample(depth_funcs_ortho samples/depth_funcs_ortho/main.c)
gen_sample(lerabot01 samples/lerabot01/main.c)
gen_sample(lerabot_blend_test samples/lerabot_blend_test/main.c)
gen_sample(lights samples/lights/main.c)
gen_sample(mipmap samples/mipmap/main.c)
gen_sample(multitexture_arrays samples/multitexture_arrays/main.c samples/multitexture_arrays/pvr-texture.c)
gen_sample(nehe02 samples/nehe02/main.c)
gen_sample(nehe02de samples/nehe02de/main.c)
gen_sample(nehe02va samples/nehe02va/main.c)
gen_sample(nehe03 samples/nehe03/main.c)
gen_sample(nehe04 samples/nehe04/main.c)
gen_sample(nehe05 samples/nehe05/main.c)
gen_sample(nehe06 samples/nehe06/main.c)
gen_sample(nehe06_vq samples/nehe06_vq/main.c)
gen_sample(nehe06_4444twid samples/nehe06_4444twid/main.c)
gen_sample(ortho2d samples/ortho2d/main.c)
gen_sample(paletted samples/paletted/main.c)
gen_sample(paletted_pcx samples/paletted_pcx/main.c)
gen_sample(polygon_offset samples/polygon_offset/main.c)
gen_sample(terrain samples/terrain/main.c)
gen_sample(zclip samples/zclip/main.c)
gen_sample(zclip_triangle samples/zclip_triangle/main.c)
gen_sample(zclip_trianglestrip samples/zclip_trianglestrip/main.c)

if(PLATFORM_DREAMCAST)
    gen_sample(polymark samples/polymark/main.c)
    gen_sample(quadmark samples/quadmark/main.c)
    gen_sample(trimark samples/trimark/main.c)
endif()
