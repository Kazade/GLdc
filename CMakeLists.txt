cmake_minimum_required(VERSION 3.0)
project(GLdc)

set(CMAKE_C_STANDARD 99)

include_directories(include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")

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
set(SOURCES ${SOURCES} GL/platforms/software.c GL/platforms/software/edge_equation.c GL/platforms/software/parameter_equation.c)
endif()

add_library(GLdc ${SOURCES})
link_libraries(m)

include_directories(include)

link_libraries(GLdc)

add_executable(nehe02 samples/nehe02/main.c)
add_executable(blend_test samples/blend_test/main.c)
add_executable(depth_funcs samples/depth_funcs/main.c)
add_executable(depth_funcs_alpha_testing samples/depth_funcs_alpha_testing/main.c samples/depth_funcs_alpha_testing/gl_png.c)
add_executable(depth_funcs_ortho samples/depth_funcs_ortho/main.c)
add_executable(lights samples/lights/main.c)
add_executable(mipmap samples/mipmap/main.c)

