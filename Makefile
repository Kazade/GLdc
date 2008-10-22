# KallistiOS ##version##
#
# addons/libgl Makefile
# (c)2001 Dan Potter

TARGET = libgl.a
OBJS =  gldepth.o gldraw.o glkos.o gllight.o glmisc.o
OBJS += gltex.o gltrans.o glvars.o glblend.o glfog.o glmodifier.o glnzclip.o
SUBDIRS =

include $(KOS_BASE)/addons/Makefile.prefab


