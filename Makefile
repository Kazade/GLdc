# KallistiOS ##version##
#
# kos-ports/libgl Makefile
# Copyright (C) 2013, 2014 Josh Pearson
# Copyright (C) 2014 Lawrence Sebald

TARGET = libGL.a
OBJS =  gl-rgb.o gl-fog.o gl-sh4-light.o gl-light.o gl-clip.o gl-clip-arrays.o
OBJS += gl-arrays.o gl-pvr.o gl-matrix.o gl-api.o glu-texture.o
OBJS += gl-framebuffer.o gl-cap.o gl-error.o
OBJS += GL/draw.o GL/flush.o GL/framebuffer.o GL/immediate.o GL/lighting.o GL/state.o GL/texture.o

SUBDIRS =

KOS_CFLAGS += -ffast-math -O3 -Iinclude

link:
	$(KOS_AR) rcs $(TARGET) $(OBJS)

build: $(OBJS) link


defaultall: create_kos_link build subdirs linklib

include $(KOS_BASE)/addons/Makefile.prefab

# creates the kos link to the headers
create_kos_link:
	rm -f ../include/GL
	ln -s ../libgl/include ../include/GL
