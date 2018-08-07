
# Manipulate the CFLAGS to look our *our* version of the library and includes
INC_DIR = $(abspath ../include)
LIB_DIR = $(abspath ../)
export CFLAGS := $(CFLAGS) -I $(INC_DIR)
export OBJEXTRA := $(LIB_DIR)/libGLdc.a


all:
	$(KOS_MAKE) -C nehe02 all
	$(KOS_MAKE) -C nehe02va all
	$(KOS_MAKE) -C nehe03 all
	$(KOS_MAKE) -C nehe06 all
	$(KOS_MAKE) -C nehe06_vq all
	$(KOS_MAKE) -C ortho2d all
	$(KOS_MAKE) -C lerabot01 all
	$(KOS_MAKE) -C zclip all
	$(KOS_MAKE) -C zclip_triangle all
	$(KOS_MAKE) -C zclip_trianglestrip all
	$(KOS_MAKE) -C terrain all
	$(KOS_MAKE) -C quadmark all
