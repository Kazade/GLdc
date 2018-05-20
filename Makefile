# KallistiOS ##version##
#
# kos-ports/libgl Makefile
# Copyright (C) 2013, 2014 Josh Pearson
# Copyright (C) 2014 Lawrence Sebald
# Copyright (C) 2018 Luke Benstead

TARGET = libGL.a
OBJS = GL/draw.o GL/flush.o GL/framebuffer.o GL/immediate.o GL/lighting.o GL/state.o GL/texture.o
OBJS += GL/matrix.o GL/fog.o GL/error.o

SUBDIRS =

KOS_CFLAGS += -ffast-math -O3 -Iinclude

link:
	$(KOS_AR) rcs $(TARGET) $(OBJS)

build: $(OBJS) link


defaultall: build subdirs linklib create_kos_link

include $(KOS_BASE)/addons/Makefile.prefab

# creates the kos link to the headers
create_kos_link:
	rm -f ../include/GL
	ln -s ../libgl/include ../include/GL
