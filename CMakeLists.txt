cmake_minimum_required(VERSION 3.0)
project(GLdc)

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
)

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

function(gen_sample sample sources)
    set(GENROMFS "$ENV{KOS_BASE}/utils/genromfs/genromfs")
    set(BIN2O $ENV{KOS_BASE}/utils/bin2o/bin2o)
    set(ROMDISK_IMG "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk.img")
    set(ROMDISK_O "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk.o")
    set(ROMDISK_DIR "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk")

    add_executable(${sample} ${sources})

    if(EXISTS "${CMAKE_SOURCE_DIR}/samples/${sample}/romdisk")
        message("Generating romdisk for sample: ${sample}")
        add_custom_command(
            OUTPUT ${ROMDISK_IMG}
            COMMAND ${GENROMFS} -f ${ROMDISK_IMG} -d ${ROMDISK_DIR} -v
            DEPENDS ${sample}
        )

        add_custom_command(
            OUTPUT ${ROMDISK_O}
            COMMAND ${BIN2O} romdisk.img romdisk romdisk.o
            DEPENDS ${ROMDISK_IMG}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/samples/${sample}"
        )

        add_custom_target(run_${sample} ALL DEPENDS ${ROMDISK_O})
        target_link_libraries(${sample} ${ROMDISK_O})
    else()
        message("No such romdisk for sample: ${sample} at 'samples/${sample}/romdisk'")
    endif()
endfunction()

gen_sample(nehe02 samples/nehe02/main.c)
gen_sample(blend_test samples/blend_test/main.c)
gen_sample(depth_funcs samples/depth_funcs/main.c)
gen_sample(depth_funcs_alpha_testing samples/depth_funcs_alpha_testing/main.c samples/depth_funcs_alpha_testing/gl_png.c)
gen_sample(depth_funcs_ortho samples/depth_funcs_ortho/main.c)
gen_sample(lights samples/lights/main.c)
gen_sample(mipmap samples/mipmap/main.c)
gen_sample(multitexture_arrays samples/multitexture_arrays/main.c samples/multitexture_arrays/pvr-texture.c)
gen_sample(nehe02de samples/nehe02de/main.c)
gen_sample(nehe06 samples/nehe06/main.c)
gen_sample(paletted samples/paletted/main.c)
gen_sample(paletted_pcx samples/paletted_pcx/main.c)
gen_sample(polygon_offset samples/polygon_offset/main.c)
gen_sample(zclip samples/zclip/main.c)
gen_sample(zclip_triangle samples/zclip_triangle/main.c)
gen_sample(zclip_trianglestrip samples/zclip_trianglestrip/main.c)

if(PLATFORM_DREAMCAST)
    add_executable(polymark samples/polymark/main.c)
endif()
