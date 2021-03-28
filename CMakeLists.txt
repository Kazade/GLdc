cmake_minimum_required(VERSION 3.0)
project(GLdc)

set(
    SOURCES
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
set(SOURCES ${SOURCES} GL/platforms/x86.c)
endif()

add_library(GLdc ${SOURCES})

