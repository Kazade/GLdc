cmake_minimum_required(VERSION 3.0)
project(GLdc)

set(CMAKE_C_STANDARD 99)

include_directories(include)

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
set(SOURCES ${SOURCES} GL/platforms/x86.c)
endif()

add_library(GLdc ${SOURCES})
link_libraries(m)

include_directories(include)

add_executable(nehe02 samples/nehe02/main.c)
target_link_libraries(nehe02 GLdc)
