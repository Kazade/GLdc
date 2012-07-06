# KallistiOS ##version##
#
# addons/libgl Makefile
# (c)2001 Dan Potter

TARGET = libgl.a
OBJS =  gldepth.o gldraw.o glkos.o gllight.o glmisc.o
OBJS += gltex.o gltrans.o glvars.o glblend.o glfog.o glmodifier.o glnzclip.o
SUBDIRS =

defaultall: create_kos_link $(OBJS) subdirs linklib

include $(KOS_BASE)/addons/Makefile.prefab

# creates the kos link to the headers
create_kos_link:
	rm -f ../include/GL
	ln -s ../libgl/include ../include/GL
