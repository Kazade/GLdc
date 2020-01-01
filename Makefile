# KallistiOS ##version##
#
# kos-ports/libgl Makefile
# Copyright (C) 2013, 2014 Josh Pearson
# Copyright (C) 2014 Lawrence Sebald
# Copyright (C) 2018 Luke Benstead

TARGET = libGLdc.a
OBJS = GL/draw.o GL/flush.o GL/framebuffer.o GL/immediate.o GL/lighting.o GL/state.o GL/texture.o GL/glu.o GL/version.h
OBJS += GL/matrix.o GL/fog.o GL/error.o GL/clip.o containers/stack.o containers/named_array.o containers/aligned_vector.o GL/profiler.o

SUBDIRS =

KOS_CFLAGS += -ffast-math -Ofast -Iinclude

GL/version.h:
	@echo -e '#pragma once\n#define GLDC_VERSION "$(shell git describe --abbrev=4 --dirty --always --tags)"\n' > $@

link:
	$(KOS_AR) rcs $(TARGET) $(OBJS)

build: GL/version.h $(OBJS) link


samples: build
	$(KOS_MAKE) -C samples all

defaultall: GL/version.h create_kos_link $(OBJS) subdirs linklib samples

include $(KOS_BASE)/addons/Makefile.prefab

# creates the kos link to the headers
create_kos_link:
	rm -f ../include/GL
	ln -s ../GLdc/include ../include/GL
