# KallistiOS ##version##
#
# kos-ports/libgl Makefile
# Copyright (C) 2013, 2014 Josh Pearson
# Copyright (C) 2014 Lawrence Sebald

TARGET = libgl.a
OBJS =  gl-rgb.o gl-fog.o gl-sh4-light.o gl-light.o gl-clip.o gl-pvr.o
OBJS += gl-arrays.o gl-matrix.o gl-api.o
SUBDIRS =

KOS_CFLAGS += -ffast-math -O3 -Iinclude

defaultall: create_kos_link $(OBJS) subdirs linklib

include $(KOS_BASE)/addons/Makefile.prefab

# creates the kos link to the headers
create_kos_link:
	rm -f ../include/GL
	ln -s ../libgl/include ../include/GL
